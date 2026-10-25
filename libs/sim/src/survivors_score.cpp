#include "ro/sim/survivors_score.h"

#include <cstdint>
#include <limits>

#include "ro/sim/survivors_geometry.h"

namespace ro::sim {

std::uint32_t KillPoints(const FoeRule& foe, bool elite) noexcept {
  std::uint32_t rank_factor = 1;
  switch (foe.rank) {
    case FoeRank::kCommon:
      break;
    case FoeRank::kMiniBoss:
      rank_factor = kMiniBossKillFactor;
      break;
    case FoeRank::kBoss:
      rank_factor = kBossKillFactor;
      break;
  }
  // Au plus 255 × 100 × 5 : le produit tient dans 32 bits.
  return std::uint32_t{foe.ro_level} * rank_factor * (elite ? kEliteKillFactor : 1U);
}

std::uint64_t SurvivorsScore(const SurvivorsTally& tally) noexcept {
  std::uint64_t outcome_score = 0;
  switch (tally.outcome) {
    case SurvivorsOutcome::kSurvived:
      outcome_score = kSurvivedScore;
      break;
    case SurvivorsOutcome::kVictory:
      outcome_score = kVictoryScore;
      break;
    case SurvivorsOutcome::kRunning:
    case SurvivorsOutcome::kFell:
    case SurvivorsOutcome::kAbandoned:
      break;
  }
  // En 64 bits, et par SECONDES ENTIÈRES survécues : une seconde entamée ne
  // compte pas. Le serveur refait ce calcul et doit tomber sur le même entier.
  const std::uint64_t survived_seconds = tally.survived_steps / kStepsPerSecond;
  return std::uint64_t{tally.kill_points} + std::uint64_t{kScorePerSecond} * survived_seconds +
         std::uint64_t{kScorePerLevel} * std::uint64_t{tally.level} + outcome_score;
}

std::uint64_t FreeRunScore(const SurvivorsTally& tally, std::uint8_t cursed_water) noexcept {
  constexpr std::uint64_t kWholePct = 100;
  const std::uint64_t raised_pct = kWholePct + kCursedWaterScorePctPerRank * cursed_water;
  const std::uint64_t score = SurvivorsScore(tally);
  // Le score tient très en deçà de 2^64 / 150 ; la saturation n'est qu'une garde.
  if (score > std::numeric_limits<std::uint64_t>::max() / raised_pct) {
    return std::numeric_limits<std::uint64_t>::max();
  }
  return score * raised_pct / kWholePct;
}

}  // namespace ro::sim
