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
inline constexpr std::uint16_t kGoldenLoadedRulesetVersion = 7;
inline constexpr sim::SurvivorsTally kGoldenLoadedTally{
    .survived_steps = 41731,
    .kills = 8408,
    .boss_kills = 4,
    .kill_points = 186614,
    .level = 36,
    .exp_total = 10310,
    .hp_left = 0,
    .damage_dealt = 1232961,
    .gems_gathered = 2506,
    .foes_alive = 377,
    .outcome = sim::SurvivorsOutcome::kFell,
    .elite_kills = 10,
    .chests_opened = 12,
    .evolutions = 1,
};
inline constexpr sim::FieldPoint kGoldenLoadedHeroPosition{73003, 98662};
// Le robot se relève une fois, sa vie de réserve, puis tombe à 13:55. Six
// cartes tombent, dont celles que les mini-boss lâchent à coup sûr, et le
// robot en ramasse cinq ; aucun monstre n'est glacé.
inline constexpr std::uint8_t kGoldenLoadedLivesLeft = 0;
inline constexpr std::uint32_t kGoldenLoadedCardsDropped = 6;
inline constexpr std::array<std::uint16_t, 5> kGoldenLoadedCardsFound{4197, 4266, 4179, 4183, 4085};
inline constexpr std::uint64_t kGoldenLoadedChills = 0;
inline constexpr std::array<GoldenOwned, kGoldenBotOwnedCount> kGoldenLoadedSkills{{
    {.id = 60001, .level = 2},  // le coup de baguette
    {.id = 83, .level = 1},     // Meteor Storm
    {.id = 21, .level = 4},     // Thunderstorm
    {.id = 15, .level = 5},     // Frost Diver
    {.id = 17, .level = 8},     // Fire Ball
    {.id = 86, .level = 8},     // Water Ball
    {.id = 18, .level = 3},     // Fire Wall
}};
// Golem Card verrouillée n'est jamais prise.
inline constexpr std::array<GoldenOwned, kGoldenBotOwnedCount> kGoldenLoadedCards{{
    {.id = 4197, .level = 2},  // Mastering Card
    {.id = 4003, .level = 3},  // Pupa Card
    {.id = 4019, .level = 4},  // Hornet Card
    {.id = 4147, .level = 1},  // Baphomet Card
    {.id = 4179, .level = 2},  // Dragon Fly Card
    {.id = 4022, .level = 1},  // Spore Card
    {},                        // emplacement libre
}};

}  // namespace ro::test_support
