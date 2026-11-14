#include <catch2/catch_test_macros.hpp>

#include "ro/net/world_update.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/movement.h"
#include "ro/sim/packed_position.h"
#include "ro/sim/world.h"

#include "support/net_frames.h"

using ro::net::ApplyFrame;
using ro::net::FrameOutcome;
using ro::sim::CellMovement;
using ro::sim::CellPosition;
using ro::sim::World;
using ro::test_support::FrameOf;
using ro::test_support::IdleUnit;

TEST_CASE("Traduction : le recul jette l'acteur sur sa case, assis compris", "[net][update]") {
  // Le serveur émet ce paquet en paire avec l'arrêt, et le client n'y construit
  // aucun chemin : il écrit la position telle quelle. Une posture ne change
  // donc pas, et le regard reste où il était.
  World world;
  REQUIRE(ApplyFrame(FrameOf(Encode(IdleUnit(150'001, CellPosition{150, 100, 6}))), world, 0) ==
          FrameOutcome::kApplied);

  ro::proto::ZcNotifyAct sit;
  sit.srcID = 150'001;
  sit.type = 2;  // s'asseoir
  REQUIRE(ApplyFrame(FrameOf(Encode(sit)), world, 0) == FrameOutcome::kApplied);
  REQUIRE(world.Find(150'001)->posture == ro::sim::Posture::kSitting);

  ro::proto::ZcHighjump blown;
  blown.srcId = 150'001;
  blown.x = 147;
  blown.y = 103;
  CHECK(ApplyFrame(FrameOf(Encode(blown)), world, 0) == FrameOutcome::kApplied);

  const auto* actor = world.Find(150'001);
  REQUIRE(actor != nullptr);
  CHECK(actor->position.x == 147);
  CHECK(actor->position.y == 103);
  CHECK(actor->position.direction == 6);
  CHECK(actor->posture == ro::sim::Posture::kSitting);
}

TEST_CASE("Traduction : le recul d'un inconnu n'invente personne", "[net][update]") {
  // Le paquet peut arriver pour un acteur dont l'apparition s'est perdue.
  World world;
  ro::proto::ZcHighjump blown;
  blown.srcId = 4'242;
  blown.x = 10;
  blown.y = 10;
  CHECK(ApplyFrame(FrameOf(Encode(blown)), world, 0) == FrameOutcome::kIgnored);
  CHECK(world.Find(4'242) == nullptr);
}

TEST_CASE("Traduction : le recul coupe la marche en cours", "[net][update]") {
  // Garder le déplacement ferait glisser l'acteur vers une destination qu'il
  // n'atteindra jamais : le recul l'a sorti de son chemin.
  World world;
  ro::proto::PacketUnitWalking walking;
  walking.AID = 150'001;
  walking.MoveData = ro::sim::PackMovement(
      CellMovement{CellPosition{150, 100, 0}, CellPosition{158, 104, 0}, 0, 0});
  REQUIRE(ApplyFrame(FrameOf(Encode(walking)), world, 0) == FrameOutcome::kApplied);
  REQUIRE(world.Find(150'001)->movement.has_value());

  ro::proto::ZcHighjump blown;
  blown.srcId = 150'001;
  blown.x = 149;
  blown.y = 98;
  CHECK(ApplyFrame(FrameOf(Encode(blown)), world, 0) == FrameOutcome::kApplied);
  CHECK(!world.Find(150'001)->movement.has_value());
}
