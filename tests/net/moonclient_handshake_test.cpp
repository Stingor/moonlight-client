// La poignée de main d'intégrité, éprouvée sur un VRAI socket.
//
// Ce qui se vérifie ici hors ligne, c'est la seule chose qui compte pour le
// serveur : les soixante et un octets qu'il va lire, aux décalages qu'il
// déclare. Une disposition fausse ne se verrait pas autrement — le serveur ne
// répond rien à un paquet qu'il n'a pas su lire, et la session serait coupée
// vingt secondes plus tard sans un mot.

#include "ro/net/moonclient_handshake.h"

#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/core/build_stamp.h"
#include "ro/net/connection.h"

#include "support/loopback_link.h"
#include "support/loopback_server.h"

using ro::net::ClientIdentity;
using ro::net::Connection;
using ro::net::MoonclientHandshake;
using ro::net::MoonclientHandshakeState;
using ro::test::Connect;
using ro::test::LoopbackServer;

namespace {

// La disposition déclarée par le serveur, `PACKET_CZ_MOONCLIENT_INTEGRITY` :
//   [packetType:2][packetLength:2][machine_guid:36][patch_index:4]
//   [commit:16][tree_modified:1]
constexpr std::size_t kOpcodeAt = 0;
constexpr std::size_t kLengthAt = 2;
constexpr std::size_t kGuidAt = 4;
constexpr std::size_t kPatchIndexAt = 40;
constexpr std::size_t kCommitAt = 44;
constexpr std::size_t kTreeModifiedAt = 60;
constexpr std::size_t kWireSize = 61;

constexpr std::uint16_t kIntegrityOpcode = 0x0F39;
constexpr std::uint16_t kSettingsOpcode = 0x0F05;
constexpr std::uint16_t kKickNoticeOpcode = 0x0F03;

// Un GUID de la bonne longueur, qui n'est celui d'aucune machine.
constexpr std::string_view kGuidForTest = "01234567-89ab-cdef-0123-456789abcdef";
// Une révision abrégée, plus courte que son champ.
constexpr std::string_view kCommitForTest = "0123456789ab";

ClientIdentity Declared(std::int32_t patch_index = 42) {
  ClientIdentity identity;
  identity.machine_guid = std::string(kGuidForTest);
  identity.patch_index = patch_index;
  identity.commit = std::string(kCommitForTest);
  identity.tree_modified = true;
  return identity;
}

[[nodiscard]] std::uint16_t U16At(const std::vector<std::uint8_t>& bytes, std::size_t at) {
  return static_cast<std::uint16_t>(bytes[at] | (static_cast<std::uint16_t>(bytes[at + 1]) << 8));
}

[[nodiscard]] std::int32_t I32At(const std::vector<std::uint8_t>& bytes, std::size_t at) {
  std::uint32_t value = 0;
  for (std::size_t index = 0; index < 4; ++index) {
    value |= static_cast<std::uint32_t>(bytes[at + index]) << (8 * index);
  }
  return static_cast<std::int32_t>(value);
}

// Une trame nue, telle que le serveur l'enverrait.
std::vector<std::uint8_t> ServerFrame(std::uint16_t opcode, std::uint16_t length) {
  std::vector<std::uint8_t> bytes(length, 0);
  bytes[0] = static_cast<std::uint8_t>(opcode & 0xFF);
  bytes[1] = static_cast<std::uint8_t>(opcode >> 8);
  bytes[2] = static_cast<std::uint8_t>(length & 0xFF);
  bytes[3] = static_cast<std::uint8_t>(length >> 8);
  return bytes;
}

}  // namespace

TEST_CASE("la déclaration a la disposition que le serveur lit", "[net][handshake]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  Connection link = Connect(server);

  MoonclientHandshake handshake(Declared());
  handshake.Pump(link, 0);

  const std::vector<std::uint8_t> sent = server.Receive(kWireSize);
  REQUIRE(sent.size() == kWireSize);
  CHECK(U16At(sent, kOpcodeAt) == kIntegrityOpcode);
  CHECK(U16At(sent, kLengthAt) == kWireSize);

  // ⚠ TRENTE-SIX CARACTÈRES ET PAS DE ZÉRO FINAL : le serveur lit ce champ par sa
  // longueur, et un octet de terminaison mangerait le premier du suivant.
  const std::string guid(reinterpret_cast<const char*>(sent.data() + kGuidAt),
                         ro::net::kMachineGuidChars);
  CHECK(guid == kGuidForTest);
  CHECK(I32At(sent, kPatchIndexAt) == 42);

  // La révision, complétée de zéros jusqu'au bout de son champ : le serveur la
  // lit comme une chaîne terminée.
  const std::string commit(reinterpret_cast<const char*>(sent.data() + kCommitAt),
                           kCommitForTest.size());
  CHECK(commit == kCommitForTest);
  for (std::size_t at = kCommitAt + kCommitForTest.size(); at < kTreeModifiedAt; ++at) {
    CHECK(sent[at] == 0);
  }
  CHECK(sent[kTreeModifiedAt] == 1);
}

TEST_CASE("la révision déclarée est celle gravée dans le binaire", "[net][handshake]") {
  const ClientIdentity identity = ro::net::ClientIdentityOf(std::nullopt);
  CHECK(identity.commit == ro::core::kBuildCommit.substr(0, ro::net::kCommitChars));
  CHECK(identity.tree_modified == (ro::core::kBuildTree != ro::core::kBuildTreeClean));
}

TEST_CASE("l'index de patch dit ce que le patcheur a posé", "[net][handshake]") {
  // Jamais patchée : la valeur que le serveur lit comme telle, et non zéro, qui
  // serait un premier patch appliqué.
  CHECK(ro::net::ClientIdentityOf(std::nullopt).patch_index == ro::net::kNeverPatched);
  CHECK(ro::net::ClientIdentityOf(std::uint64_t{0}).patch_index == 0);
  CHECK(ro::net::ClientIdentityOf(std::uint64_t{57}).patch_index == 57);

  // Trop grand pour le champ : plafonné. Tronqué, il deviendrait négatif, et le
  // serveur couperait une installation à jour pour contenu périmé.
  constexpr std::int32_t kLargest = std::numeric_limits<std::int32_t>::max();
  const auto beyond = static_cast<std::uint64_t>(kLargest) + 1;
  CHECK(ro::net::ClientIdentityOf(beyond).patch_index == kLargest);
}

TEST_CASE("sans réponse, la poignée de main se rejoue puis renonce", "[net][handshake]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  Connection link = Connect(server);

  MoonclientHandshake handshake(Declared());
  handshake.Pump(link, 0);
  CHECK(handshake.state() == MoonclientHandshakeState::kAwaiting);
  CHECK(handshake.attempts() == 1);

  // AVANT le délai, rien : c'est ce qui sépare un renvoi d'une inondation.
  handshake.Pump(link, ro::net::kHandshakeRetryMs - 1);
  CHECK(handshake.attempts() == 1);

  handshake.Pump(link, ro::net::kHandshakeRetryMs);
  CHECK(handshake.attempts() == 2);

  // Jusqu'à la borne, puis plus rien — et l'état le DIT, au lieu de se taire.
  std::uint32_t now_ms = ro::net::kHandshakeRetryMs;
  for (int round = 0; round < ro::net::kHandshakeMaxAttempts + 2; ++round) {
    now_ms += ro::net::kHandshakeRetryMs;
    handshake.Pump(link, now_ms);
  }
  CHECK(handshake.attempts() == ro::net::kHandshakeMaxAttempts);
  CHECK(handshake.state() == MoonclientHandshakeState::kGivenUp);

  const std::vector<std::uint8_t> sent =
      server.Receive(kWireSize * static_cast<std::size_t>(ro::net::kHandshakeMaxAttempts));
  CHECK(sent.size() == kWireSize * static_cast<std::size_t>(ro::net::kHandshakeMaxAttempts));
}

TEST_CASE("les réglages du serveur valent accusé de réception", "[net][handshake]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  Connection link = Connect(server);

  MoonclientHandshake handshake(Declared());
  handshake.Pump(link, 0);
  REQUIRE(server.Receive(kWireSize).size() == kWireSize);

  // 🔴 LE SERVEUR N'ACQUITTE PAS : il envoie les réglages du joueur, et c'est
  // ce premier paquet personnalisé qui vaut accusé.
  handshake.NoteFrame(kSettingsOpcode);
  CHECK(handshake.state() == MoonclientHandshakeState::kVerified);

  handshake.Pump(link, ro::net::kHandshakeRetryMs * 10);
  CHECK(handshake.attempts() == 1);
}

TEST_CASE("l'avis de coupure se reconnaît, et arrête les renvois", "[net][handshake]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  Connection link = Connect(server);

  MoonclientHandshake handshake(Declared());
  handshake.Pump(link, 0);
  REQUIRE(server.Receive(kWireSize).size() == kWireSize);

  handshake.NoteFrame(kKickNoticeOpcode);
  CHECK(handshake.state() == MoonclientHandshakeState::kRefused);

  // Insister ne sert à rien : la coupure part d'un minuteur, pas d'une réponse.
  handshake.Pump(link, ro::net::kHandshakeRetryMs * 10);
  CHECK(handshake.attempts() == 1);
}

TEST_CASE("une trame ordinaire ne vaut pas accusé", "[net][handshake]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  Connection link = Connect(server);

  MoonclientHandshake handshake(Declared());
  handshake.Pump(link, 0);
  REQUIRE(server.Receive(kWireSize).size() == kWireSize);

  // Le monde continue d'arriver pendant l'attente ; rien de tout cela ne dit que
  // le serveur nous a reconnus.
  const std::vector<std::uint8_t> chat = ServerFrame(0x009A, 8);
  REQUIRE(server.Send(chat));
  handshake.NoteFrame(0x009A);
  CHECK(handshake.state() == MoonclientHandshakeState::kAwaiting);
}
