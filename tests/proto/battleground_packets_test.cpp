// LES PAQUETS DU CHAMP DE BATAILLE écrits à la main par le serveur : leurs
// tailles, leurs champs, la parole et son en-tête.

#include "ro/proto/battleground_packets.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/proto/chat_limits.h"
#include "ro/proto/manual_packets.h"
#include "ro/proto/packet_lengths.h"

#include "support/wire_bytes.h"

using ro::proto::BattlefieldCampNotice;
using ro::proto::BattlefieldChatNotice;
using ro::proto::BattlefieldChatRequest;
using ro::proto::BattlefieldPositionNotice;
using ro::proto::BattlefieldScoreNotice;
using ro::proto::Decode;
using ro::proto::Encode;
using ro::proto::LobbyAdmissionAck;
using ro::test_support::ReadU16;

namespace {

constexpr std::uint32_t kSpeaker = 2'000'321;
constexpr std::uint32_t kAccount = 2'000'777;
constexpr std::uint16_t kCampA = 3;
constexpr std::uint16_t kCampB = 4;
constexpr std::uint16_t kLionScore = 2;
constexpr std::uint16_t kEagleScore = 11;
constexpr std::uint16_t kKnightJob = 7;
constexpr std::int16_t kCellX = 152;
constexpr std::int16_t kCellY = 87;
constexpr std::uint8_t kAdmissionGranted = 1;
// L'opcode et la longueur, devant le texte de la parole émise.
constexpr std::size_t kChatRequestHeader = 4;

[[nodiscard]] BattlefieldChatNotice Chat(std::string message) {
  BattlefieldChatNotice notice;
  notice.speaker_id = kSpeaker;
  notice.speaker_name = "Aldric";
  notice.message = std::move(message);
  return notice;
}

}  // namespace

TEST_CASE("Champ de bataille : les tailles fixes sont celles du serveur", "[proto][battleground]") {
  STATIC_REQUIRE(BattlefieldScoreNotice::kWireSize == 6);
  STATIC_REQUIRE(BattlefieldPositionNotice::kWireSize == 36);
  const std::array<std::pair<std::uint16_t, std::size_t>, 4> fixed = {{
      {BattlefieldCampNotice::kOpcode, BattlefieldCampNotice::kWireSize},
      {BattlefieldScoreNotice::kOpcode, BattlefieldScoreNotice::kWireSize},
      {BattlefieldPositionNotice::kOpcode, BattlefieldPositionNotice::kWireSize},
      {LobbyAdmissionAck::kOpcode, LobbyAdmissionAck::kWireSize},
  }};
  for (const auto& [opcode, size] : fixed) {
    const std::optional<std::int16_t> declared = ro::proto::PacketLength(opcode);
    REQUIRE(declared.has_value());
    CHECK(static_cast<std::size_t>(*declared) == size);
  }
  // Les deux paroles ont une longueur variable.
  CHECK(ro::proto::PacketLength(BattlefieldChatNotice::kOpcode) == ro::proto::kVariableLength);
  CHECK(ro::proto::PacketLength(BattlefieldChatRequest::kOpcode) == ro::proto::kVariableLength);
}

TEST_CASE("Champ de bataille : le camp se relit champ par champ", "[proto][battleground]") {
  BattlefieldCampNotice sent;
  sent.actor_id = kAccount;
  sent.name = "Aldric";
  sent.camp = kCampB;
  const std::vector<std::uint8_t> frame = Encode(sent);
  REQUIRE(frame.size() == BattlefieldCampNotice::kWireSize);
  CHECK(ReadU16(frame, 30) == kCampB);
  BattlefieldCampNotice read;
  REQUIRE(Decode(frame, read));
  CHECK(read.actor_id == kAccount);
  CHECK(read.name == "Aldric");
  CHECK(read.camp == kCampB);
  // Une trame tronquée, ou d'un autre opcode, ne se lit pas.
  const std::vector<std::uint8_t> short_frame(frame.begin(), frame.end() - 1);
  CHECK_FALSE(Decode(short_frame, read));
  std::vector<std::uint8_t> other = frame;
  other[0] = 0xDE;
  CHECK_FALSE(Decode(other, read));
}

TEST_CASE("Champ de bataille : le score se relit lion d'abord", "[proto][battleground]") {
  // 02DE, lion 2, aigle 11 — petit-boutiste.
  const std::vector<std::uint8_t> wire = {0xDE, 0x02, 0x02, 0x00, 0x0B, 0x00};
  BattlefieldScoreNotice read;
  REQUIRE(Decode(wire, read));
  CHECK(read.lion == kLionScore);
  CHECK(read.eagle == kEagleScore);
  CHECK(Encode(read) == wire);
  const std::vector<std::uint8_t> longer = {0xDE, 0x02, 0x02, 0x00, 0x0B, 0x00, 0x00};
  CHECK_FALSE(Decode(longer, read));
}

TEST_CASE("Champ de bataille : la position d'un coéquipier fait l'aller-retour",
          "[proto][battleground]") {
  BattlefieldPositionNotice sent;
  sent.account_id = kAccount;
  sent.name = "Brunhild";
  sent.job = kKnightJob;
  sent.x = kCellX;
  sent.y = kCellY;
  const std::vector<std::uint8_t> frame = Encode(sent);
  REQUIRE(frame.size() == BattlefieldPositionNotice::kWireSize);
  BattlefieldPositionNotice read;
  REQUIRE(Decode(frame, read));
  CHECK(read.account_id == kAccount);
  CHECK(read.name == "Brunhild");
  CHECK(read.job == kKnightJob);
  CHECK(read.x == kCellX);
  CHECK(read.y == kCellY);
}

TEST_CASE("Champ de bataille : le retrait d'un coéquipier se lit en −1", "[proto][battleground]") {
  // Ce que le serveur écrit au départ d'un coéquipier : nom vide, classe 0,
  // et 0xFFFF dans les deux coordonnées.
  // 02DF, le compte 2000777 en petit-boutiste, puis les 24 octets du nom et
  // la classe à zéro.
  std::vector<std::uint8_t> wire(BattlefieldPositionNotice::kWireSize, 0);
  wire[0] = 0xDF;
  wire[1] = 0x02;
  wire[2] = 0x89;
  wire[3] = 0x87;
  wire[4] = 0x1E;
  wire[5] = 0x00;
  for (std::size_t at = 32; at < BattlefieldPositionNotice::kWireSize; ++at) {
    wire[at] = 0xFF;
  }
  BattlefieldPositionNotice read;
  REQUIRE(Decode(wire, read));
  CHECK(read.account_id == kAccount);
  CHECK(read.name.empty());
  CHECK(read.job == 0);
  CHECK(read.x == -1);
  CHECK(read.y == -1);
}

TEST_CASE("Champ de bataille : la réponse d'entrée porte deux noms", "[proto][battleground]") {
  LobbyAdmissionAck sent;
  sent.result = kAdmissionGranted;
  sent.field_name = "bat_a01";
  sent.lobby_name = "bat_a01";
  const std::vector<std::uint8_t> frame = Encode(sent);
  REQUIRE(frame.size() == LobbyAdmissionAck::kWireSize);
  CHECK(frame[2] == kAdmissionGranted);
  LobbyAdmissionAck read;
  REQUIRE(Decode(frame, read));
  CHECK(read.result == kAdmissionGranted);
  CHECK(read.field_name == "bat_a01");
  CHECK(read.lobby_name == "bat_a01");
}

TEST_CASE("Champ de bataille : la parole reçue perd son zéro, pas son texte",
          "[proto][battleground]") {
  const std::string said = "Aldric : à l'attaque";
  const std::vector<std::uint8_t> frame = Encode(Chat(said));
  // La longueur compte l'en-tête, le texte et son zéro.
  REQUIRE(frame.size() == BattlefieldChatNotice::kHeaderSize + said.size() + 1);
  CHECK(ReadU16(frame, 2) == frame.size());
  CHECK(frame.back() == 0);
  BattlefieldChatNotice read;
  REQUIRE(Decode(frame, read));
  CHECK(read.speaker_id == kSpeaker);
  CHECK(read.speaker_name == "Aldric");
  CHECK(read.message == said);
}

TEST_CASE("Champ de bataille : la parole du serveur vient de l'orateur zéro",
          "[proto][battleground]") {
  BattlefieldChatNotice sent = Chat("Server : Lions win!");
  sent.speaker_id = 0;
  sent.speaker_name = "Server";
  BattlefieldChatNotice read;
  REQUIRE(Decode(Encode(sent), read));
  CHECK(read.speaker_id == 0);
  CHECK(read.message == "Server : Lions win!");
}

TEST_CASE("Champ de bataille : une parole à la longueur incohérente est refusée",
          "[proto][battleground]") {
  std::vector<std::uint8_t> frame = Encode(Chat("Aldric : tenez"));
  BattlefieldChatNotice read;
  SECTION("longueur annoncée trop grande") {
    frame[2] = static_cast<std::uint8_t>(frame[2] + 1);
    CHECK_FALSE(Decode(frame, read));
  }
  SECTION("longueur annoncée trop petite") {
    frame[2] = static_cast<std::uint8_t>(frame[2] - 1);
    CHECK_FALSE(Decode(frame, read));
  }
  SECTION("trame plus courte que l'en-tête") {
    frame.resize(BattlefieldChatNotice::kHeaderSize - 1);
    frame[2] = static_cast<std::uint8_t>(frame.size());
    frame[3] = 0;
    CHECK_FALSE(Decode(frame, read));
  }
}

TEST_CASE("Champ de bataille : la parole émise est préfixée du nom", "[proto][battleground]") {
  BattlefieldChatRequest request;
  request.speaker = "Aldric";
  request.text = "on tient le pont";
  const auto frame = Encode(request);
  REQUIRE(frame.has_value());
  const std::string expected = "Aldric : on tient le pont";
  REQUIRE(frame->size() == kChatRequestHeader + expected.size() + 1);
  CHECK(ReadU16(*frame, 0) == BattlefieldChatRequest::kOpcode);
  CHECK(ReadU16(*frame, 2) == frame->size());
  const std::string body(frame->begin() + kChatRequestHeader, frame->end() - 1);
  CHECK(body == expected);
  CHECK(frame->back() == 0);
}

TEST_CASE("Champ de bataille : la parole émise refuse un orateur vide et un texte trop long",
          "[proto][battleground]") {
  BattlefieldChatRequest request;
  request.text = "personne";
  CHECK_FALSE(Encode(request).has_value());
  request.speaker = "Aldric";
  request.text = std::string(ro::proto::kChatTextMax + 1, 'a');
  CHECK_FALSE(Encode(request).has_value());
  // « Nom : texte » ne doit pas dépasser ce que le relais diffuse, même quand
  // le texte seul passerait.
  const std::size_t prefix_bytes =
      request.speaker.size() + ro::proto::ChatRequest::kNameSeparator.size();
  request.text = std::string(ro::proto::kChatNamedBodyMax - prefix_bytes + 1, 'a');
  REQUIRE(request.text.size() <= ro::proto::kChatTextMax);
  CHECK_FALSE(Encode(request).has_value());
  request.text.pop_back();
  CHECK(Encode(request).has_value());
}
