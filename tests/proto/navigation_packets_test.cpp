// La demande de navigation du serveur : sa disposition sur le fil, et surtout
// sa présence dans la table des longueurs — sans elle, le découpage du flux
// ferme la session à la première réception.

#include "ro/proto/navigation_packets.h"

#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/proto/packet_lengths.h"

using ro::proto::Decode;
using ro::proto::Encode;
using ro::proto::NavigationActive;
using ro::proto::PacketLength;

namespace {

// Vingt-sept octets écrits à la main, dans l'ordre que le serveur les pose.
std::vector<std::uint8_t> HandWritten() {
  std::vector<std::uint8_t> bytes = {0xE2, 0x08,  // opcode
                                     0x00,        // type : une cellule
                                     0x65,        // drapeau : 101
                                     0x01};       // fenêtre masquée
  const char map[] = "prontera.gat";
  for (std::size_t index = 0; index < NavigationActive::kMapNameWidth; ++index) {
    bytes.push_back(index < sizeof(map) - 1 ? static_cast<std::uint8_t>(map[index]) : 0);
  }
  const std::vector<std::uint8_t> tail = {0x9B, 0x00,   // x = 155
                                          0xB4, 0x00,   // y = 180
                                          0x34, 0x12};  // monstre = 0x1234
  bytes.insert(bytes.end(), tail.begin(), tail.end());
  return bytes;
}

}  // namespace

TEST_CASE("Navigation (proto) : la table des longueurs connaît l'opcode", "[proto][navigation]") {
  const auto length = PacketLength(NavigationActive::kOpcode);
  REQUIRE(length.has_value());
  CHECK(*length == static_cast<std::int16_t>(NavigationActive::kWireSize));
}

TEST_CASE("Navigation (proto) : les octets du serveur se lisent champ par champ",
          "[proto][navigation]") {
  const std::vector<std::uint8_t> bytes = HandWritten();
  REQUIRE(bytes.size() == NavigationActive::kWireSize);

  NavigationActive packet;
  REQUIRE(Decode(bytes, packet));
  CHECK(packet.type == 0);
  CHECK(packet.flag == 101);
  CHECK(packet.hide_window == 1);
  CHECK(packet.map == "prontera.gat");
  CHECK(packet.x == 155);
  CHECK(packet.y == 180);
  CHECK(packet.mob_id == 0x1234);
}

TEST_CASE("Navigation (proto) : l'encodage rend les octets écrits à la main",
          "[proto][navigation]") {
  NavigationActive packet;
  REQUIRE(Decode(HandWritten(), packet));
  CHECK(Encode(packet) == HandWritten());
}

TEST_CASE("Navigation (proto) : une trame d'une autre taille ou d'un autre opcode est refusée",
          "[proto][navigation]") {
  NavigationActive packet;
  std::vector<std::uint8_t> short_frame = HandWritten();
  short_frame.pop_back();
  CHECK_FALSE(Decode(short_frame, packet));

  std::vector<std::uint8_t> long_frame = HandWritten();
  long_frame.push_back(0);
  CHECK_FALSE(Decode(long_frame, packet));

  std::vector<std::uint8_t> other = HandWritten();
  other[0] = 0xE3;
  CHECK_FALSE(Decode(other, packet));
}

TEST_CASE("Navigation (proto) : un nom de carte qui remplit les seize octets se lit entier",
          "[proto][navigation]") {
  NavigationActive packet;
  packet.map = "0123456789abcdef";
  NavigationActive read;
  REQUIRE(Decode(Encode(packet), read));
  CHECK(read.map == "0123456789abcdef");
}
