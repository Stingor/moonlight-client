#include "world/deferred_walk.h"

#include <cstdint>
#include <optional>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/strike_play.h"
#include "ro/sim/actor.h"
#include "ro/sim/combat_notice.h"
#include "ro/sim/pending_action.h"
#include "ro/sim/world.h"

using moonclient::DeferredWalk;
using moonclient::GroundWalkDue;
using moonclient::ReleasedWalk;
using moonclient::StandsOn;
using ro::render::Motion;
using ro::sim::ApproachWalkClock;
using ro::sim::CellPosition;
using ro::sim::World;

namespace {

constexpr std::uint32_t kPlayerId = 150'001;
constexpr std::uint32_t kSwingStartMs = 1000;
constexpr std::uint32_t kSecondSwingMs = 1600;

const moonclient::PlayerGesture kSwing{Motion::kAttack, ro::render::kStrikeAttackState};
const moonclient::PlayerGesture kVariantSwing{Motion::kAttack,
                                              ro::render::kStrikeAttackVariantState};
const moonclient::PlayerGesture kResting{Motion::kIdle, std::nullopt};
const moonclient::PlayerGesture kWalking{Motion::kWalk, std::nullopt};
const moonclient::PlayerGesture kSitting{Motion::kSit, std::nullopt};
const moonclient::PlayerGesture kPickingUp{Motion::kPickup, std::nullopt};

World SwingingPlayer(std::uint32_t swing_started_ms = kSwingStartMs) {
  World world;
  world.set_player_id(kPlayerId);
  ro::sim::Actor player;
  player.id = kPlayerId;
  player.kind = ro::sim::ActorKind::kPlayer;
  player.position = CellPosition{50, 50, 0};
  player.attack = ro::sim::Strike{.attacker = kPlayerId, .started_ms = swing_started_ms};
  world.Spawn(player);
  return world;
}

}  // namespace

TEST_CASE("Marche retenue : un clic au sol pendant un coup attend la fin du coup", "[app][walk]") {
  World world = SwingingPlayer();
  ApproachWalkClock clock;
  DeferredWalk deferred;
  const CellPosition clicked{60, 50, 0};

  // Le coup en cours retient la marche : rien ne part, la case est gardée.
  CHECK_FALSE(
      GroundWalkDue(world, clock, &deferred, kSwing, clicked, kSwingStartMs + 100).has_value());
  REQUIRE(deferred.destination.has_value());
  CHECK(deferred.destination->x == 60);
  // Tant que le même coup se joue, elle reste gardée.
  CHECK_FALSE(ReleasedWalk(world, clock, deferred, kSwing, kSwingStartMs + 200));
  CHECK(deferred.destination.has_value());

  // Le coup fini, elle part une seule fois.
  const auto released = ReleasedWalk(world, clock, deferred, kResting, kSwingStartMs + 500);
  REQUIRE(released.has_value());
  CHECK(released->x == 60);
  CHECK_FALSE(deferred.destination.has_value());
  CHECK_FALSE(ReleasedWalk(world, clock, deferred, kResting, kSwingStartMs + 900));
}

TEST_CASE("Marche retenue : le coup suivant la libère aussi", "[app][walk]") {
  World world = SwingingPlayer();
  ApproachWalkClock clock;
  DeferredWalk deferred;
  CHECK_FALSE(
      GroundWalkDue(world, clock, &deferred, kSwing, CellPosition{60, 50, 0}, kSwingStartMs + 100)
          .has_value());

  // Un second coup commence : la motion est la même, mais elle a recommencé.
  const World next_swing = SwingingPlayer(kSecondSwingMs);
  CHECK(ReleasedWalk(next_swing, clock, deferred, kSwing, kSecondSwingMs).has_value());
}

TEST_CASE("Marche retenue : hors des trois gestes, le clic part — ou tombe sous le plancher",
          "[app][walk]") {
  World world = SwingingPlayer();
  ApproachWalkClock clock;
  DeferredWalk deferred;
  deferred.destination = CellPosition{1, 1, 0};

  // Un clic au sol hors d'un geste retire la case gardée, en partant lui-même.
  CHECK(
      GroundWalkDue(world, clock, &deferred, kResting, CellPosition{60, 50, 0}, 5000).has_value());
  CHECK_FALSE(deferred.destination.has_value());
  // Trop tôt après : perdu, et rien n'est gardé.
  CHECK_FALSE(
      GroundWalkDue(world, clock, &deferred, kWalking, CellPosition{61, 50, 0}, 5050).has_value());
  CHECK_FALSE(deferred.destination.has_value());
  // Assis, il se garde.
  CHECK_FALSE(
      GroundWalkDue(world, clock, &deferred, kSitting, CellPosition{62, 50, 0}, 5300).has_value());
  CHECK(deferred.destination.has_value());
}

TEST_CASE("Marche retenue : changer de carte l'oublie", "[app][walk]") {
  World world = SwingingPlayer();
  ApproachWalkClock clock;
  DeferredWalk deferred;
  CHECK_FALSE(
      GroundWalkDue(world, clock, &deferred, kPickingUp, CellPosition{60, 50, 0}, 100).has_value());
  world.EnterMap("prontera", 10, 10);
  CHECK_FALSE(ReleasedWalk(world, clock, deferred, kResting, 900).has_value());
  CHECK_FALSE(deferred.destination.has_value());
}

TEST_CASE("Marche retenue : la variante d'attaque ne retient rien", "[app][walk]") {
  // Seul l'état deux retient : un coup d'état neuf laisse partir le clic.
  World world = SwingingPlayer();
  ApproachWalkClock clock;
  DeferredWalk deferred;
  CHECK(GroundWalkDue(world, clock, &deferred, kVariantSwing, CellPosition{60, 50, 0},
                      kSwingStartMs + 100)
            .has_value());
  CHECK_FALSE(deferred.destination.has_value());

  // Et l'attaque ordinaire qui cède la place à la variante libère ce qu'elle
  // retenait : c'est un changement de geste.
  CHECK_FALSE(
      GroundWalkDue(world, clock, &deferred, kSwing, CellPosition{61, 50, 0}, kSwingStartMs + 300)
          .has_value());
  CHECK(ReleasedWalk(world, clock, deferred, kVariantSwing, kSwingStartMs + 400).has_value());
}

TEST_CASE("Marche : la case où l'on se tient se reconnaît, et l'appui y part quand même",
          "[app][walk]") {
  World world = SwingingPlayer();
  CHECK(StandsOn(world, CellPosition{50, 50, 0}));
  CHECK_FALSE(StandsOn(world, CellPosition{51, 50, 0}));

  // L'appui sur ses propres pieds pendant un coup est retenu, puis rendu : c'est
  // ce qui arrête une attaque continue.
  ApproachWalkClock clock;
  DeferredWalk deferred;
  CHECK_FALSE(
      GroundWalkDue(world, clock, &deferred, kSwing, CellPosition{50, 50, 0}, kSwingStartMs + 100)
          .has_value());
  REQUIRE(deferred.destination.has_value());
  CHECK(ReleasedWalk(world, clock, deferred, kResting, kSwingStartMs + 900).has_value());
}
