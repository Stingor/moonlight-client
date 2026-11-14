#include "ro/net/zone_move.h"

#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/world_update.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/world.h"

#include "support/net_frames.h"

using ro::net::ApplyFrame;
using ro::net::FrameOutcome;
using ro::net::ServerMoveOf;
using ro::net::ZoneTicket;
using ro::sim::World;
using ro::test_support::FrameOf;

namespace {

constexpr std::uint32_t kCharacterId = 150'123;
// 192.168.1.20, tel que le fil le porte : ordre réseau lu en petit-boutiste.
constexpr std::uint32_t kWireIp = 0x1401'A8C0U;
constexpr std::uint16_t kZonePort = 5'122;
constexpr std::uint16_t kArrivalX = 156;
constexpr std::uint16_t kArrivalY = 191;

ro::proto::ZcNpcackServermove MoveToPayon() {
  ro::proto::ZcNpcackServermove move;
  move.mapName = "payon.gat";
  move.xPos = kArrivalX;
  move.yPos = kArrivalY;
  move.ip = kWireIp;
  move.port = kZonePort;
  return move;
}

}  // namespace

TEST_CASE("Serveur de zone : la trame de passage dit où aller", "[net][zone]") {
  const auto handoff = ServerMoveOf(FrameOf(Encode(MoveToPayon())), kCharacterId);
  REQUIRE(handoff.has_value());
  CHECK(handoff->character_id == kCharacterId);
  // Le tronc commun, comme partout ailleurs : sans « .gat ».
  CHECK(handoff->map_name == "payon");
  CHECK(handoff->address.host() == "192.168.1.20");
  CHECK(handoff->address.port == kZonePort);
}

TEST_CASE("Serveur de zone : le monde entre sur la carte annoncée", "[net][zone]") {
  // 🔴 LA TRAME FINISSAIT EN « NON TRAITÉE » : le monde restait sur l'ancienne
  // carte, sur une liaison que le serveur ne servait plus.
  World world;
  const std::uint32_t entries_before = world.map_entries();
  CHECK(ApplyFrame(FrameOf(Encode(MoveToPayon())), world, 0) == FrameOutcome::kApplied);
  CHECK(world.map_entries() == entries_before + 1);
  CHECK(world.map_name() == "payon");
}

TEST_CASE("Serveur de zone : une autre trame n'est pas un passage", "[net][zone]") {
  ro::proto::ZcNpcackMapmove same_server;
  same_server.mapName = "payon.gat";
  CHECK_FALSE(ServerMoveOf(FrameOf(Encode(same_server)), kCharacterId).has_value());
}

TEST_CASE("Serveur de zone : un billet sans compte ni personnage ne sert pas", "[net][zone]") {
  CHECK_FALSE(ZoneTicket().usable());
  ZoneTicket ticket;
  ticket.account.account_id = 2'000'001;
  CHECK_FALSE(ticket.usable());
  ticket.character_id = kCharacterId;
  CHECK(ticket.usable());
}
