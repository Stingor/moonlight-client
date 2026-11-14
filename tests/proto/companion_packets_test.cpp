// LES ORDRES D'UN COMPAGNON : l'attaque et le retour au maître, tels que le
// serveur les lit.

#include "ro/proto/companion_packets.h"

#include <array>
#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/proto/manual_packets.h"

using ro::proto::CompanionAttackRequest;
using ro::proto::CompanionReturnRequest;

TEST_CASE("Ordre de compagnon : le serveur lit l'attaque et le retour à ces décalages",
          "[proto][companion]") {
  constexpr std::array<std::uint16_t, 3> kAttackOffsets = {2, 6, 10};
  constexpr std::array<std::uint16_t, 1> kReturnOffsets = {2};
  CHECK(ro::proto::MatchesServerLayout(CompanionAttackRequest::kOpcode, "clif_parse_HomAttack",
                                       kAttackOffsets));
  CHECK(ro::proto::MatchesServerLayout(CompanionReturnRequest::kOpcode,
                                       "clif_parse_HomMoveToMaster", kReturnOffsets));
}

TEST_CASE("Ordre de compagnon : l'attaque part en 0x0233 sur onze octets", "[proto][companion]") {
  const auto frame = ro::proto::Encode(
      CompanionAttackRequest{.companion_id = 0x01020304, .target_id = 0x0A0B0C0D, .action = 0});
  REQUIRE(frame.has_value());
  CHECK(*frame == std::vector<std::uint8_t>{0x33, 0x02, 0x04, 0x03, 0x02, 0x01, 0x0D, 0x0C, 0x0B,
                                            0x0A, 0x00});
}

TEST_CASE("Ordre de compagnon : le retour au maître part en 0x0234 sur six octets",
          "[proto][companion]") {
  const auto frame = ro::proto::Encode(CompanionReturnRequest{.companion_id = 0x01020304});
  REQUIRE(frame.has_value());
  CHECK(*frame == std::vector<std::uint8_t>{0x34, 0x02, 0x04, 0x03, 0x02, 0x01});
}
