// LA QUESTION DU NOMBRE DE JOUEURS : son opcode seul, recoupé avec ce que le
// serveur déclare lire.

#include "ro/proto/user_count_packet.h"

#include <cstddef>
#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/proto/packet_lengths.h"

using ro::proto::UserCountRequest;

TEST_CASE("Compte des joueurs : la longueur est celle du serveur", "[proto][who]") {
  const auto length = ro::proto::PacketLength(UserCountRequest::kOpcode);
  REQUIRE(length.has_value());
  CHECK(static_cast<std::size_t>(*length) == UserCountRequest::kWireSize);
}

TEST_CASE("Compte des joueurs : CZ_REQ_USER_COUNT n'est que son opcode", "[proto][who]") {
  const auto frame = ro::proto::EncodeUserCountRequest();
  REQUIRE(frame.has_value());
  CHECK(*frame == std::vector<std::uint8_t>{0xC1, 0x00});
}
