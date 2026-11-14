// LES ORDRES D'UN COMPAGNON PARTENT SUR LE FIL : aller à une case, attaquer,
// revenir au maître. Et le refus d'un de ses skills se lit au retour.

#include "ro/net/companion_orders.h"

#include <cstdint>
#include <optional>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/connection.h"
#include "ro/net/skill.h"
#include "ro/proto/companion_packets.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/packed_position.h"

#include "support/loopback_server.h"
#include "support/net_frames.h"

using ro::test::LoopbackServer;
using ro::test_support::FrameOf;

TEST_CASE("Ordre de compagnon : le refus d'un skill se lit dans 0x0110", "[net][companion]") {
  // L'opcode, le skill 8013, le type, l'objet, l'issue, la cause (le délai).
  constexpr std::uint16_t kRefusedSkill = 8013;
  constexpr std::uint8_t kFailed = 0;
  constexpr std::uint8_t kSucceeded = 1;
  const auto ack = [](std::uint8_t flag) {
    return FrameOf({0x10, 0x01, 0x4D, 0x1F, 0, 0, 0, 0, 0, 0, 0, 0, flag, 0x04});
  };
  CHECK(ro::net::RefusedSkillOf(ack(kFailed)) == kRefusedSkill);
  CHECK_FALSE(ro::net::RefusedSkillOf(ack(kSucceeded)).has_value());
  CHECK_FALSE(ro::net::RefusedSkillOf(FrameOf({0x34, 0x02, 0, 0, 0, 0})).has_value());
}

TEST_CASE("Ordre de compagnon : les trois ordres partent tels que le serveur les lit",
          "[net][companion]") {
  constexpr std::uint32_t kCompanionId = 0x01020304;
  constexpr std::uint32_t kTargetId = 0x0A0B0C0D;
  LoopbackServer server;
  REQUIRE(server.listening());
  auto link = ro::net::Connection::Open("127.0.0.1", server.port());
  REQUIRE(link.has_value());
  REQUIRE(server.Accept());

  const ro::sim::CellPosition cell{150, 200, 0};
  REQUIRE(ro::net::RequestCompanionMove(*link, kCompanionId, cell).has_value());
  const std::vector<std::uint8_t> moved = server.Receive(ro::proto::CzRequestMovenpc::kWireSize);
  // L'opcode, l'acteur, puis (150, 200) empaquetés comme le serveur les relit :
  // x = 0x25 << 2 | 0x8C >> 6 = 150, y = (0x8C & 0x3F) << 4 | 0x80 >> 4 = 200.
  CHECK(moved == std::vector<std::uint8_t>{0x32, 0x02, 0x04, 0x03, 0x02, 0x01, 0x25, 0x8C, 0x80});

  REQUIRE(ro::net::RequestCompanionAttack(*link, kCompanionId, kTargetId).has_value());
  CHECK(
      server.Receive(ro::proto::CompanionAttackRequest::kWireSize) ==
      std::vector<std::uint8_t>{0x33, 0x02, 0x04, 0x03, 0x02, 0x01, 0x0D, 0x0C, 0x0B, 0x0A, 0x00});

  REQUIRE(ro::net::RequestCompanionReturn(*link, kCompanionId).has_value());
  CHECK(server.Receive(ro::proto::CompanionReturnRequest::kWireSize) ==
        std::vector<std::uint8_t>{0x34, 0x02, 0x04, 0x03, 0x02, 0x01});
}
