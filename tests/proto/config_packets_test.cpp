// LES RÉGLAGES DU PERSONNAGE SUR LE FIL, octet par octet.
//
// 🔴 UNE DISPOSITION FAUSSE NE SE VOIT PAS EN JOUANT : le serveur lit le réglage
// et sa valeur à des décalages fixes, et un décalage glissé change un AUTRE
// réglage — l'appel d'urgence, les costumes — sans rien dire.

#include "ro/proto/config_packets.h"

#include <array>
#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/proto/manual_packets.h"
#include "ro/proto/packet_lengths.h"

using ro::proto::ConfigAck;
using ro::proto::ConfigRequest;
using ro::proto::ConfigType;
using ro::proto::DecodeConfigAck;
using ro::proto::EncodeConfigAck;

TEST_CASE("Réglages : la demande porte l'opcode, le réglage puis sa valeur",
          "[proto][show_equip]") {
  const auto frame =
      ro::proto::Encode(ConfigRequest{.type = ConfigType::kShowEquipment, .value = 1});
  REQUIRE(frame.has_value());
  const std::vector<std::uint8_t> expected = {0xD8, 0x02, 0x00, 0x00, 0x00,
                                              0x00, 0x01, 0x00, 0x00, 0x00};
  CHECK(*frame == expected);
}

TEST_CASE("Réglages : la disposition s'accorde avec ce que le serveur déclare lire",
          "[proto][show_equip]") {
  // Le réglage en +2, la valeur en +6 : ce que la table du serveur déclare.
  constexpr std::array<std::uint16_t, 2> kDeclared = {2, 6};
  CHECK(ro::proto::MatchesServerLayout(ConfigRequest::kOpcode, "clif_parse_configuration",
                                       kDeclared));
  // Et une dérive d'un octet se voit.
  constexpr std::array<std::uint16_t, 2> kDrifted = {2, 7};
  CHECK_FALSE(
      ro::proto::MatchesServerLayout(ConfigRequest::kOpcode, "clif_parse_configuration", kDrifted));
  CHECK(ro::proto::PacketLength(ConfigRequest::kOpcode) == 10);
  CHECK(ro::proto::PacketLength(ConfigAck::kOpcode) == 10);
}

TEST_CASE("Réglages : l'écho se lit, et une trame de travers se refuse", "[proto][show_equip]") {
  const std::vector<std::uint8_t> bytes = EncodeConfigAck(ConfigAck{.type = 5, .value = 1});
  const auto ack = DecodeConfigAck(bytes);
  REQUIRE(ack.has_value());
  CHECK(ack->type == 5);
  CHECK(ack->value == 1);

  std::vector<std::uint8_t> short_by_one = bytes;
  short_by_one.pop_back();
  CHECK_FALSE(DecodeConfigAck(short_by_one).has_value());

  // Même longueur, autre paquet : la demande elle-même.
  std::vector<std::uint8_t> other = bytes;
  other[0] = 0xD8;
  CHECK_FALSE(DecodeConfigAck(other).has_value());
}
