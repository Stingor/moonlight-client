#include "game/survivors_book_mode.h"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <utility>

#include "ro/core/error.h"
#include "ro/core/log.h"
#include "ro/sim/survivors_ranked.h"

#include "survivors/survivors_meeting.h"

namespace moonclient {
namespace {

[[nodiscard]] std::int64_t SystemUtcSeconds() {
  return std::chrono::duration_cast<std::chrono::seconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

}  // namespace

SurvivorsBookMode::SurvivorsBookMode(UtcClock now_utc, SurvivorsSaveFile file)
    : now_utc_(now_utc ? std::move(now_utc) : UtcClock(SystemUtcSeconds)), file_(std::move(file)) {}

void SurvivorsBookMode::ChooseArena(const ro::sim::SurvivorsRules& rules,
                                    std::uint16_t arena_id) noexcept {
  if (ro::sim::FindArena(rules, arena_id) != nullptr) free_arena_id_ = arena_id;
}

void SurvivorsBookMode::ChooseClass(const ro::sim::SurvivorsRules& rules,
                                    std::uint16_t class_id) noexcept {
  if (ro::sim::FindClass(rules, class_id) != nullptr) class_id_ = class_id;
}

std::uint16_t SurvivorsBookMode::board_class_id(
    const ro::sim::SurvivorsRules& rules) const noexcept {
  if (class_id_ != 0 || rules.classes.empty()) return class_id_;
  return rules.classes.front().class_id;
}

void SurvivorsBookMode::SettleClass(const ro::sim::SurvivorsRules& rules,
                                    const SurvivorsSave& save) {
  const ro::sim::ClassRule* chosen = ro::sim::FindClass(rules, class_id_);
  if (chosen != nullptr && !ClassUnlocked(rules, *chosen, save)) class_id_ = 0;
}

bool SurvivorsBookMode::ChosenClassOpen(const ro::sim::SurvivorsRules& rules,
                                        const std::filesystem::path& client_root) {
  const ro::sim::ClassRule* chosen = ro::sim::FindClass(rules, board_class_id(rules));
  if (chosen == nullptr) return true;
  // Une classe hors classement ne joue ni la partie classée ni le défi.
  if (chosen->free_play_only && kind_ != SurvivorsBoardKind::kFree) return false;
  // Une classe ouverte d'emblée ne dépend pas du carnet.
  if (ClassUnlocked(rules, *chosen, SurvivorsSave{})) return true;
  const SurvivorsSaveRead read = file_.Load(client_root);
  if (read.damaged_copy.has_value()) damaged_copy_ = read.damaged_copy;
  return ClassUnlocked(rules, *chosen, read.save);
}

SurvivorsBookMode::Begun SurvivorsBookMode::StartAs(SurvivorsBoardKind kind,
                                                    const ro::sim::SurvivorsRules& rules,
                                                    const std::filesystem::path& client_root,
                                                    std::uint64_t seed) {
  if (kind == SurvivorsBoardKind::kChallenge) {
    const ro::sim::IsoWeek week = IsoWeekOfUtc(now_utc_());
    return Begun{.start = ChallengeStartOf(rules, week, board_class_id(rules)),
                 .board = SurvivorsRunBoard{.kind = SurvivorsBoardKind::kChallenge, .week = week}};
  }
  if (kind == SurvivorsBoardKind::kRanked) {
    return Begun{.start = ro::sim::RankedStartOf(rules, seed, board_class_id(rules)),
                 .board = SurvivorsRunBoard{.kind = SurvivorsBoardKind::kRanked}};
  }
  return FreeStart(rules, client_root, seed, free_arena_id_, class_id_);
}

SurvivorsBookMode::Begun SurvivorsBookMode::StartRankedAsFree(
    const ro::sim::SurvivorsRules& rules, const std::filesystem::path& client_root,
    std::uint64_t seed, std::uint16_t class_id) {
  const ro::sim::ArenaRule* ranked_arena = ro::sim::FirstRankedArena(rules);
  return FreeStart(rules, client_root, seed,
                   ranked_arena == nullptr ? std::uint16_t{0} : ranked_arena->arena_id, class_id);
}

std::optional<SurvivorsBookMode::Begun> SurvivorsBookMode::StartAlone(
    const ro::sim::SurvivorsRules& rules, const std::filesystem::path& client_root,
    std::uint64_t seed) {
  if (kind_ != SurvivorsBoardKind::kFree && !ChosenClassOpen(rules, client_root)) {
    return std::nullopt;
  }
  if (kind_ == SurvivorsBoardKind::kRanked) {
    return StartRankedAsFree(rules, client_root, seed, board_class_id(rules));
  }
  return StartOf(rules, client_root, seed);
}

SurvivorsBookMode::Begun SurvivorsBookMode::FreeStart(const ro::sim::SurvivorsRules& rules,
                                                      const std::filesystem::path& client_root,
                                                      std::uint64_t seed, std::uint16_t arena_id,
                                                      std::uint16_t class_id) {
  const SurvivorsSaveRead read = file_.Load(client_root);
  if (read.file_unread) {
    ro::core::LogWarning("survivors : carnet illisible, la partie part sans la méta");
  }
  if (read.damaged_copy.has_value()) damaged_copy_ = read.damaged_copy;
  return Begun{.start = FreeStartOf(rules, read.save, seed, arena_id, class_id),
               .board = SurvivorsRunBoard{.without_minimap = read.save.meta.without_minimap}};
}

const ro::sim::ClassRule* SurvivorsBookMode::ClassAwaiting(
    const ro::sim::SurvivorsRules& rules, const ro::sim::ArenaRule& arena,
    const SurvivorsRunBoard& board, const std::filesystem::path& client_root) {
  const SurvivorsSaveRead read = file_.Load(client_root);
  if (read.damaged_copy.has_value()) damaged_copy_ = read.damaged_copy;
  return read.file_unread ? nullptr : ClassAwaitingMeeting(rules, arena, board, read.save);
}

void SurvivorsBookMode::Inscribe(std::string_view feat_key,
                                 const std::filesystem::path& client_root) {
  const ro::core::Result<SurvivorsSaveRead> written = file_.Mutate(
      client_root, [feat_key](SurvivorsSave& save) { return InscribeFeat(save, feat_key); });
  if (!written) {
    ro::core::LogWarning("survivors : exploit non inscrit au carnet — {}",
                         ro::core::Describe(written.error()));
    return;
  }
  if (written->damaged_copy.has_value()) damaged_copy_ = written->damaged_copy;
}

void SurvivorsBookMode::Restart() noexcept {
  credited_ = false;
  credit_.reset();
}

void SurvivorsBookMode::FollowEntered(const SurvivorsStage& stage,
                                      const ro::sim::SurvivorsRules& rules,
                                      const std::filesystem::path& client_root) {
  if (stage.entered() != SurvivorsScreen::kSummary) return;
  Credit(stage, rules, client_root);
}

void SurvivorsBookMode::Credit(const SurvivorsStage& stage, const ro::sim::SurvivorsRules& rules,
                               const std::filesystem::path& client_root) {
  // Un bilan relu à la réouverture de la salle n'a plus de partie : elle a été
  // créditée quand elle s'est figée.
  if (credited_ || stage.play() == nullptr || stage.ending() == nullptr) return;
  credited_ = true;
  const SurvivorsEnding& ending = *stage.ending();
  const ro::sim::SurvivorsStart& start = stage.play()->run().field().start;
  const std::int64_t now_utc_s = now_utc_();
  SurvivorsCredit credit;
  const ro::core::Result<SurvivorsSaveRead> written =
      file_.Mutate(client_root, [&](SurvivorsSave& save) {
        credit = CreditRun(rules, save, ending, start, now_utc_s);
        CreditSightings(save, stage.sightings());
        return true;
      });
  if (!written) {
    ro::core::LogWarning("survivors : partie non créditée au carnet — {}",
                         ro::core::Describe(written.error()));
    return;
  }
  if (written->damaged_copy.has_value()) damaged_copy_ = written->damaged_copy;
  credit_ = std::move(credit);
}

}  // namespace moonclient
