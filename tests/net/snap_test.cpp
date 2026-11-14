// LE BOND DE BODY RELOCATION : 0x08D2 fait glisser l'acteur vers sa case d'arrivée.

#include "ro/net/snap.h"

#include <algorithm>
#include <cstdint>
#include <optional>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/world_update.h"
#include "ro/proto/packet_lengths.h"
#include "ro/proto/snap_packets.h"
#include "ro/sim/actor.h"
#include "ro/sim/movement.h"
#include "ro/sim/world.h"

#include "support/net_frames.h"

using ro::net::ApplyFrame;
using ro::net::FrameOutcome;
using ro::test_support::FrameOf;

namespace {

constexpr std::uint32_t kMonkId = 150'001;
constexpr std::uint32_t kSnapMs = 4000;
constexpr std::uint16_t kWalkMsPerCell = 150;
constexpr std::uint16_t kSnapGlideMsPerCell = 15;
// Douze cases à quinze, diagonales comprises, tiennent sous trois cents.
constexpr std::uint32_t kGlideBoundMs = 300;
constexpr std::uint32_t kTrailEffect = 166;
// Cent tics de seize millisecondes.
constexpr std::uint32_t kTrailLifeMs = 1600;

}  // namespace

TEST_CASE("Snap : la trame a la longueur que le serveur déclare", "[net][snap]") {
  const std::optional<std::int16_t> declared =
      ro::proto::PacketLength(ro::proto::SnapNotice::kOpcode);
  REQUIRE(declared.has_value());
  CHECK(static_cast<std::size_t>(*declared) == ro::proto::SnapNotice::kWireSize);
}

TEST_CASE("Snap : le Monk glisse vers sa case d'arrivée, une traînée blanche derrière lui",
          "[net][snap]") {
  // 🔴 LA TRAME PASSE PAR L'AIGUILLAGE ORDINAIRE : sans son traducteur, le
  // bond n'arrivait nulle part, et le Monk restait sur place à l'écran.
  ro::sim::World world;
  ro::sim::Actor monk;
  monk.id = kMonkId;
  monk.kind = ro::sim::ActorKind::kPlayer;
  monk.speed_ms_per_cell = kWalkMsPerCell;
  monk.position = ro::sim::CellPosition{40, 50, 0};
  world.Spawn(monk);

  ro::proto::SnapNotice snap;
  snap.actor_id = kMonkId;
  snap.x = 52;
  snap.y = 58;
  CHECK(ApplyFrame(FrameOf(Encode(snap)), world, kSnapMs) == FrameOutcome::kApplied);
  const ro::sim::Actor& moved = *world.Find(kMonkId);
  REQUIRE(moved.movement.has_value());
  CHECK(moved.movement->to.x == 52);
  CHECK(moved.movement->to.y == 58);
  CHECK(moved.glide_ms_per_cell == kSnapGlideMsPerCell);
  // Douze cases à quinze millisecondes, et non à cent cinquante : le glissement
  // dure moins d'un tiers de seconde.
  CHECK(ro::sim::MovementDurationMs(moved) < kGlideBoundMs);
  CHECK(ro::sim::IsWalking(moved, kSnapMs + 1));
  CHECK_FALSE(ro::sim::IsWalking(moved, kSnapMs + kGlideBoundMs));

  // LA TRAÎNÉE : l'effet 166, blanc, qui s'éteint de lui-même après cent tics.
  const auto trail = std::ranges::find(moved.effects, kTrailEffect, &ro::sim::PlayedEffect::id);
  REQUIRE(trail != moved.effects.end());
  REQUIRE(trail->told_color.has_value());
  CHECK((*trail->told_color)[0] == 250.0F);
  world.ExpirePlayerStatuses(kSnapMs + kTrailLifeMs - 1);
  CHECK(std::ranges::find(world.Find(kMonkId)->effects, kTrailEffect, &ro::sim::PlayedEffect::id) !=
        world.Find(kMonkId)->effects.end());
  world.ExpirePlayerStatuses(kSnapMs + kTrailLifeMs);
  CHECK(std::ranges::find(world.Find(kMonkId)->effects, kTrailEffect, &ro::sim::PlayedEffect::id) ==
        world.Find(kMonkId)->effects.end());

  // UNE MARCHE NEUVE reprend sa propre vitesse.
  CHECK(world.StartMoving(kMonkId, ro::sim::CellMovement{.from = {52, 58, 0}, .to = {53, 58, 0}},
                          kSnapMs + kTrailLifeMs));
  CHECK(world.Find(kMonkId)->glide_ms_per_cell == 0);
}
