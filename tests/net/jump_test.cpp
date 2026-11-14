// LE SAUT COSMÉTIQUE SUR LE FIL : 0x0F1A part, 0x0F1B fait sauter les autres.

#include "ro/net/jump.h"

#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/connection.h"
#include "ro/net/world_update.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/actor.h"
#include "ro/sim/jump.h"
#include "ro/sim/world.h"

#include "support/loopback_link.h"
#include "support/loopback_server.h"
#include "support/net_frames.h"
#include "support/wire_bytes.h"

using ro::net::ApplyFrame;
using ro::net::Connection;
using ro::net::FrameOutcome;
using ro::net::JumpPlayer;
using ro::sim::kJumpDurationMs;
using ro::test::Connect;
using ro::test::LoopbackServer;
using ro::test_support::FrameOf;
using ro::test_support::ReadU16;

namespace {

constexpr std::uint32_t kSelfId = 2'000'001;
constexpr std::uint32_t kNeighbourId = 2'000'002;
constexpr std::uint32_t kJumpMs = 9'000;

void SpawnPlayer(ro::sim::World& world, std::uint32_t id) {
  ro::sim::Actor player;
  player.id = id;
  player.kind = ro::sim::ActorKind::kPlayer;
  player.position = ro::sim::CellPosition{40, 50, 0};
  world.Spawn(player);
}

[[nodiscard]] FrameOutcome Notify(ro::sim::World& world, std::uint32_t gid) {
  ro::proto::ZcMoonlightJump notice;
  notice.gid = gid;
  return ApplyFrame(FrameOf(ro::proto::Encode(notice)), world, kJumpMs);
}

}  // namespace

TEST_CASE("Saut : un voisin qui saute s'élève chez nous", "[net][jump]") {
  // 🔴 LA TRAME PASSE PAR L'AIGUILLAGE ORDINAIRE : sans son traducteur, le
  // voisin restait au sol.
  ro::sim::World world;
  world.set_player_id(kSelfId);
  SpawnPlayer(world, kSelfId);
  SpawnPlayer(world, kNeighbourId);

  CHECK(Notify(world, kNeighbourId) == FrameOutcome::kApplied);
  CHECK(ro::sim::Airborne(*world.Find(kNeighbourId), kJumpMs + kJumpDurationMs / 2));
}

TEST_CASE("Saut : notre propre compte revenu du fil est ignoré", "[net][jump]") {
  ro::sim::World world;
  world.set_player_id(kSelfId);
  SpawnPlayer(world, kSelfId);

  CHECK(Notify(world, kSelfId) == FrameOutcome::kIgnored);
  CHECK_FALSE(world.Find(kSelfId)->jump_since_ms.has_value());
}

TEST_CASE("Saut : la demande part en quatre octets et pose le saut tout de suite", "[net][jump]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  Connection link = Connect(server);
  ro::sim::World world;
  world.set_player_id(kSelfId);
  SpawnPlayer(world, kSelfId);

  REQUIRE(JumpPlayer(link, world, kJumpMs).has_value());
  const std::vector<std::uint8_t> bytes = server.Receive(ro::proto::CzMoonlightJump::kWireSize);
  REQUIRE(bytes.size() == ro::proto::CzMoonlightJump::kWireSize);
  CHECK(ReadU16(bytes, 0) == ro::proto::CzMoonlightJump::kOpcode);
  // ⚠ LA LONGUEUR S'ÉCRIT : le serveur la lit, et zéro ferait rejeter la trame.
  CHECK(ReadU16(bytes, 2) == ro::proto::CzMoonlightJump::kWireSize);
  // 🔴 LE SERVEUR NE RÉPOND PAS À L'ENVOYEUR : le saut est déjà dans le monde.
  CHECK(ro::sim::Airborne(*world.Find(kSelfId), kJumpMs + 1));
}

TEST_CASE("Saut : en l'air, la touche ne fait rien partir", "[net][jump]") {
  // Le serveur jetterait ce saut-là : les voisins verraient un saut de moins
  // que nous.
  LoopbackServer server;
  REQUIRE(server.listening());
  Connection link = Connect(server);
  ro::sim::World world;
  world.set_player_id(kSelfId);
  SpawnPlayer(world, kSelfId);

  REQUIRE(JumpPlayer(link, world, kJumpMs).has_value());
  REQUIRE(JumpPlayer(link, world, kJumpMs + kJumpDurationMs / 2).has_value());
  REQUIRE(JumpPlayer(link, world, kJumpMs + kJumpDurationMs).has_value());
  // Deux demandes et non trois : la seconde est restée chez nous.
  const std::vector<std::uint8_t> bytes =
      server.Receive(ro::proto::CzMoonlightJump::kWireSize * 3, /*timeout_ms=*/300);
  CHECK(bytes.size() == ro::proto::CzMoonlightJump::kWireSize * 2);
}
