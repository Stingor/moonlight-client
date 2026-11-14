// LA PRÉSENCE EN ARÈNE SUR LE FIL : le CZ épinglé à l'octet, le ZC décodé
// depuis des octets écrits à la main, et ce qu'une trame abîmée donne.

#include "ro/net/survivors_presence.h"

#include <cstdint>
#include <optional>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "support/wire_bytes.h"

using ro::net::DecodeSurvivorsPresence;
using ro::net::EncodeSurvivorsPresenceAsk;
using ro::net::SurvivorsPresence;
using ro::net::SurvivorsPresenceAsk;
using ro::net::SurvivorsPresenceState;
using ro::test_support::PutU16;
using ro::test_support::PutU32;
using ro::test_support::ReadU16;

namespace {

constexpr std::uint32_t kAccountId = 2000123;
constexpr std::uint32_t kCharId = 150042;
constexpr std::uint16_t kArenaId = 3;
constexpr std::uint16_t kClassId = 2;
constexpr std::uint32_t kElapsedS = 462;

// Un ZC écrit à la main, `len` et taille au choix.
[[nodiscard]] std::vector<std::uint8_t> PresenceFrame(
    std::uint8_t state, std::uint16_t length = ro::net::kSurvivorsPresenceSize) {
  std::vector<std::uint8_t> frame;
  PutU16(frame, ro::net::kSurvivorsPresenceOpcode);
  PutU16(frame, length);
  PutU32(frame, kAccountId);
  PutU32(frame, kCharId);
  frame.push_back(state);
  PutU16(frame, kArenaId);
  PutU16(frame, kClassId);
  PutU32(frame, kElapsedS);
  return frame;
}

}  // namespace

TEST_CASE("Présence survivors : le CZ s'écrit à l'octet", "[net][survivors]") {
  const std::vector<std::uint8_t> bytes =
      EncodeSurvivorsPresenceAsk(SurvivorsPresenceAsk{.state = SurvivorsPresenceState::kPaused,
                                                      .arena_id = kArenaId,
                                                      .class_id = kClassId,
                                                      .flags = ro::net::kSurvivorsPresenceRanked});
  REQUIRE(bytes.size() == ro::net::kSurvivorsPresenceAskSize);
  CHECK(ReadU16(bytes, 0) == ro::net::kSurvivorsPresenceAskOpcode);
  CHECK(ReadU16(bytes, 2) == ro::net::kSurvivorsPresenceAskSize);
  CHECK(bytes[4] == 2);
  CHECK(ReadU16(bytes, 5) == kArenaId);
  CHECK(ReadU16(bytes, 7) == kClassId);
  CHECK(bytes[9] == ro::net::kSurvivorsPresenceRanked);
}

TEST_CASE("Présence survivors : le ZC se lit, et une trame abîmée se rejette", "[net][survivors]") {
  const std::optional<SurvivorsPresence> read = DecodeSurvivorsPresence(PresenceFrame(1));
  REQUIRE(read.has_value());
  CHECK(*read == SurvivorsPresence{.account_id = kAccountId,
                                   .char_id = kCharId,
                                   .state = SurvivorsPresenceState::kPlaying,
                                   .arena_id = kArenaId,
                                   .class_id = kClassId,
                                   .elapsed_s = kElapsedS});

  // Un état inconnu se lit dehors : rien n'est promis.
  const std::optional<SurvivorsPresence> unknown = DecodeSurvivorsPresence(PresenceFrame(9));
  REQUIRE(unknown.has_value());
  CHECK(unknown->state == SurvivorsPresenceState::kOut);

  // Un `len` qui ment, une trame tronquée ou allongée, un autre opcode.
  CHECK_FALSE(DecodeSurvivorsPresence(PresenceFrame(1, 20)).has_value());
  std::vector<std::uint8_t> short_frame = PresenceFrame(1);
  short_frame.pop_back();
  CHECK_FALSE(DecodeSurvivorsPresence(short_frame).has_value());
  std::vector<std::uint8_t> long_frame = PresenceFrame(1);
  long_frame.push_back(0);
  CHECK_FALSE(DecodeSurvivorsPresence(long_frame).has_value());
  std::vector<std::uint8_t> other = PresenceFrame(1);
  other[0] = 0x56;
  CHECK_FALSE(DecodeSurvivorsPresence(other).has_value());
}
