// L'ARRÊT D'UNE ATTAQUE : son opcode seul, recoupé avec ce que le serveur
// déclare lire.

#include "ro/proto/stop_attack_packet.h"

#include <cstddef>
#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/proto/packet_lengths.h"

using ro::proto::StopAttackRequest;

TEST_CASE("Arrêt d'attaque : la longueur est celle du serveur", "[proto][attaque]") {
  const auto length = ro::proto::PacketLength(StopAttackRequest::kOpcode);
  REQUIRE(length.has_value());
  CHECK(static_cast<std::size_t>(*length) == StopAttackRequest::kWireSize);
}

TEST_CASE("Arrêt d'attaque : CZ_STOP_ATTACK n'est que son opcode", "[proto][attaque]") {
  const auto frame = ro::proto::EncodeStopAttack();
  REQUIRE(frame.has_value());
  CHECK(*frame == std::vector<std::uint8_t>{0x18, 0x01});
}
