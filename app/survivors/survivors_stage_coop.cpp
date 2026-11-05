#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "ro/core/error.h"
#include "ro/sim/survivors_geometry.h"

#include "survivors/survivors_stage.h"

// L'ÉTAGE D'UNE PARTIE COOP. La partie qui se dessine (`play_`) montre le présent
// du lockstep, héros local en scène : tout ce qui lit l'étage lit donc le héros de
// ce joueur, sans savoir qu'il y en a d'autres.
//
// Rien ne fige le monde depuis ce client : la pause n'est qu'un menu, et le héros y
// reste à l'arrêt. Seul un vote ou un coffre, qui sont des phases de la partie, le
// fige, pour tous.

namespace moonclient {

using ro::sim::SurvivorsOutcome;
using ro::sim::SurvivorsPhase;

ro::core::Result<void> SurvivorsStage::BeginCoop(const ro::sim::SurvivorsRules& rules,
                                                 std::optional<ro::sim::WalkMap> arena,
                                                 const ro::sim::SurvivorsStart& start,
                                                 std::size_t local_player,
                                                 std::uint32_t input_delay_ticks) {
  SurvivorsRunBoard board;
  board.coop = true;
  if (ro::core::Result<void> begun = Begin(rules, arena, start, board); !begun) return begun;
  auto run = arena.has_value() ? ro::sim::SurvivorsRun::Start(rules, std::move(*arena), start)
                               : ro::sim::SurvivorsRun::Start(rules, start);
  if (!run) return std::unexpected(run.error());
  match_.emplace(std::move(*run), local_player, input_delay_ticks);
  coop_pending_ms_ = 0;
  coop_last_tick_ = 0;
  coop_stalled_ms_ = 0;
  coop_offers_.clear();
  coop_ballot_.reset();
  play_->Show(match_->present(), 0);
  return {};
}

void SurvivorsStage::CoopReceive(std::size_t player, std::span<const std::uint8_t> payload) {
  if (match_.has_value()) (void)match_->Receive(player, payload);
}

void SurvivorsStage::CoopHear(std::size_t player, std::span<const std::uint8_t> payload) {
  if (match_.has_value()) match_->Hear(player, payload);
}

void SurvivorsStage::CoopLeave(std::size_t player) {
  if (match_.has_value()) match_->Leave(player);
}

bool SurvivorsStage::coop_over() const noexcept {
  return match_.has_value() && match_->confirmed().field().phase == SurvivorsPhase::kOver;
}

void SurvivorsStage::AbandonCoop() {
  if (!match_.has_value()) return;
  play_->Abandon();
  ending_ = EndingOf(play_->run().field(), board_);
  match_.reset();
  Enter(SurvivorsScreen::kSummary);
}

std::optional<std::vector<std::uint8_t>> SurvivorsStage::TakeCoopFrame() {
  if (!match_.has_value()) return std::nullopt;
  return match_->TakeFrame();
}

void SurvivorsStage::AdvanceCoop(std::uint32_t elapsed_ms, const SurvivorsIntent& intent) {
  // L'ARRIVÉE : aucune entrée ne part avant la fin de la descente, et les autres
  // attendent : personne ne joue avant que tous soient là.
  if (screen_ == SurvivorsScreen::kArriving) {
    arrival_ms_ = static_cast<std::uint32_t>(
        std::min<std::uint64_t>(std::uint64_t{arrival_ms_} + elapsed_ms, kArrivalMs));
    if (arrival_ms_ >= kArrivalMs) Enter(SurvivorsScreen::kPlaying);
    return;
  }
  if (screen_ == SurvivorsScreen::kPlaying && intent.escape) {
    pause_cause_ = PauseCause::kPlayer;
    Enter(SurvivorsScreen::kPaused);
  } else if (screen_ == SurvivorsScreen::kPaused) {
    if (intent.abandon) {
      AbandonCoop();
      return;
    }
    if ((intent.escape || intent.resume) && guard_.Accepts(clock_ms_)) {
      Enter(SurvivorsScreen::kPlaying);
    }
  }
  if (screen_ == SurvivorsScreen::kChoosing && intent.answer.has_value() &&
      guard_.Accepts(clock_ms_)) {
    coop_ballot_ = intent.answer;
  }
  // Une entrée par tic ; sans avance permise, le temps attend les autres.
  coop_pending_ms_ += elapsed_ms;
  const std::uint8_t heading =
      screen_ == SurvivorsScreen::kPlaying ? intent.heading : ro::sim::kNoHeading;
  while (coop_pending_ms_ >= ro::sim::kSurvivorsStepMs) {
    ro::sim::SurvivorsTeamInput input{.heading = heading};
    input.ballot = coop_ballot_;
    if (!match_->PushLocal(input)) {
      coop_pending_ms_ = 0;
      break;
    }
    coop_ballot_.reset();
    coop_pending_ms_ -= ro::sim::kSurvivorsStepMs;
  }
  match_->Advance();
  const std::uint32_t confirmed_tick = match_->confirmed().field().ticks;
  coop_stalled_ms_ = confirmed_tick == coop_last_tick_ ? coop_stalled_ms_ + elapsed_ms : 0;
  coop_last_tick_ = confirmed_tick;
  news_ = match_->TakeNews();
  chests_.insert(chests_.end(), news_.chests.begin(), news_.chests.end());
  play_->Show(match_->present(), coop_pending_ms_);
  NoteSightings(sightings_, play_->run().field(), news_, presentation_.presentation_ms());
  presentation_.Follow(play_->run().field().step, play_->blend_per_mille());
  FollowCoopRun();
}

void SurvivorsStage::FollowCoopRun() {
  // L'ÉCRAN SUIT LA PARTIE CONFIRMÉE : un vote s'ouvre et se ferme au même tic chez
  // tous, ce que le présent ne garantit pas.
  const ro::sim::SurvivorsField& field = match_->confirmed().field();
  if (field.phase == SurvivorsPhase::kOver) {
    ending_ = EndingOf(field, board_);
    Enter(field.tally.outcome == SurvivorsOutcome::kFell ? SurvivorsScreen::kDying
                                                         : SurvivorsScreen::kWon);
    return;
  }
  if (field.phase == SurvivorsPhase::kChoosing) {
    // UN VOTE QUI EN SUIT UN AUTRE, ou une relance, offre autre chose sans quitter la
    // phase : il repasse par l'entrée, pour qu'un clic parti trop tôt ne vote pas pour
    // une offre qu'on n'a pas lue.
    if (screen_ != SurvivorsScreen::kChoosing || coop_offers_ != field.offers) {
      coop_offers_ = field.offers;
      Enter(SurvivorsScreen::kChoosing);
    }
    return;
  }
  if (field.hold_ticks_left > 0) {
    if (screen_ != SurvivorsScreen::kChest) {
      chest_list_shown_ = false;
      Enter(SurvivorsScreen::kChest);
    } else if (!chest_list_shown_ && screen_elapsed_ms() >= kCoopChestRouletteMs) {
      chest_list_shown_ = true;
    }
    return;
  }
  if (screen_ == SurvivorsScreen::kChoosing || screen_ == SurvivorsScreen::kChest) {
    chests_.clear();
    chest_list_shown_ = false;
    Enter(SurvivorsScreen::kPlaying);
  }
}

}  // namespace moonclient
