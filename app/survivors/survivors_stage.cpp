#include "survivors/survivors_stage.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <utility>

#include "ro/core/error.h"
#include "ro/sim/survivors_journal.h"

namespace moonclient {
namespace {

using ro::sim::SurvivorsOutcome;
using ro::sim::SurvivorsPhase;

[[nodiscard]] PauseCause CauseOf(const SurvivorsIntent& intent) noexcept {
  if (intent.world_alert) return PauseCause::kWorldAlert;
  if (intent.focus_lost) return PauseCause::kFocusLost;
  return PauseCause::kPlayer;
}

[[nodiscard]] bool AsksPause(const SurvivorsIntent& intent) noexcept {
  return intent.escape || intent.focus_lost || intent.world_alert;
}

}  // namespace

SurvivorsEnding EndingOf(const ro::sim::SurvivorsField& field, const SurvivorsRunBoard& board) {
  return SurvivorsEnding{.class_id = field.start.class_id,
                         .tally = field.tally,
                         .recap = field.recap,
                         .cards = field.hero.cards,
                         .zeny = field.hero.zeny,
                         .board = board};
}

void SurvivorsStage::Open() {
  if (screen_ == SurvivorsScreen::kClosed) Enter(SurvivorsScreen::kPreparing);
}

ro::core::Result<void> SurvivorsStage::Begin(const ro::sim::SurvivorsRules& rules,
                                             ro::sim::WalkMap arena,
                                             const ro::sim::SurvivorsStart& start,
                                             const SurvivorsRunBoard& board) {
  if (screen_ != SurvivorsScreen::kPreparing && screen_ != SurvivorsScreen::kLoading &&
      screen_ != SurvivorsScreen::kSummary) {
    return ro::core::Fail(ro::core::ErrorCode::kInvalidArgument,
                          "une partie ne commence que depuis la préparation, le chargement ou le "
                          "bilan");
  }
  auto run = ro::sim::SurvivorsRun::Start(rules, std::move(arena), start);
  if (!run) return std::unexpected(run.error());
  play_.emplace(std::move(*run));
  board_ = board;
  ending_.reset();
  news_ = {};
  chests_.clear();
  presentation_.Reset();
  chest_list_shown_ = false;
  pause_cause_ = PauseCause::kPlayer;
  held_pause_.reset();
  arrival_ms_ = 0;
  Enter(SurvivorsScreen::kLoading);
  return {};
}

void SurvivorsStage::Loaded() {
  if (screen_ == SurvivorsScreen::kLoading) Enter(SurvivorsScreen::kArriving);
}

void SurvivorsStage::Advance(std::uint32_t elapsed_ms, const SurvivorsIntent& intent) {
  entered_.reset();
  news_ = {};
  clock_ms_ += elapsed_ms;
  switch (screen_) {
    case SurvivorsScreen::kClosed:
    case SurvivorsScreen::kPreparing:
    case SurvivorsScreen::kLoading:
    case SurvivorsScreen::kSummary:
      return;
    case SurvivorsScreen::kArriving:
      AdvanceArriving(elapsed_ms, intent);
      return;
    case SurvivorsScreen::kPlaying:
      AdvancePlaying(elapsed_ms, intent);
      return;
    case SurvivorsScreen::kPaused:
      AdvancePaused(intent);
      return;
    case SurvivorsScreen::kChoosing:
      AdvanceChoosing(intent);
      return;
    case SurvivorsScreen::kChest:
      AdvanceChest(intent);
      return;
    case SurvivorsScreen::kDying:
      AdvanceEnd(elapsed_ms, intent, kDeathVeilMs);
      return;
    case SurvivorsScreen::kWon:
      AdvanceEnd(elapsed_ms, intent, kVictoryBannerMs);
      return;
  }
}

bool SurvivorsStage::Freeze() {
  if (!play_) return false;
  if (screen_ == SurvivorsScreen::kDying || screen_ == SurvivorsScreen::kWon) {
    Enter(SurvivorsScreen::kSummary);
    return true;
  }
  if (play_->run().field().phase == SurvivorsPhase::kOver) return false;
  play_->Abandon();
  TakeNews();
  ending_ = EndingOf(play_->run().field(), board_);
  Enter(SurvivorsScreen::kSummary);
  return true;
}

void SurvivorsStage::ShowEnding(SurvivorsEnding ending) {
  play_.reset();
  board_ = ending.board;
  ending_ = std::move(ending);
  Enter(SurvivorsScreen::kSummary);
}

void SurvivorsStage::Close() { *this = SurvivorsStage(); }

bool SurvivorsStage::owns_screen() const noexcept {
  return screen_ != SurvivorsScreen::kClosed && screen_ != SurvivorsScreen::kPreparing;
}

const ro::sim::SurvivorsPlay* SurvivorsStage::play() const noexcept {
  return play_ ? &*play_ : nullptr;
}

const SurvivorsEnding* SurvivorsStage::ending() const noexcept {
  return ending_ ? &*ending_ : nullptr;
}

void SurvivorsStage::Enter(SurvivorsScreen screen) {
  screen_ = screen;
  entered_ = screen;
  entered_at_ms_ = clock_ms_;
  guard_.Arm(clock_ms_);
}

void SurvivorsStage::AdvancePlaying(std::uint32_t elapsed_ms, const SurvivorsIntent& intent) {
  if (AsksPause(intent)) {
    PauseFor(CauseOf(intent));
    return;
  }
  play_->Advance(elapsed_ms, ro::sim::SurvivorsInput{.heading = intent.heading});
  TakeNews();
  presentation_.Follow(play_->run().field().step, play_->blend_per_mille());
  FollowRun();
}

void SurvivorsStage::AdvanceArriving(std::uint32_t elapsed_ms, const SurvivorsIntent& intent) {
  if (AsksPause(intent)) {
    PauseFor(CauseOf(intent));
    return;
  }
  arrival_ms_ = static_cast<std::uint32_t>(
      std::min<std::uint64_t>(std::uint64_t{arrival_ms_} + elapsed_ms, kArrivalMs));
  if (arrival_ms_ >= kArrivalMs) Enter(SurvivorsScreen::kPlaying);
}

void SurvivorsStage::AdvanceChoosing(const SurvivorsIntent& intent) {
  HoldPause(intent);
  if (!intent.answer || !guard_.Accepts(clock_ms_)) return;
  if (!play_->Answer(*intent.answer)) return;
  TakeNews();
  FollowRun();
}

void SurvivorsStage::AdvanceChest(const SurvivorsIntent& intent) {
  HoldPause(intent);
  const bool clicked = intent.proceed && guard_.Accepts(clock_ms_);
  if (!chest_list_shown_) {
    if (clicked || screen_elapsed_ms() >= kChestRouletteMs) {
      chest_list_shown_ = true;
      guard_.Arm(clock_ms_);
    }
    return;
  }
  if (!clicked) return;
  chests_.clear();
  chest_list_shown_ = false;
  FollowRun();
}

void SurvivorsStage::AdvancePaused(const SurvivorsIntent& intent) {
  if (intent.abandon) {
    play_->Abandon();
    TakeNews();
    FollowRun();
    return;
  }
  // Une alerte arrivée pendant la pause du joueur prend le bandeau.
  if (intent.world_alert) pause_cause_ = PauseCause::kWorldAlert;
  if ((intent.escape || intent.resume) && guard_.Accepts(clock_ms_)) {
    Enter(arrival_ms_ < kArrivalMs ? SurvivorsScreen::kArriving : SurvivorsScreen::kPlaying);
  }
}

void SurvivorsStage::HoldPause(const SurvivorsIntent& intent) {
  // L'alerte l'emporte sur la perte de focus : son bandeau dit davantage.
  if (intent.world_alert) {
    held_pause_ = PauseCause::kWorldAlert;
  } else if (intent.focus_lost && !held_pause_) {
    held_pause_ = PauseCause::kFocusLost;
  }
}

void SurvivorsStage::AdvanceEnd(std::uint32_t elapsed_ms, const SurvivorsIntent& intent,
                                std::uint32_t lasts_ms) {
  presentation_.RunOn(elapsed_ms);
  if (screen_elapsed_ms() >= lasts_ms || (intent.proceed && guard_.Accepts(clock_ms_))) {
    Enter(SurvivorsScreen::kSummary);
  }
}

void SurvivorsStage::PauseFor(PauseCause cause) {
  pause_cause_ = cause;
  Enter(SurvivorsScreen::kPaused);
}

void SurvivorsStage::FollowRun() {
  const ro::sim::SurvivorsField& field = play_->run().field();
  if (field.phase == SurvivorsPhase::kOver) {
    ending_ = EndingOf(field, board_);
    switch (field.tally.outcome) {
      case SurvivorsOutcome::kFell:
        Enter(SurvivorsScreen::kDying);
        return;
      case SurvivorsOutcome::kAbandoned:
        Enter(SurvivorsScreen::kSummary);
        return;
      case SurvivorsOutcome::kRunning:
      case SurvivorsOutcome::kSurvived:
      case SurvivorsOutcome::kVictory:
        Enter(SurvivorsScreen::kWon);
        return;
    }
    return;
  }
  if (!chests_.empty()) {
    chest_list_shown_ = false;
    Enter(SurvivorsScreen::kChest);
    return;
  }
  // Chaque choix s'ouvre avec sa garde, une relance comprise : ses offres
  // sont neuves.
  if (field.phase == SurvivorsPhase::kChoosing) {
    Enter(SurvivorsScreen::kChoosing);
    return;
  }
  if (held_pause_) {
    const PauseCause cause = *held_pause_;
    held_pause_.reset();
    PauseFor(cause);
    return;
  }
  if (screen_ != SurvivorsScreen::kPlaying) Enter(SurvivorsScreen::kPlaying);
}

void SurvivorsStage::TakeNews() {
  news_ = play_->TakeNews();
  chests_.insert(chests_.end(), news_.chests.begin(), news_.chests.end());
}

}  // namespace moonclient
