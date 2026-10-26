#include "game/survivors_book_mode.h"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <utility>

#include "ro/core/error.h"
#include "ro/core/log.h"
#include "ro/sim/survivors_ranked.h"

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

SurvivorsBookMode::Begun SurvivorsBookMode::StartAs(SurvivorsBoardKind kind,
                                                    const ro::sim::SurvivorsRules& rules,
                                                    const std::filesystem::path& client_root,
                                                    std::uint64_t seed,
                                                    std::uint16_t first_skill_id) {
  if (kind == SurvivorsBoardKind::kChallenge) {
    const ro::sim::IsoWeek week = IsoWeekOfUtc(now_utc_());
    return Begun{.start = ChallengeStartOf(rules, week),
                 .board = SurvivorsRunBoard{.kind = SurvivorsBoardKind::kChallenge, .week = week}};
  }
  if (kind == SurvivorsBoardKind::kRanked) {
    return Begun{.start = ro::sim::RankedStartOf(rules, seed, first_skill_id),
                 .board = SurvivorsRunBoard{.kind = SurvivorsBoardKind::kRanked}};
  }
  const SurvivorsSaveRead read = file_.Load(client_root);
  if (read.file_unread) {
    ro::core::LogWarning("survivors : carnet illisible, la partie part sans la méta");
  }
  if (read.damaged_copy.has_value()) damaged_copy_ = read.damaged_copy;
  return Begun{.start = FreeStartOf(rules, read.save, seed, first_skill_id), .board = {}};
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
