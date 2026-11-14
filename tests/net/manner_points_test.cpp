// LES POINTS DE MANIÈRE ET LE RETRAIT DE L'ÉQUIPEMENT : ce qui part, octet par
// octet, et ce que les réponses écrivent dans la conversation.
//
// 🔴 UN SENS INVERSÉ NE SE VOIT PAS DE L'ÉMETTEUR : le serveur répond « appliqué »
// à un bon comme à un mauvais point, et c'est la cible qui se retrouve muette au
// lieu d'être libérée. Le banc relit le sens tel qu'il part.

#include "ro/net/manner_points.h"

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/core/byte_writer.h"
#include "ro/net/connection.h"
#include "ro/net/staff_requests.h"
#include "ro/proto/map_packets.h"
#include "ro/proto/staff_packets.h"
#include "ro/sim/chat_line.h"
#include "ro/sim/world.h"

#include "support/loopback_link.h"
#include "support/loopback_server.h"
#include "support/net_frames.h"
#include "support/wire_bytes.h"

using ro::net::ApplyMannerPointFrame;
using ro::net::FrameOutcome;
using ro::net::MannerPointWay;
using ro::test::Connect;
using ro::test::LoopbackServer;
using ro::test_support::FrameOf;
using ro::test_support::ReadU16;
using ro::test_support::ReadU32;

namespace {

constexpr std::uint32_t kTarget = 2000123;
constexpr std::uint32_t kNowMs = 42'000;
constexpr int kMinutes = 90;
// Les champs de la demande, tels que le serveur les lit.
constexpr std::size_t kAccountAt = 2;
constexpr std::size_t kWayAt = 6;
constexpr std::size_t kMinutesAt = 7;
// Le verdict, son opcode, et ses numéros tels que le serveur les donne.
constexpr std::uint16_t kVerdictOpcode = 0x014A;
constexpr std::uint32_t kAligned = 0;
constexpr std::uint32_t kBlockedByStaff = 3;
// Celui que la cible reçoit à chaque point : rien ne s'écrit.
constexpr std::uint32_t kTargetNotice = 5;
constexpr std::uint8_t kGoodWay = 0;
constexpr std::uint8_t kBadWay = 1;
constexpr std::uint32_t kCyan = 0x00FFFF;
constexpr std::uint32_t kYellow = 0xFFFF00;

// La trame d'un verdict : l'opcode, puis le numéro sur quatre octets.
[[nodiscard]] std::vector<std::uint8_t> Verdict(std::uint32_t verdict) {
  std::vector<std::uint8_t> bytes;
  ro::core::ByteWriter writer(bytes);
  writer.U16(kVerdictOpcode);
  writer.U32(verdict);
  return bytes;
}

[[nodiscard]] std::vector<std::uint8_t> Given(std::uint8_t way, const char* author) {
  ro::proto::ZcNotifyMannerPointGiven packet;
  packet.type = way;
  packet.name = author;
  return ro::proto::Encode(packet);
}

}  // namespace

TEST_CASE("Points de manière : le compte, le sens puis les minutes", "[net][entity_menu]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  ro::net::Connection link = Connect(server);
  REQUIRE(ro::net::RequestMannerPoint(link, kTarget, MannerPointWay::kBad, kMinutes).has_value());
  const std::vector<std::uint8_t> bad = server.Receive(ro::proto::MannerPointRequest::kWireSize);
  REQUIRE(bad.size() == ro::proto::MannerPointRequest::kWireSize);
  CHECK(ReadU16(bad, 0) == 0x0149);
  CHECK(ReadU32(bad, kAccountAt) == kTarget);
  CHECK(bad[kWayAt] == kBadWay);
  CHECK(ReadU16(bad, kMinutesAt) == kMinutes);

  REQUIRE(ro::net::RequestMannerPoint(link, kTarget, MannerPointWay::kGood, kMinutes).has_value());
  const std::vector<std::uint8_t> good = server.Receive(ro::proto::MannerPointRequest::kWireSize);
  REQUIRE(good.size() == ro::proto::MannerPointRequest::kWireSize);
  CHECK(good[kWayAt] == kGoodWay);
}

TEST_CASE("Points de manière : les minutes se bornent avant de partir", "[net][entity_menu]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  ro::net::Connection link = Connect(server);
  REQUIRE(ro::net::RequestMannerPoint(link, kTarget, MannerPointWay::kBad,
                                      ro::net::kMannerPointsMostMinutes + 1)
              .has_value());
  const std::vector<std::uint8_t> high = server.Receive(ro::proto::MannerPointRequest::kWireSize);
  REQUIRE(high.size() == ro::proto::MannerPointRequest::kWireSize);
  CHECK(ReadU16(high, kMinutesAt) == ro::net::kMannerPointsMostMinutes);

  REQUIRE(ro::net::RequestMannerPoint(link, kTarget, MannerPointWay::kBad, -kMinutes).has_value());
  const std::vector<std::uint8_t> low = server.Receive(ro::proto::MannerPointRequest::kWireSize);
  REQUIRE(low.size() == ro::proto::MannerPointRequest::kWireSize);
  CHECK(ReadU16(low, kMinutesAt) == 0);
}

TEST_CASE("Retirer tout l'équipement : 0x07F5, puis le compte", "[net][entity_menu]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  ro::net::Connection link = Connect(server);
  REQUIRE(ro::net::RequestFullStrip(link, kTarget).has_value());
  const std::vector<std::uint8_t> sent = server.Receive(ro::proto::FullStripRequest::kWireSize);
  REQUIRE(sent.size() == ro::proto::FullStripRequest::kWireSize);
  CHECK(ReadU16(sent, 0) == 0x07F5);
  CHECK(ReadU32(sent, kAccountAt) == kTarget);
}

TEST_CASE("Points de manière : le verdict s'écrit, sauf celui que la cible reçoit toujours",
          "[net][entity_menu]") {
  ro::sim::World world;
  CHECK(ApplyMannerPointFrame(FrameOf(Verdict(kAligned)), world, kNowMs) == FrameOutcome::kApplied);
  REQUIRE(world.chat_lines().size() == 1);
  CHECK(world.chat_lines().back().text == "Le point de manière a été appliqué.");
  CHECK(world.chat_lines().back().color == kCyan);

  CHECK(ApplyMannerPointFrame(FrameOf(Verdict(kBlockedByStaff)), world, kNowMs) ==
        FrameOutcome::kApplied);
  REQUIRE(world.chat_lines().size() == 2);
  CHECK(world.chat_lines().back().color == kYellow);

  CHECK(ApplyMannerPointFrame(FrameOf(Verdict(kTargetNotice)), world, kNowMs) ==
        FrameOutcome::kIgnored);
  CHECK(world.chat_lines().size() == 2);
}

TEST_CASE("Points de manière : la cible apprend de qui vient un bon point, pas un mauvais",
          "[net][entity_menu]") {
  ro::sim::World world;
  CHECK(ApplyMannerPointFrame(FrameOf(Given(kGoodWay, "Modo")), world, kNowMs) ==
        FrameOutcome::kApplied);
  REQUIRE(world.chat_lines().size() == 1);
  CHECK(world.chat_lines().back().text == "Bon point de manière reçu de Modo.");

  CHECK(ApplyMannerPointFrame(FrameOf(Given(kBadWay, "Modo")), world, kNowMs) ==
        FrameOutcome::kIgnored);
  CHECK(world.chat_lines().size() == 1);
}

TEST_CASE("Points de manière : une autre trame ne les regarde pas", "[net][entity_menu]") {
  ro::sim::World world;
  ro::proto::ZcNotifyTime other;
  CHECK(ApplyMannerPointFrame(FrameOf(ro::proto::Encode(other)), world, kNowMs) ==
        FrameOutcome::kUnhandled);
  // Un verdict tronqué ne se lit pas.
  std::vector<std::uint8_t> cut = Verdict(kAligned);
  cut.pop_back();
  CHECK(ApplyMannerPointFrame(FrameOf(std::move(cut)), world, kNowMs) ==
        FrameOutcome::kUndecodable);
  CHECK(world.chat_lines().empty());
}
