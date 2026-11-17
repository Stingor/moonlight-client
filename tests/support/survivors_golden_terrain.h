#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>

#include "ro/sim/survivors_bot.h"
#include "ro/sim/survivors_geometry.h"
#include "ro/sim/survivors_play.h"
#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_run.h"
#include "ro/sim/survivors_state.h"

#include "support/survivors_golden.h"
#include "support/survivors_golden_bot.h"

namespace ro::test_support {

// LA PARTIE EN OR SUR SOL GÉNÉRÉ : les règles livrées telles quelles, leur
// arène générée (la prairie), le départ de la graine en or, et le robot du
// Magicien aux commandes jusqu'à la fin de la partie. Le terrain, les plantes
// au fil des tués et la horde sans bord se redérivent de la graine : ni
// carte, ni installation. Sans Catch2 ni I/O, comme les autres parties en or, pour
// qu'un exécutable bâti par un autre compilateur compare son bilan.

inline constexpr std::uint16_t kGoldenTerrainArenaId = 2;

[[nodiscard]] inline sim::SurvivorsStart GoldenTerrainStart(std::uint64_t seed) {
  sim::SurvivorsStart start = GoldenStart(seed);
  start.arena_id = kGoldenTerrainArenaId;
  return start;
}

// La partie sur sol généré jouée jusqu'à sa fin ; rien si Start refuse.
[[nodiscard]] inline std::optional<sim::SurvivorsPlay> PlayGoldenTerrain(std::uint64_t seed) {
  auto run = sim::SurvivorsRun::Start(sim::ShippedSurvivorsRules(), GoldenTerrainStart(seed));
  if (!run) return std::nullopt;
  std::optional<sim::SurvivorsPlay> play(std::in_place, std::move(*run));
  sim::SurvivorsBot bot(sim::MageBotProfile());
  sim::PlayBot(*play, bot, GoldenBotMostSteps(), nullptr);
  return play;
}

// ⚠ LE BILAN EN OR SUR SOL GÉNÉRÉ, épinglé avec sa version : il change quand
// le catalogue, sa règle de terrain OU le robot changent. Les mêmes valeurs
// sous MSVC et sous g++-14.
inline constexpr std::uint16_t kGoldenTerrainRulesetVersion = 10;
inline constexpr sim::SurvivorsTally kGoldenTerrainTally{
    .survived_steps = 45480,
    .kills = 9159,
    .boss_kills = 5,
    .kill_points = 225191,
    .level = 57,
    .exp_total = 102632,
    .hp_left = 226,
    .damage_dealt = 925507,
    .gems_gathered = 5401,
    .foes_alive = 183,
    .outcome = sim::SurvivorsOutcome::kVictory,
    .elite_kills = 10,
    .chests_opened = 12,
    .evolutions = 1,
};
// Le robot l'emporte à 15:09, à quelque 43 cases à l'ouest et 150 au nord de
// son départ.
inline constexpr sim::FieldPoint kGoldenTerrainHeroPosition{33510414, 33708442};
// En fin de partie : les plantes encore posées, les tués à la dernière
// naissance d'une plante, les plantes cassées et le prochain numéro de
// naissance.
inline constexpr std::size_t kGoldenTerrainPlants = 0;
inline constexpr std::uint32_t kGoldenTerrainPlantKillsMark = 9001;
inline constexpr std::uint32_t kGoldenTerrainPlantsBroken = 85;
inline constexpr std::uint32_t kGoldenTerrainNextSerial = 19849;
inline constexpr std::array<GoldenOwned, kGoldenBotOwnedCount> kGoldenTerrainSkills{{
    {.id = 60001, .level = 2},  // le coup de baguette
    {.id = 19, .level = 6},     // Fire Bolt
    {.id = 11, .level = 5},     // Napalm Beat
    {.id = 13, .level = 8},     // Soul Strike
    {.id = 88, .level = 1},     // Frost Nova
    {.id = 86, .level = 8},     // Water Ball
    {.id = 14, .level = 6},     // Cold Bolt
}};
inline constexpr std::array<GoldenOwned, kGoldenBotOwnedCount> kGoldenTerrainCards{{
    {.id = 4019, .level = 5},  // Hornet Card
    {.id = 4072, .level = 5},  // Golem Card
    {.id = 4022, .level = 5},  // Spore Card
    {.id = 4197, .level = 2},  // Mastering Card
    {.id = 4266, .level = 5},  // Eclipse Card
    {.id = 4085, .level = 5},  // Orc Skeleton Card
    {},                        // emplacement libre
}};

}  // namespace ro::test_support
