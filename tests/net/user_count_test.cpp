// `/who` : la réponse du serveur, lue telle que le fil la porte.

#include "ro/net/user_count.h"

#include <cstdint>
#include <optional>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/proto/map_packets.h"

#include "support/net_frames.h"

using ro::test_support::FrameOf;

TEST_CASE("Compte des joueurs : la réponse donne le nombre", "[net][who]") {
  ro::proto::ZcUserCount answer;
  answer.playersCount = 1234;
  const std::optional<std::int32_t> count =
      ro::net::ReadUserCount(FrameOf(ro::proto::Encode(answer)));
  REQUIRE(count.has_value());
  CHECK(*count == 1234);
}

TEST_CASE("Compte des joueurs : une autre trame n'en est pas une", "[net][who]") {
  CHECK_FALSE(ro::net::ReadUserCount(ro::net::Frame{.opcode = 0x0001, .bytes = {}}).has_value());
  // Une réponse tronquée ne se lit pas.
  ro::proto::ZcUserCount answer;
  ro::net::Frame cut = FrameOf(ro::proto::Encode(answer));
  cut.bytes.pop_back();
  CHECK_FALSE(ro::net::ReadUserCount(cut).has_value());
}
