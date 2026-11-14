#include "ro/net/attack_reach.h"

#include <cstdint>
#include <optional>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/world_update.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/movement.h"
#include "ro/sim/world.h"

#include "support/net_frames.h"

using ro::net::ApplyFrame;
using ro::net::FrameOutcome;
using ro::sim::CellPosition;
using ro::sim::World;
using ro::test_support::FrameOf;
using ro::test_support::IdleUnit;

namespace {

constexpr std::uint32_t kPlayerId = 150'001;
constexpr std::uint32_t kMonsterId = 300'001;

// Le refus tel que `clif_movetoattack` l'écrit : la cible, SA case, la NÔTRE,
// et la portée de l'arme.
[[nodiscard]] ro::proto::ZcAttackFailureForDistance TooFar(std::int16_t target_x,
                                                           std::int16_t target_y,
                                                           std::int16_t self_x, std::int16_t self_y,
                                                           std::int16_t range) {
  ro::proto::ZcAttackFailureForDistance told;
  told.targetAID = kMonsterId;
  told.targetXPos = target_x;
  told.targetYPos = target_y;
  told.xPos = self_x;
  told.yPos = self_y;
  told.currentAttRange = range;
  return told;
}

[[nodiscard]] World TwoActors() {
  World world;
  world.set_player_id(kPlayerId);
  REQUIRE(ApplyFrame(FrameOf(Encode(IdleUnit(kPlayerId, CellPosition{2, 8, 0}))), world, 0) ==
          FrameOutcome::kApplied);
  REQUIRE(ApplyFrame(FrameOf(Encode(IdleUnit(kMonsterId, CellPosition{12, 8, 0}))), world, 0) ==
          FrameOutcome::kApplied);
  return world;
}

}  // namespace

TEST_CASE("Portée : le refus du serveur recale les deux positions et note la portée",
          "[net][attack]") {
  World world = TwoActors();
  // Les deux acteurs ont bougé sans qu'on le sache : le paquet dit où ils sont.
  CHECK(ApplyFrame(FrameOf(Encode(TooFar(14, 9, 3, 7, 2))), world, 0) == FrameOutcome::kApplied);

  CHECK(world.player_status().attack_range == 2);
  const ro::sim::Actor* self = world.player();
  REQUIRE(self != nullptr);
  CHECK(self->position.x == 3);
  CHECK(self->position.y == 7);
  const ro::sim::Actor* target = world.Find(kMonsterId);
  REQUIRE(target != nullptr);
  CHECK(target->position.x == 14);
  CHECK(target->position.y == 9);
}

TEST_CASE("Portée : le refus nomme la cible dont il faut reprendre la poursuite", "[net][attack]") {
  // 🔴 CE N'EST PAS UN FAIT DU MONDE : le serveur ne sait rien de ce que le
  // client attend. La trame le dit, et `app` en fait ce qu'elle veut.
  CHECK(ro::net::TargetTooFar(FrameOf(Encode(TooFar(12, 8, 2, 8, 1)))) == kMonsterId);
}

TEST_CASE("Portée : une autre trame ne se lit pas comme un refus", "[net][attack]") {
  CHECK(ro::net::TargetTooFar(FrameOf(Encode(IdleUnit(kMonsterId, CellPosition{12, 8, 0})))) ==
        std::nullopt);
  // Une trame du bon opcode mais tronquée ne nomme personne : le décodeur
  // refuse la disposition, et inventer une cible ferait courir après zéro.
  ro::net::Frame ecourtee = FrameOf(Encode(TooFar(12, 8, 2, 8, 1)));
  ecourtee.bytes.pop_back();
  CHECK(ro::net::TargetTooFar(ecourtee) == std::nullopt);
}

TEST_CASE("Portée : une cible qu'on ne voit pas n'invente pas d'acteur", "[net][attack]") {
  World world = TwoActors();
  ro::proto::ZcAttackFailureForDistance told = TooFar(12, 8, 2, 8, 1);
  told.targetAID = 999'999;
  REQUIRE(ApplyFrame(FrameOf(Encode(told)), world, 0) == FrameOutcome::kApplied);
  CHECK(world.Find(999'999) == nullptr);
  // Notre propre position est recalée quand même : le paquet la porte, et elle
  // ne dépend pas de la cible.
  const ro::sim::Actor* self = world.player();
  REQUIRE(self != nullptr);
  CHECK(self->position.x == 2);
}

TEST_CASE("Portée : le recalage laisse marcher qui marche encore", "[net][attack]") {
  // 🔴 LE MESSAGE 61 NE BÂTIT PAS DE CHEMIN ET N'ARRÊTE PERSONNE : la cible qui
  // s'éloigne garde sa marche. Nous la figions sur la case du refus.
  World world = TwoActors();
  REQUIRE(world.SetSpeed(kMonsterId, 150));
  ro::sim::CellMovement away;
  away.from = CellPosition{12, 8, 0};
  away.to = CellPosition{20, 8, 0};
  REQUIRE(world.StartMoving(kMonsterId, away, 1'000));
  REQUIRE(ApplyFrame(FrameOf(Encode(TooFar(13, 8, 2, 8, 1))), world, 1'200) ==
          FrameOutcome::kApplied);
  const ro::sim::Actor* target = world.Find(kMonsterId);
  REQUIRE(target != nullptr);
  REQUIRE(target->movement.has_value());
  CHECK(target->movement->to.x == 20);
}

TEST_CASE("Portée : le recalage garde la marche achevée que le repos quitte", "[net][attack]") {
  World world = TwoActors();
  REQUIRE(world.SetSpeed(kPlayerId, 150));
  ro::sim::CellMovement walk;
  walk.from = CellPosition{2, 8, 0};
  walk.to = CellPosition{4, 8, 0};
  REQUIRE(world.StartMoving(kPlayerId, walk, 1'000));
  REQUIRE(ApplyFrame(FrameOf(Encode(TooFar(12, 8, 5, 9, 1))), world, 9'000) ==
          FrameOutcome::kApplied);
  const ro::sim::Actor* self = world.player();
  REQUIRE(self != nullptr);
  // Recalé sans arrêt : la marche reste connue, ramenée sur la case annoncée.
  REQUIRE(self->movement.has_value());
  CHECK(self->movement_started_ms == 1'000);
  const ro::sim::FractionalCell cell = ro::sim::InterpolatedCell(*self, 9'000);
  CHECK(cell.x == 5.0F);
  CHECK(cell.y == 9.0F);
}
