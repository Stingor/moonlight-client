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
inline constexpr std::uint16_t kGoldenTerrainRulesetVersion = 7;
inline constexpr sim::SurvivorsTally kGoldenTerrainTally{
    .survived_steps = 46312,
    .kills = 10541,
    .boss_kills = 5,
    .kill_points = 286201,
    .level = 67,
    .exp_total = 181844,
    .hp_left = 292,
    .damage_dealt = 1240344,
    .gems_gathered = 6373,
    .foes_alive = 10,
    .outcome = sim::SurvivorsOutcome::kVictory,
    .elite_kills = 11,
    .chests_opened = 14,
    .evolutions = 3,
};
// Le robot l'emporte à 15:26, à quelque 33 cases à l'ouest et 105 au nord de
// son départ.
inline constexpr sim::FieldPoint kGoldenTerrainHeroPosition{33520740, 33662533};
// En fin de partie : les plantes encore posées, les tués à la dernière
// naissance d'une plante, les plantes cassées et le prochain numéro de
// naissance.
inline constexpr std::size_t kGoldenTerrainPlants = 0;
inline constexpr std::uint32_t kGoldenTerrainPlantKillsMark = 10391;
inline constexpr std::uint32_t kGoldenTerrainPlantsBroken = 94;
inline constexpr std::uint32_t kGoldenTerrainNextSerial = 20732;
inline constexpr std::array<GoldenOwned, kGoldenBotOwnedCount> kGoldenTerrainSkills{{
    {.id = 60001, .level = 2},  // le coup de baguette
    {.id = 83, .level = 1},     // Meteor Storm
    {.id = 85, .level = 1},     // Lord of Vermilion
    {.id = 81, .level = 1},     // Sightrasher
    {.id = 11, .level = 8},     // Napalm Beat
    {.id = 87, .level = 8},     // Ice Wall
    {.id = 13, .level = 8},     // Soul Strike
}};
inline constexpr std::array<GoldenOwned, kGoldenBotOwnedCount> kGoldenTerrainCards{{
    {.id = 4147, .level = 5},  // Baphomet Card
    {.id = 4009, .level = 5},  // Chonchon Card
    {.id = 4197, .level = 2},  // Mastering Card
    {.id = 4019, .level = 5},  // Hornet Card
    {.id = 4079, .level = 5},  // Mantis Card
    {.id = 4183, .level = 5},  // Vagabond Wolf Card
    {},                        // emplacement libre
}};

}  // namespace ro::test_support
