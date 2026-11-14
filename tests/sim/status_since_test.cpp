// L'INSTANT OÙ UN ÉTAT EST POSÉ : une pose qu'il impose s'anime depuis lui.

#include <cstdint>
#include <optional>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/actor.h"
#include "ro/sim/motion_events.h"
#include "ro/sim/status_effects.h"
#include "ro/sim/world.h"

namespace {

// Un passant, sans rien d'autre que sa case.
ro::sim::Actor Someone(ro::sim::CellPosition cell) {
  ro::sim::Actor actor;
  actor.id = 150'001;
  actor.position = cell;
  return actor;
}

}  // namespace

TEST_CASE("Monde : un état date sa pose, une redite ne la redate pas", "[sim][world]") {
  constexpr std::uint32_t kNovice = 150'001;
  constexpr std::uint32_t kPosedMs = 1000;
  constexpr std::uint32_t kRepeatedMs = 5000;
  ro::sim::World world;
  ro::sim::Actor novice;
  novice.id = kNovice;
  world.Spawn(novice);
  REQUIRE(world.SetStatusEffect(kNovice, ro::sim::kEfstTrickDead, true, std::nullopt, std::nullopt,
                                kPosedMs));
  CHECK(world.Find(kNovice)->status_since_ms.at(ro::sim::kEfstTrickDead) == kPosedMs);
  REQUIRE(world.SetStatusEffect(kNovice, ro::sim::kEfstTrickDead, true, std::nullopt, std::nullopt,
                                kRepeatedMs));
  CHECK(world.Find(kNovice)->status_since_ms.at(ro::sim::kEfstTrickDead) == kPosedMs);
  // La levée emporte l'instant avec l'état.
  REQUIRE(world.SetStatusEffect(kNovice, ro::sim::kEfstTrickDead, false));
  CHECK_FALSE(world.Find(kNovice)->status_since_ms.contains(ro::sim::kEfstTrickDead));
}

TEST_CASE("Monde : la sortie de Hiding se compte, et une réannonce ne compte rien",
          "[sim][world]") {
  // La sortie de Hiding fait sa fumée comme l'entrée : il faut donc le front
  // DESCENDANT, que le mot seul ne dit pas plus que le montant.
  ro::sim::World world;
  world.Spawn(Someone(ro::sim::CellPosition{1, 1, 0}));
  const ro::sim::Actor* watched = world.Find(150'001);
  REQUIRE(watched != nullptr);

  CHECK(world.SetStates(150'001, 0, 0, ro::sim::kOptionHide, 100));
  CHECK(watched->option_fall_count == 0);
  CHECK(watched->state_change_count == 1);
  CHECK(world.SetStates(150'001, 0, 0, 0, 200));
  CHECK(watched->option_fall_count == 1);
  CHECK(watched->option_fallen == ro::sim::kOptionHide);
  CHECK(watched->option_rise_count == 1);
  CHECK(watched->state_change_count == 2);

  // 🔴 LE SERVEUR REDIT L'UNITÉ À CHAQUE PAS : la fiche neuve garde les comptes.
  world.Spawn(Someone(ro::sim::CellPosition{2, 1, 0}));
  watched = world.Find(150'001);
  REQUIRE(watched != nullptr);
  CHECK(watched->option_fall_count == 1);
  CHECK(watched->option_rise_count == 1);
  CHECK(watched->state_change_count == 2);
}

TEST_CASE("Monde : le silence note son début et l'oublie quand il cesse", "[sim][world]") {
  // Les points de suspension s'animent depuis le début du silence ; un état
  // redit pendant qu'il dure ne les fait pas repartir de zéro.
  ro::sim::World world;
  world.Spawn(Someone(ro::sim::CellPosition{1, 1, 0}));
  const ro::sim::Actor* watched = world.Find(150'001);
  REQUIRE(watched != nullptr);
  CHECK_FALSE(watched->silenced_since_ms.has_value());

  CHECK(world.SetStates(150'001, 0, ro::sim::kHealthSilenced, 0, 300));
  REQUIRE(watched->silenced_since_ms.has_value());
  CHECK(*watched->silenced_since_ms == 300);
  CHECK(world.SetStates(150'001, 0, ro::sim::kHealthSilenced, ro::sim::kOptionHide, 900));
  CHECK(*watched->silenced_since_ms == 300);
  CHECK(watched->state_change_count == 2);

  CHECK(world.SetStates(150'001, 0, 0, 0, 1200));
  CHECK_FALSE(watched->silenced_since_ms.has_value());
}

TEST_CASE("Monde : le gel et l'étourdissement datent leur pose, et une redite ne la redate pas",
          "[sim][world]") {
  // 🔴 LA GLACE ET LES ÉTOILES S'ANIMENT DEPUIS CET INSTANT : une réannonce qui
  // le ramènerait à son arrivée les ferait repartir de leur première image.
  ro::sim::World world;
  world.Spawn(Someone(ro::sim::CellPosition{1, 1, 0}));
  const ro::sim::Actor* watched = world.Find(150'001);
  REQUIRE(watched != nullptr);
  CHECK_FALSE(ro::sim::BodyStateHeldSince(*watched, ro::sim::kBodyStunned).has_value());

  CHECK(world.SetStates(150'001, ro::sim::kBodyStunned, 0, 0, 400));
  CHECK(ro::sim::BodyStateHeldSince(*watched, ro::sim::kBodyStunned) == std::optional{400U});
  CHECK_FALSE(ro::sim::BodyStateHeldSince(*watched, ro::sim::kBodyFrozen).has_value());

  ro::sim::Actor announced = Someone(ro::sim::CellPosition{1, 1, 0});
  announced.body_state = ro::sim::kBodyStunned;
  announced.body_state_since_ms = 2000;
  world.Spawn(announced);
  watched = world.Find(150'001);
  REQUIRE(watched != nullptr);
  CHECK(ro::sim::BodyStateHeldSince(*watched, ro::sim::kBodyStunned) == std::optional{400U});

  // Un autre état du corps repart de son instant à lui.
  CHECK(world.SetStates(150'001, ro::sim::kBodyFrozen, 0, 0, 2500));
  CHECK(ro::sim::BodyStateHeldSince(*watched, ro::sim::kBodyFrozen) == std::optional{2500U});
}

TEST_CASE("Monde : une redite n'éteint pas les bulles d'un dormeur", "[sim][world]") {
  ro::sim::World world;
  world.Spawn(Someone(ro::sim::CellPosition{1, 1, 0}));
  CHECK(world.SetStates(150'001, ro::sim::kBodyAsleep, 0, 0, 700));
  ro::sim::Actor sleeper = Someone(ro::sim::CellPosition{1, 1, 0});
  sleeper.body_state = ro::sim::kBodyAsleep;
  sleeper.body_state_since_ms = 3000;
  world.Spawn(sleeper);
  const ro::sim::Actor* watched = world.Find(150'001);
  REQUIRE(watched != nullptr);
  CHECK(watched->asleep_since_ms == std::optional{700U});
  CHECK(ro::sim::BodyStateHeldSince(*watched, ro::sim::kBodyAsleep) == std::optional{700U});
}

TEST_CASE("Monde : la malédiction date sa pose, une redite ne la redate pas", "[sim][world]") {
  // Sa marque s'anime depuis cet instant, comme les points du silence.
  ro::sim::World world;
  world.Spawn(Someone(ro::sim::CellPosition{1, 1, 0}));
  CHECK(world.SetStates(150'001, 0, ro::sim::kHealthCursed, 0, 800));
  CHECK(world.SetStates(150'001, 0, ro::sim::kHealthCursed, ro::sim::kOptionHide, 1500));
  ro::sim::Actor announced = Someone(ro::sim::CellPosition{2, 1, 0});
  announced.health_state = ro::sim::kHealthCursed;
  world.Spawn(announced);
  const ro::sim::Actor* watched = world.Find(150'001);
  REQUIRE(watched != nullptr);
  CHECK(watched->cursed_since_ms == std::optional{800U});

  CHECK(world.SetStates(150'001, 0, 0, 0, 2000));
  CHECK_FALSE(watched->cursed_since_ms.has_value());
}

TEST_CASE("Monde : Blade Stop prend deux acteurs connus, et relâche chacun", "[sim][world]") {
  constexpr std::uint32_t kMonk = 150'001;
  constexpr std::uint32_t kKnight = 150'002;
  ro::sim::World world;
  world.Spawn(Someone(ro::sim::CellPosition{1, 1, 0}));
  // Sans la cible, la prise ne se pose pas.
  CHECK_FALSE(world.SetBladeStop(kMonk, kKnight, true, 100));
  CHECK_FALSE(world.Find(kMonk)->blade_stop_since_ms.has_value());

  ro::sim::Actor knight = Someone(ro::sim::CellPosition{2, 1, 0});
  knight.id = kKnight;
  world.Spawn(knight);
  CHECK(world.SetBladeStop(kMonk, kKnight, true, 200));
  CHECK(world.Find(kMonk)->blade_stop_since_ms == std::optional{200U});
  CHECK(world.Find(kMonk)->blade_stop_caster);
  CHECK(world.Find(kKnight)->blade_stop_since_ms == std::optional{200U});
  CHECK_FALSE(world.Find(kKnight)->blade_stop_caster);

  // Une réannonce ne relâche pas la prise.
  world.Spawn(knight);
  CHECK(world.Find(kKnight)->blade_stop_since_ms == std::optional{200U});

  CHECK(world.SetBladeStop(kMonk, kKnight, false, 900));
  CHECK_FALSE(world.Find(kMonk)->blade_stop_since_ms.has_value());
  // Le moine garde son côté et l'instant du relâchement : sa pose tient encore.
  CHECK(world.Find(kMonk)->blade_stop_caster);
  CHECK(world.Find(kMonk)->blade_stop_released_ms == std::optional{900U});
  CHECK_FALSE(world.Find(kKnight)->blade_stop_released_ms.has_value());
  CHECK_FALSE(world.Find(kKnight)->blade_stop_since_ms.has_value());
}

TEST_CASE(
    "Monde : une réannonce garde les états et leurs valeurs, une entrée en vue repart de rien",
    "[sim][world]") {
  // 🔴 LE SERVEUR RÉANNONCE UN ACTEUR À CHAQUE PAS, SANS REDIRE SES ÉTATS : les
  // perdre le descendrait de sa monture dès qu'il marche.
  ro::sim::World world;
  world.Spawn(Someone(ro::sim::CellPosition{1, 1, 0}));
  REQUIRE(world.SetStatusEffect(150'001, ro::sim::kStatusAllRiding, true));
  REQUIRE(world.SetStatusEffect(150'001, ro::sim::kStatusMadogear, true,
                                ro::sim::kMadogearSuitValue, std::nullopt, 1000U));
  world.Spawn(Someone(ro::sim::CellPosition{2, 2, 0}));
  const ro::sim::Actor* walker = world.Find(150'001);
  REQUIRE(walker != nullptr);
  CHECK(walker->status_effects.contains(ro::sim::kStatusAllRiding));
  CHECK(walker->status_values.at(ro::sim::kStatusMadogear) == ro::sim::kMadogearSuitValue);
  CHECK(walker->status_since_ms.at(ro::sim::kStatusMadogear) == 1000U);
  // Sorti de la vue puis revenu, il repart de rien : le serveur redit alors ce
  // qui tient.
  REQUIRE(world.Remove(150'001));
  world.Spawn(Someone(ro::sim::CellPosition{2, 2, 0}));
  CHECK(world.Find(150'001)->status_effects.empty());
  CHECK(world.Find(150'001)->status_values.empty());
}
