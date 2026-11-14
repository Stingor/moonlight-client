// LE RAPPORT DE BUG SUR LE FIL : la trame, octet pour octet, contrôlée contre ce
// que le serveur déclare lire, et son accusé.

#include "ro/net/bug_report.h"

#include <cstdint>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/connection.h"
#include "ro/proto/manual_packets.h"
#include "ro/proto/map_packets.h"
#include "ro/proto/parsed_packets.h"
#include "ro/sim/bank.h"

#include "support/loopback_link.h"
#include "support/loopback_server.h"

using ro::test::LoopbackServer;

namespace {

[[nodiscard]] ro::net::Connection ConnectToListening(LoopbackServer& server) {
  REQUIRE(server.listening());
  return ro::test::Connect(server);
}

}  // namespace

TEST_CASE("Rapport de bug : le serveur le lit sous 0x0F13, en longueur variable",
          "[net][bugreport]") {
  const auto declared = ro::proto::ParsedPacketFor(ro::proto::BugReportRequest::kOpcode);
  REQUIRE(declared.has_value());
  CHECK(declared->handler == "clif_parse_moonlight_bug_report");
}

TEST_CASE("Rapport de bug : la trame, en CP1252, octet pour octet", "[net][bugreport]") {
  LoopbackServer server;
  ro::net::Connection link = ConnectToListening(server);
  // « é » : deux octets en UTF-8, un seul sur le fil.
  REQUIRE(ro::net::SendBugReport(link, 1, "{\"n\":\"\u00e9\"}", "\u00e9!").has_value());
  const std::string context = "{\"n\":\"\xE9\"}";
  const std::vector<std::uint8_t> head = server.Receive(7);
  REQUIRE(head.size() == 7);
  const std::size_t length = 7 + context.size() + 2;
  CHECK(head[0] == 0x13);
  CHECK(head[1] == 0x0F);
  CHECK(head[2] == static_cast<std::uint8_t>(length));
  CHECK(head[3] == 0);
  CHECK(head[4] == 1);
  CHECK(head[5] == static_cast<std::uint8_t>(context.size()));
  CHECK(head[6] == 0);
  const std::vector<std::uint8_t> tail = server.Receive(context.size() + 2);
  REQUIRE(tail.size() == context.size() + 2);
  CHECK(tail[context.size() - 3] == 0xE9);  // le « é » du contexte
  CHECK(tail[context.size()] == 0xE9);
  CHECK(tail[context.size() + 1] == '!');
}

TEST_CASE("Rapport de bug : le message se coupe à ce que la base garde", "[net][bugreport]") {
  LoopbackServer server;
  ro::net::Connection link = ConnectToListening(server);
  const std::string long_message(ro::net::kBugReportMessageMostBytes + 40, 'a');
  REQUIRE(ro::net::SendBugReport(link, 0, "{}", long_message).has_value());
  const std::size_t expected = 7 + 2 + ro::net::kBugReportMessageMostBytes;
  CHECK(server.Receive(expected).size() == expected);
}

TEST_CASE("Rapport de bug : l'accusé du serveur", "[net][bugreport]") {
  const std::vector<std::uint8_t> bytes =
      ro::proto::Encode(ro::proto::ZcMoonlightBugReportAck{.status = 1});
  const ro::net::Frame frame{.opcode = ro::proto::ZcMoonlightBugReportAck::kOpcode, .bytes = bytes};
  REQUIRE(ro::net::ReadBugReportAck(frame).has_value());
  CHECK(*ro::net::ReadBugReportAck(frame) == 1);
  const ro::net::Frame other{.opcode = 0x0F05, .bytes = bytes};
  CHECK_FALSE(ro::net::ReadBugReportAck(other).has_value());
}
