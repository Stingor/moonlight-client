// CE QUE LE MONDE TIENT SOUS UN IDENTIFIANT, et pourquoi il faut le demander
// aux DEUX tables.
//
// 🔴 UN PORTEUR N'EST PAS TOUJOURS UN ACTEUR. Les acteurs et les unités au sol
// vivent dans deux tables distinctes, et leurs identifiants viennent du même
// espace : le serveur numérote une unité comme il numérote un acteur. Ne
// consulter que les acteurs laissait le mur de Safety Wall à l'écran dix mille
// tics après que le serveur l'avait levé — vu en jeu par le joueur, qui a écrit
// « le safety wall reste affiché même quand il n'est plus actif ».

#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/actor.h"
#include "ro/sim/world.h"
#include "ro/sim/world_bearers.h"

namespace {

constexpr std::uint32_t kUnit = 0x0011'2233;
constexpr std::uint32_t kPlayer = 0x0044'5566;
constexpr std::uint32_t kPersonne = 0x0077'8899;

// Une unité au sol, telle que `ZC_SKILL_ENTRY` la pose. ⚠ 128 EST LE PORTAIL
// EN ATTENTE, pas Safety Wall, qui est la 126 : ce qui compte ici est qu'une
// unité vive dans une AUTRE table que les acteurs.
[[nodiscard]] ro::sim::SkillUnit GroundUnit() {
  ro::sim::SkillUnit unit;
  unit.id = kUnit;
  unit.unit_id = 128;
  unit.cell = ro::sim::CellPosition{40, 40};
  unit.skill_level = 10;
  return unit;
}

}  // namespace

TEST_CASE("Monde : une unité au sol se tient comme un acteur", "[sim][world]") {
  ro::sim::World world;
  ro::sim::Actor player;
  player.id = kPlayer;
  world.Spawn(player);
  world.PlaceSkillUnit(GroundUnit());

  CHECK(ro::sim::WorldHolds(world, kPlayer));
  // LA SECONDE TABLE RÉPOND AUSSI, et c'est tout le propos : sans elle, un effet
  // accroché à son unité tomberait dès le premier passage.
  CHECK(ro::sim::WorldHolds(world, kUnit));
  CHECK_FALSE(ro::sim::WorldHolds(world, kPersonne));

  // ⚠ ET LES DEUX TABLES RESTENT DISTINCTES : une unité n'est pas un acteur, et
  // le code qui parcourt les acteurs ne doit pas la voir apparaître.
  CHECK_FALSE(world.actors().contains(kUnit));
  CHECK(world.skill_units().contains(kUnit));
}

TEST_CASE("Monde : l'unité levée cesse d'être tenue", "[sim][world]") {
  ro::sim::World world;
  world.PlaceSkillUnit(GroundUnit());
  REQUIRE(ro::sim::WorldHolds(world, kUnit));

  // 🔴 LE SERVEUR LÈVE L'UNITÉ PAR LES DEUX BOUTS — sa durée qui s'achève, son
  // compte de coups qui s'épuise — et les deux passent par ce seul retrait. Ce
  // qu'elle portait doit partir avec elle, sans quoi un nœud de dix mille tics
  // reste sur une case vide pendant deux minutes et demie.
  CHECK(world.RemoveSkillUnit(kUnit));
  CHECK_FALSE(ro::sim::WorldHolds(world, kUnit));
  // Retirer ce qui n'est plus là ne ment pas : le même paquet sert à retirer
  // d'autres choses, et l'appelant lit cette réponse.
  CHECK_FALSE(world.RemoveSkillUnit(kUnit));
}
