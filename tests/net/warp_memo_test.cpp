// LA MÉMORISATION D'UN POINT DE WARP PORTAL, sur le fil : la demande, octet
// pour octet, et les deux paquets par lesquels le serveur répond.
//
// 🔴 LES RÉPONSES SONT BÂTIES PAR L'ENCODEUR ENGENDRÉ DE LA DISPOSITION DU
// SERVEUR : ce banc croise notre lecture avec la structure que le serveur
// compile.

#include "ro/net/warp_memo.h"

#include <cstdint>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/connection.h"
#include "ro/proto/manual_packets.h"
#include "ro/proto/map_packets.h"
#include "ro/proto/packet_lengths.h"
#include "ro/proto/parsed_packets.h"

#include "support/loopback_server.h"

using ro::net::Frame;
using ro::test::LoopbackServer;

namespace {

// Les codes du serveur (`e_ack_remember_warppoint_result`, puis
// `e_notify_mapinfo_result`).
constexpr std::uint8_t kAckSaved = 0;
constexpr std::uint8_t kAckSkillTooLow = 1;
constexpr std::uint8_t kAckSkillNotLearned = 2;
constexpr std::uint8_t kAckCannotSaveHere = 3;
constexpr std::int16_t kMapInfoTeleportForbidden = 0;
constexpr std::int16_t kMapInfoMemoForbidden = 1;

[[nodiscard]] Frame AckFrame(std::uint8_t type) {
  ro::proto::ZcAckRememberWarppoint packet;
  packet.type = type;
  return Frame{.opcode = ro::proto::ZcAckRememberWarppoint::kOpcode,
               .bytes = ro::proto::Encode(packet)};
}

[[nodiscard]] Frame MapInfoFrame(std::int16_t type) {
  ro::proto::ZcNotifyMapinfo packet;
  packet.type = type;
  return Frame{.opcode = ro::proto::ZcNotifyMapinfo::kOpcode, .bytes = ro::proto::Encode(packet)};
}

}  // namespace

TEST_CASE("Mémoriser : la demande est l'opcode 0x011D et rien d'autre", "[net][memo]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  auto link = ro::net::Connection::Open("127.0.0.1", server.port());
  REQUIRE(link.has_value());
  REQUIRE(server.Accept());

  REQUIRE(ro::net::RequestWarpMemo(*link).has_value());
  const std::vector<std::uint8_t> received = server.Receive(2);
  REQUIRE(received.size() == 2);
  CHECK(received[0] == 0x1D);
  CHECK(received[1] == 0x01);
}

TEST_CASE("Mémoriser : le serveur lit la demande sous ce nom, sans champ", "[net][memo]") {
  const auto declared = ro::proto::ParsedPacketFor(ro::proto::RememberWarpPointRequest::kOpcode);
  REQUIRE(declared.has_value());
  CHECK(declared->handler == "clif_parse_RequestMemo");
  const auto length = ro::proto::PacketLength(ro::proto::RememberWarpPointRequest::kOpcode);
  REQUIRE(length.has_value());
  CHECK(static_cast<std::size_t>(*length) == ro::proto::RememberWarpPointRequest::kWireSize);
  // La réponse fait trois octets : l'opcode et un code.
  const auto ack_length = ro::proto::PacketLength(ro::proto::ZcAckRememberWarppoint::kOpcode);
  REQUIRE(ack_length.has_value());
  CHECK(static_cast<std::size_t>(*ack_length) == ro::proto::ZcAckRememberWarppoint::kWireSize);
}

TEST_CASE("Mémoriser : le code de la réponse se relit tel que le serveur l'écrit", "[net][memo]") {
  for (const std::uint8_t code : {kAckSaved, kAckSkillTooLow, kAckSkillNotLearned}) {
    const auto read = ro::net::ReadWarpMemoAck(AckFrame(code));
    REQUIRE(read.has_value());
    CHECK(*read == code);
  }
  // Un code que ce serveur n'émet pas passe tel quel : c'est `app` qui décide
  // de son sens, ou de son silence.
  const auto beyond = ro::net::ReadWarpMemoAck(AckFrame(kAckCannotSaveHere));
  REQUIRE(beyond.has_value());
  CHECK(*beyond == kAckCannotSaveHere);
}

TEST_CASE("Mémoriser : une trame qui n'est pas la réponse, ou tronquée, ne se lit pas",
          "[net][memo]") {
  // Le paquet du refus de carte n'est pas l'accusé.
  CHECK_FALSE(ro::net::ReadWarpMemoAck(MapInfoFrame(kMapInfoMemoForbidden)).has_value());
  Frame truncated = AckFrame(kAckSaved);
  truncated.bytes.pop_back();
  CHECK_FALSE(ro::net::ReadWarpMemoAck(truncated).has_value());
}

TEST_CASE("Mémoriser : le refus de la carte se relit, et son code est signé", "[net][memo]") {
  const auto forbidden = ro::net::ReadMapInfoNotice(MapInfoFrame(kMapInfoMemoForbidden));
  REQUIRE(forbidden.has_value());
  CHECK(*forbidden == kMapInfoMemoForbidden);
  // Deux octets sur le fil : un code négatif ne se confond pas avec un grand.
  const auto negative = ro::net::ReadMapInfoNotice(MapInfoFrame(-1));
  REQUIRE(negative.has_value());
  CHECK(*negative == -1);
  CHECK_FALSE(ro::net::ReadMapInfoNotice(AckFrame(0)).has_value());
  Frame truncated = MapInfoFrame(kMapInfoTeleportForbidden);
  truncated.bytes.pop_back();
  CHECK_FALSE(ro::net::ReadMapInfoNotice(truncated).has_value());
}
