// LE PIÈGE QUI SE REFERME : ce que le monde retient pour que le modèle d'une
// unité au sol joue son animation une fois — un coup porté par l'unité, ou le
// changement d'apparence que le serveur annonce.

#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/actor.h"
#include "ro/sim/combat_notice.h"
#include "ro/sim/skill_unit_look.h"
#include "ro/sim/world.h"

namespace {

constexpr std::uint32_t kUnit = 0x0011'2233;
constexpr std::uint32_t kVictim = 0x0044'5566;
// Un piège, puis l'apparence du piège usé.
constexpr std::uint32_t kTrap = 145;
constexpr std::uint32_t kUsedTrap = 140;

[[nodiscard]] ro::sim::SkillUnit Trap(std::uint32_t placed_ms) {
  ro::sim::SkillUnit unit;
  unit.id = kUnit;
  unit.unit_id = kTrap;
  unit.cell = ro::sim::CellPosition{40, 40};
  unit.placed_ms = placed_ms;
  unit.look_since_ms = placed_ms;
  return unit;
}

}  // namespace

TEST_CASE("Monde : un piège posé reste figé", "[sim][world][skill_unit]") {
  ro::sim::World world;
  world.PlaceSkillUnit(Trap(100));
  const ro::sim::SkillUnit& unit = world.skill_units().at(kUnit);
  CHECK(unit.visible);
  CHECK_FALSE(unit.sprung_ms.has_value());
}

TEST_CASE("Monde : le coup d'une unité fait jouer son modèle, une seule fois",
          "[sim][world][skill_unit]") {
  ro::sim::World world;
  ro::sim::Actor victim;
  victim.id = kVictim;
  world.Spawn(victim);
  world.PlaceSkillUnit(Trap(100));

  ro::sim::Strike strike;
  strike.attacker = kUnit;
  strike.target = kVictim;
  strike.started_ms = 500;
  world.RecordStrike(strike);
  REQUIRE(world.skill_units().at(kUnit).sprung_ms.has_value());
  CHECK(*world.skill_units().at(kUnit).sprung_ms == 500);

  // Le second coup ne relance rien : le modèle est déjà parti.
  strike.started_ms = 900;
  world.RecordStrike(strike);
  CHECK(*world.skill_units().at(kUnit).sprung_ms == 500);
}

TEST_CASE("Monde : un changement d'apparence rebâtit le modèle et le fait jouer",
          "[sim][world][skill_unit]") {
  ro::sim::World world;
  world.PlaceSkillUnit(Trap(100));
  CHECK(ro::sim::ChangeSkillUnitLook(world, kUnit, kUsedTrap, 700));
  const ro::sim::SkillUnit& unit = world.skill_units().at(kUnit);
  CHECK(unit.unit_id == kUsedTrap);
  CHECK(unit.look_since_ms == 700);
  REQUIRE(unit.sprung_ms.has_value());
  CHECK(*unit.sprung_ms == 700);
  // La pose, elle, ne bouge pas.
  CHECK(unit.placed_ms == 100);

  CHECK_FALSE(ro::sim::ChangeSkillUnitLook(world, kVictim, kUsedTrap, 800));
}

TEST_CASE("Monde : une unité cachée reste muette quand son apparence change",
          "[sim][world][skill_unit]") {
  ro::sim::World world;
  ro::sim::SkillUnit hidden = Trap(100);
  hidden.visible = false;
  hidden.unit_id = ro::sim::kSilentSkillUnit;
  world.PlaceSkillUnit(hidden);
  CHECK(ro::sim::ChangeSkillUnitLook(world, kUnit, kUsedTrap, 700));
  CHECK(world.skill_units().at(kUnit).unit_id == ro::sim::kSilentSkillUnit);
}
