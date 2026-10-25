#pragma once

#include <cstdint>

#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_state.h"

namespace ro::sim {

// LA FORMULE QUE LE SERVEUR REFAIT : toute modification monte
// kSurvivorsRulesetVersion et se reporte côté serveur.
inline constexpr std::uint32_t kScorePerSecond = 10;
inline constexpr std::uint32_t kScorePerLevel = 100;
inline constexpr std::uint32_t kSurvivedScore = 5000;
inline constexpr std::uint32_t kVictoryScore = 25000;
inline constexpr std::uint32_t kMiniBossKillFactor = 20;
inline constexpr std::uint32_t kBossKillFactor = 100;
inline constexpr std::uint32_t kEliteKillFactor = 5;

// Les points d'un tué : ro_level × le facteur de son rang (1 pour un commun),
// × kEliteKillFactor pour une élite.
[[nodiscard]] std::uint32_t KillPoints(const FoeRule& foe, bool elite) noexcept;
// kill_points + 10 × (survived_steps / 50) + 100 × level + le bonus de
// l'issue (kSurvivedScore, kVictoryScore, rien sinon) : 10 par SECONDE
// entière survécue, la division d'abord.
[[nodiscard]] std::uint64_t SurvivorsScore(const SurvivorsTally& tally) noexcept;

// LE SCORE D'UNE PARTIE LIBRE, que le carnet range : SurvivorsScore × (100 +
// 10 × rang de Cursed Water) / 100, division entière, saturé. Le serveur ne
// le refait pas : seule une partie à départ neutre se classe.
inline constexpr std::uint64_t kCursedWaterScorePctPerRank = 10;
[[nodiscard]] std::uint64_t FreeRunScore(const SurvivorsTally& tally,
                                         std::uint8_t cursed_water) noexcept;

}  // namespace ro::sim
