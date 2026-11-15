#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>

#include "ro/sim/survivors_bot.h"
#include "ro/sim/survivors_geometry.h"
#include "ro/sim/survivors_meta.h"
#include "ro/sim/survivors_play.h"
#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_run.h"
#include "ro/sim/survivors_state.h"

#include "support/survivors_fixture.h"
#include "support/survivors_golden.h"
#include "support/survivors_golden_bot.h"

namespace ro::test_support {

// LA PARTIE EN OR À DÉPART CHARGÉ : la partie en or longue, même graine, même
// arène et même robot, partie avec toute la méta. Chaque Bénédiction et chaque
// service Kafra au rang maximal (donc trois charges de chaque), la vie de
// Resurrection, Cursed Water 3 et Golem Card verrouillée : elle épingle ce que
// la partie interprète de la méta (bonus, charges, relève, Cursed Water,
// verrou, chute des cartes). Sans Catch2 ni I/O, comme les deux autres.

inline constexpr std::uint8_t kGoldenLoadedCursedWater = 3;
inline constexpr std::uint16_t kGoldenLoadedLockedCardId = 4072;  // Golem Card

[[nodiscard]] inline sim::SurvivorsStart GoldenLoadedStart(std::uint64_t seed) {
  sim::SurvivorsStart start = GoldenStart(seed);
  const sim::SurvivorsMeta& meta = sim::ShippedSurvivorsMeta();
  const sim::ClassRule* hero_class = sim::FindClass(GoldenRules(), kGoldenClassId);
  if (hero_class == nullptr) return start;
  const sim::SurvivorsMetaStart loaded =
      sim::LoadoutOf(meta, sim::MostMetaRanks(meta), *hero_class);
  start.loadout = loaded.loadout;
  start.lives = loaded.lives;
  start.cursed_water = kGoldenLoadedCursedWater;
  start.locked_ids = {kGoldenLoadedLockedCardId};
  return start;
}

// La partie chargée jouée jusqu'à sa fin ; rien si Start refuse.
[[nodiscard]] inline std::optional<sim::SurvivorsPlay> PlayGoldenLoaded(std::uint64_t seed) {
  auto run = sim::SurvivorsRun::Start(GoldenRules(), PillarArena(kGoldenArenaCells),
                                      GoldenLoadedStart(seed));
  if (!run) return std::nullopt;
  std::optional<sim::SurvivorsPlay> play(std::in_place, std::move(*run));
  sim::SurvivorsBot bot(sim::MageBotProfile());
  sim::PlayBot(*play, bot, GoldenBotMostSteps(), nullptr);
  return play;
}

// ⚠ LE BILAN EN OR À DÉPART CHARGÉ, épinglé avec la version des règles : il
// change quand le catalogue, le catalogue de la méta OU le robot changent.
// Les mêmes valeurs sous MSVC et sous g++-14.
inline constexpr std::uint16_t kGoldenLoadedRulesetVersion = 9;
inline constexpr sim::SurvivorsTally kGoldenLoadedTally{
    .survived_steps = 46428,
    .kills = 7657,
    .boss_kills = 5,
    .kill_points = 234996,
    .level = 56,
    .exp_total = 98119,
    .hp_left = 289,
    .damage_dealt = 1512054,
    .gems_gathered = 2636,
    .foes_alive = 0,
    .outcome = sim::SurvivorsOutcome::kVictory,
    .elite_kills = 11,
    .chests_opened = 13,
    .evolutions = 2,
};
inline constexpr sim::FieldPoint kGoldenLoadedHeroPosition{123781, 137880};
// Le robot dépense sa vie de réserve et l'emporte à 15:28. Dix cartes
// tombent, dont celles que les mini-boss lâchent à coup sûr, et le robot en
// ramasse quatre ; aucun monstre n'est glacé.
inline constexpr std::uint8_t kGoldenLoadedLivesLeft = 0;
inline constexpr std::uint32_t kGoldenLoadedCardsDropped = 10;
inline constexpr std::array<std::uint16_t, 4> kGoldenLoadedCardsFound{4266, 4183, 4179, 4147};
inline constexpr std::uint64_t kGoldenLoadedChills = 0;
inline constexpr std::array<GoldenOwned, kGoldenBotOwnedCount> kGoldenLoadedSkills{{
    {.id = 60001, .level = 2},  // le coup de baguette
    {.id = 83, .level = 1},     // Meteor Storm
    {.id = 18, .level = 8},     // Fire Wall
    {.id = 21, .level = 8},     // Thunderstorm
    {.id = 15, .level = 8},     // Frost Diver
    {.id = 81, .level = 1},     // Sightrasher
    {.id = 20, .level = 8},     // Lightning Bolt
}};
// Golem Card verrouillée n'est jamais prise.
inline constexpr std::array<GoldenOwned, kGoldenBotOwnedCount> kGoldenLoadedCards{{
    {.id = 4019, .level = 5},  // Hornet Card
    {.id = 4147, .level = 4},  // Baphomet Card
    {.id = 4009, .level = 2},  // Chonchon Card
    {.id = 4179, .level = 3},  // Dragon Fly Card
    {.id = 4197, .level = 2},  // Mastering Card
    {.id = 4003, .level = 3},  // Pupa Card
    {},                        // emplacement libre
}};

}  // namespace ro::test_support
