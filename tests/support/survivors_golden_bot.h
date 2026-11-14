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

#include "support/survivors_fixture.h"
#include "support/survivors_golden.h"

namespace ro::test_support {

// LA PARTIE EN OR LONGUE : les règles livrées sur l'arène à piliers de la
// partie en or, le départ de sa graine, et le robot du Magicien aux
// commandes, jusqu'à la victoire, la chute ou la fin du dépassement. Comme
// la partie en or courte, elle ne dépend ni de Catch2 ni d'aucune I/O, pour
// qu'un exécutable bâti par un autre compilateur compare son bilan.

// La partie entière et son dépassement, plus une marge : PlayBot s'arrête de
// lui-même à la fin de la partie, la borne ne sert qu'à ne jamais boucler.
inline constexpr std::uint32_t kGoldenBotMarginSteps = 100;

[[nodiscard]] inline std::uint32_t GoldenBotMostSteps() {
  const sim::SurvivorsRules& rules = GoldenRules();
  return sim::StepsOf(rules.run_ms + rules.tuning.final_overtime_ms) + kGoldenBotMarginSteps;
}

// La partie longue jouée jusqu'à sa fin ; rien si Start refuse.
[[nodiscard]] inline std::optional<sim::SurvivorsPlay> PlayGoldenBot(std::uint64_t seed) {
  auto run =
      sim::SurvivorsRun::Start(GoldenRules(), PillarArena(kGoldenArenaCells), GoldenStart(seed));
  if (!run) return std::nullopt;
  std::optional<sim::SurvivorsPlay> play(std::in_place, std::move(*run));
  sim::SurvivorsBot bot(sim::MageBotProfile());
  sim::PlayBot(*play, bot, GoldenBotMostSteps(), nullptr);
  return play;
}

// ⚠ LE BILAN EN OR DU ROBOT, épinglé avec sa version : il change quand le
// catalogue OU le robot changent. Re-épinglé, il se vérifie sous MSVC et sous
// g++-14, qui rendent les mêmes valeurs.
inline constexpr std::uint16_t kGoldenBotRulesetVersion = 7;
inline constexpr sim::SurvivorsTally kGoldenBotTally{
    .survived_steps = 44549,
    .kills = 10545,
    .boss_kills = 4,
    .kill_points = 273185,
    .level = 37,
    .exp_total = 11285,
    .hp_left = 0,
    .damage_dealt = 1218978,
    .gems_gathered = 3392,
    .foes_alive = 308,
    .outcome = sim::SurvivorsOutcome::kFell,
    .elite_kills = 11,
    .chests_opened = 14,
    .evolutions = 1,
};
inline constexpr sim::FieldPoint kGoldenBotHeroPosition{158478, 163067};
// Ce qu'on épingle des skills et des cartes : un emplacement par entrée, les
// emplacements libres à vide. Sept, ceux des skills ; les cartes n'en ont que
// six, et la septième entrée reste vide.
inline constexpr std::size_t kGoldenBotOwnedCount = 7;
inline constexpr std::array<GoldenOwned, kGoldenBotOwnedCount> kGoldenBotSkills{{
    {.id = 60001, .level = 2},  // le coup de baguette
    {.id = 83, .level = 1},     // Meteor Storm
    {.id = 21, .level = 6},     // Thunderstorm
    {.id = 10, .level = 4},     // Sight
    {.id = 11, .level = 6},     // Napalm Beat
    {.id = 87, .level = 3},     // Ice Wall
    {.id = 13, .level = 6},     // Soul Strike
}};
inline constexpr std::array<GoldenOwned, kGoldenBotOwnedCount> kGoldenBotCards{{
    {.id = 4147, .level = 3},  // Baphomet Card
    {.id = 4009, .level = 5},  // Chonchon Card
    {.id = 4197, .level = 2},  // Mastering Card
    {.id = 4019, .level = 4},  // Hornet Card
    {.id = 4079, .level = 1},  // Mantis Card
    {.id = 4183, .level = 4},  // Vagabond Wolf Card
    {},                        // emplacement libre
}};

}  // namespace ro::test_support
