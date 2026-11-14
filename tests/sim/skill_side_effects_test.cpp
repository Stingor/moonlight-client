#include "ro/sim/skill_side_effects.h"

#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/combat_notice.h"
#include "ro/sim/world.h"

using ro::sim::Actor;
using ro::sim::ActorKind;
using ro::sim::CellPosition;
using ro::sim::World;

namespace {

Actor Someone(std::uint32_t id, CellPosition where) {
  Actor actor;
  actor.id = id;
  actor.kind = ActorKind::kPlayer;
  actor.position = where;
  actor.name = "quelqu'un";
  return actor;
}

}  // namespace

TEST_CASE("Monde : Auto Counter pose sa barre sous le seul joueur local", "[sim][world]") {
  World world;
  world.set_player_id(170'031);
  world.Spawn(Someone(170'031, CellPosition{10, 10, 0}));
  world.Spawn(Someone(170'032, CellPosition{12, 10, 0}));
  ro::sim::SupportHit hit;
  hit.skill_id = ro::sim::kAutoCounterSkill;
  hit.amount = 5;
  hit.started_ms = 7'000;
  hit.source_id = 170'031;
  // 🔴 UN EMPLOI MANQUÉ NE LA POSE PAS : le client ne consulte sa compétence
  // qu'après un succès.
  world.RecordSupport(170'031, hit);
  CHECK_FALSE(world.Find(170'031)->gauge.has_value());
  hit.succeeded = true;
  world.RecordSupport(170'031, hit);
  REQUIRE(world.Find(170'031)->gauge.has_value());
  CHECK(world.Find(170'031)->gauge->started_ms == 7'000);
  CHECK(world.Find(170'031)->gauge->duration_ms == 2'000);
  // Un autre lanceur : le client la pose puis la retire aussitôt.
  hit.source_id = 170'032;
  world.RecordSupport(170'032, hit);
  CHECK_FALSE(world.Find(170'032)->gauge.has_value());
}

TEST_CASE("Monde : l'impact d'Asura arme une pose que l'arrêt suivant lève", "[sim][world]") {
  World world;
  world.Spawn(Someone(170'041, CellPosition{10, 10, 0}));
  world.Spawn(Someone(170'042, CellPosition{12, 10, 0}));
  ro::sim::Strike strike;
  strike.attacker = 170'041;
  strike.target = 170'042;
  strike.skill_id = ro::sim::kArrivalPoseSkill;
  strike.type = 8;
  strike.started_ms = 9'000;
  world.RecordStrike(strike);
  CHECK(world.Find(170'041)->arrival_pose_armed);
  // Le glissement n'est pas une arrivée ; l'arrêt, si.
  world.PlaceAt(170'041, CellPosition{13, 10, 0});
  CHECK_FALSE(world.Find(170'041)->arrival_pose_since_ms.has_value());
  world.PlaceAt(170'041, CellPosition{13, 10, 0}, 9'020);
  CHECK_FALSE(world.Find(170'041)->arrival_pose_armed);
  REQUIRE(world.Find(170'041)->arrival_pose_since_ms.has_value());
  CHECK(*world.Find(170'041)->arrival_pose_since_ms == 9'020);
  // Une sorte que l'impact n'arme pas.
  strike.type = 5;
  world.RecordStrike(strike);
  CHECK_FALSE(world.Find(170'041)->arrival_pose_armed);
}

TEST_CASE("Monde : un monstre passe son niveau à la bannière, non sa cible", "[sim][world]") {
  World world;
  Actor caster = Someone(170'021, CellPosition{10, 10, 0});
  caster.kind = ActorKind::kMonster;
  world.Spawn(caster);
  constexpr std::uint16_t kWeFemale = 335;
  constexpr std::uint32_t kPartner = 170'022;
  constexpr std::uint32_t kLevel = 3;
  REQUIRE(world.AnnounceSkill(170'021, kWeFemale, 4'000, kPartner, kLevel));
  CHECK(world.Find(170'021)->announced_target_id == kLevel);
}
