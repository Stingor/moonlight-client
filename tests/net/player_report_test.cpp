// LE SIGNALEMENT D'UN JOUEUR sur le fil : la demande à la disposition du client
// d'origine, octet par octet, et l'accusé tel que le serveur l'écrit.

#include "ro/net/player_report.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/connection.h"
#include "ro/proto/map_packets.h"

#include "support/loopback_server.h"
#include "support/net_frames.h"
#include "support/wire_bytes.h"

using ro::net::PlayerReportReason;
using ro::test::LoopbackServer;
using ro::test_support::ReadU16;
using ro::test_support::ReadU32;

namespace {

constexpr std::uint32_t kReporter = 2000001;
constexpr std::uint32_t kTarget = 2000123;

// La disposition du client d'origine.
constexpr std::size_t kReporterAt = 2;
constexpr std::size_t kTargetAt = 6;
constexpr std::size_t kNameAt = 10;
constexpr std::size_t kNameBytes = 24;
constexpr std::size_t kReasonAt = 34;
constexpr std::size_t kMessageAt = 36;
constexpr std::size_t kWireBytes = 137;
constexpr std::size_t kAckWireBytes = 34;

[[nodiscard]] std::vector<std::uint8_t> Sent(const ro::net::PlayerReport& report) {
  LoopbackServer server;
  REQUIRE(server.listening());
  auto link = ro::net::Connection::Open("127.0.0.1", server.port());
  REQUIRE(link.has_value());
  REQUIRE(server.Accept());
  REQUIRE(ro::net::SendPlayerReport(*link, report).has_value());
  return server.Receive(kWireBytes);
}

[[nodiscard]] std::vector<std::uint8_t> Ack(std::int32_t result) {
  ro::proto::ZcAckReportUser packet;
  packet.targetAID = kTarget;
  packet.targetName = "Bob";
  packet.result = result;
  return ro::proto::Encode(packet);
}

}  // namespace

TEST_CASE("Signalement : la demande suit la disposition du client d'origine",
          "[net][player_report]") {
  const std::vector<std::uint8_t> sent = Sent({.reporter_aid = kReporter,
                                               .target_aid = kTarget,
                                               .target_name = "Bob",
                                               .reason = PlayerReportReason::kBugAbuse,
                                               .message = "Il traverse les murs."});
  REQUIRE(sent.size() == kWireBytes);
  CHECK(ReadU16(sent, 0) == 0x0BE2);
  CHECK(ReadU32(sent, kReporterAt) == kReporter);
  CHECK(ReadU32(sent, kTargetAt) == kTarget);
  CHECK(std::string(reinterpret_cast<const char*>(&sent[kNameAt])) == "Bob");
  CHECK(ReadU16(sent, kReasonAt) == 1);
  CHECK(std::string(reinterpret_cast<const char*>(&sent[kMessageAt])) == "Il traverse les murs.");
}

TEST_CASE("Signalement : le texte part en CP1252, coupé à cent caractères, zéro final gardé",
          "[net][player_report]") {
  // Cent vingt « é » : deux octets chacun en UTF-8, un seul en CP1252.
  std::string long_text;
  constexpr std::size_t kTyped = 120;
  for (std::size_t typed = 0; typed < kTyped; ++typed) {
    long_text += "é";
  }
  // Un nom trop long aussi : le client d'origine en garde vingt-trois.
  const std::vector<std::uint8_t> sent = Sent({.reporter_aid = kReporter,
                                               .target_aid = kTarget,
                                               .target_name = std::string(30, 'n'),
                                               .reason = PlayerReportReason::kIllegalProgram,
                                               .message = long_text});
  REQUIRE(sent.size() == kWireBytes);
  CHECK(ReadU16(sent, kReasonAt) == 0);
  constexpr std::uint8_t kCp1252EAcute = 0xE9;
  for (std::size_t at = 0; at < ro::net::kPlayerReportMessageMostBytes; ++at) {
    CHECK(sent[kMessageAt + at] == kCp1252EAcute);
  }
  CHECK(sent[kMessageAt + ro::net::kPlayerReportMessageMostBytes] == 0);
  CHECK(sent[kNameAt + ro::net::kPlayerReportNameMostBytes] == 0);
  CHECK(kNameAt + kNameBytes == kReasonAt);
}

TEST_CASE("Signalement : l'accusé se lit, et rien d'autre ne passe pour lui",
          "[net][player_report]") {
  const auto ack = ro::net::ReadPlayerReportAck(ro::test_support::FrameOf(Ack(1)));
  REQUIRE(ack.has_value());
  CHECK(ack->target_aid == kTarget);
  CHECK(ack->target_name == "Bob");
  CHECK(ack->result == 1);
  CHECK(Ack(1).size() == kAckWireBytes);

  // Une trame tronquée ne se lit pas.
  std::vector<std::uint8_t> short_frame = Ack(1);
  short_frame.pop_back();
  CHECK_FALSE(ro::net::DecodePlayerReportAck(short_frame).has_value());

  // Un autre opcode n'est pas un accusé.
  ro::proto::ZcMoonlightChatBlocks other;
  CHECK_FALSE(ro::net::ReadPlayerReportAck(ro::test_support::FrameOf(ro::proto::Encode(other)))
                  .has_value());
}
