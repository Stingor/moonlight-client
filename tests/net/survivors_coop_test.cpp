#include "ro/net/survivors_coop.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

namespace ro::net {
namespace {

void PutU16(std::vector<std::uint8_t>& out, std::uint16_t value) {
  out.push_back(static_cast<std::uint8_t>(value & 0xFFU));
  out.push_back(static_cast<std::uint8_t>(value >> 8U));
}

void PutU32(std::vector<std::uint8_t>& out, std::uint32_t value) {
  for (std::size_t index = 0; index < 4; ++index) {
    out.push_back(static_cast<std::uint8_t>((value >> (index * 8U)) & 0xFFU));
  }
}

TEST_CASE("le salon se code à l'octet", "[survivors]") {
  const std::array<std::uint8_t, 2> part{7, 8};
  const auto bytes = EncodeSurvivorsCoopLobbyAsk(SurvivorsCoopAction::kReady, part);
  REQUIRE(bytes);
  CHECK(*bytes == std::vector<std::uint8_t>{0x58, 0x0F, 7, 0, 1, 7, 8});
  const std::vector<std::uint8_t> too_long(kSurvivorsCoopMostPartBytes + 1U);
  CHECK_FALSE(EncodeSurvivorsCoopLobbyAsk(SurvivorsCoopAction::kReady, too_long));
}

TEST_CASE("le choix d'un membre se code à l'octet", "[survivors]") {
  const auto bytes = EncodeSurvivorsCoopLobbyAsk(SurvivorsCoopAction::kChoice,
                                                 SurvivorsCoopChoicePart(0x0102, 0x0304));
  REQUIRE(bytes);
  CHECK(*bytes == std::vector<std::uint8_t>{0x58, 0x0F, 9, 0, 3, 0x02, 0x01, 0x04, 0x03});
}

TEST_CASE("une trame d'entrées respecte ses bornes", "[survivors]") {
  const std::vector<std::uint8_t> fine(4);
  const auto bytes = EncodeSurvivorsCoopFrameAsk(fine);
  REQUIRE(bytes);
  CHECK(*bytes == std::vector<std::uint8_t>{0x5B, 0x0F, 8, 0, 0, 0, 0, 0});
  CHECK_FALSE(EncodeSurvivorsCoopFrameAsk(std::vector<std::uint8_t>(3)));
  CHECK_FALSE(EncodeSurvivorsCoopFrameAsk(std::vector<std::uint8_t>(257)));
}

TEST_CASE("le salon se lit, noms et refus compris", "[survivors]") {
  std::vector<std::uint8_t> frame;
  PutU16(frame, kSurvivorsCoopLobbyOpcode);
  PutU16(frame, 10 + 33);
  frame.push_back(1);
  PutU32(frame, kSurvivorsCoopOutOfTown);
  frame.push_back(1);
  PutU32(frame, 150001);
  frame.push_back(1);
  const std::string name = "Stingor";
  frame.insert(frame.end(), name.begin(), name.end());
  frame.resize(frame.size() + kSurvivorsCoopNameBytes - name.size());
  PutU16(frame, 2);
  PutU16(frame, 5);
  const auto lobby = DecodeSurvivorsCoopLobby(frame);
  REQUIRE(lobby);
  CHECK(lobby->state == SurvivorsCoopState::kLobby);
  CHECK(lobby->refusals == kSurvivorsCoopOutOfTown);
  REQUIRE(lobby->members.size() == 1);
  CHECK(
      lobby->members[0] ==
      SurvivorsCoopMember{
          .char_id = 150001, .ready = true, .name = "Stingor", .class_id = 2, .first_skill_id = 5});
  frame.pop_back();
  CHECK_FALSE(DecodeSurvivorsCoopLobby(frame));
}

TEST_CASE("le départ se lit dans l'ordre des places", "[survivors]") {
  std::vector<std::uint8_t> frame;
  PutU16(frame, kSurvivorsCoopStartOpcode);
  PutU16(frame, 14 + 6 + 2 + 6);
  PutU32(frame, 0x11223344U);
  PutU32(frame, 0x55667788U);
  frame.push_back(4);
  frame.push_back(2);
  PutU32(frame, 1);
  PutU16(frame, 2);
  frame.push_back(9);
  frame.push_back(8);
  PutU32(frame, 2);
  PutU16(frame, 0);
  const auto start = DecodeSurvivorsCoopStart(frame);
  REQUIRE(start);
  CHECK(start->seed == 0x5566778811223344ULL);
  CHECK(start->input_delay_ticks == 4);
  REQUIRE(start->players.size() == 2);
  CHECK(start->players[0].part == std::vector<std::uint8_t>{9, 8});
  CHECK(start->players[1].char_id == 2);
  frame.push_back(0);
  CHECK_FALSE(DecodeSurvivorsCoopStart(frame));
}

TEST_CASE("une trame relayée et un départ de joueur se lisent", "[survivors]") {
  std::vector<std::uint8_t> frame;
  PutU16(frame, kSurvivorsCoopFrameOpcode);
  PutU16(frame, 12);
  PutU32(frame, 77);
  PutU32(frame, 5);
  const auto relayed = DecodeSurvivorsCoopFrame(frame);
  REQUIRE(relayed);
  CHECK(relayed->char_id == 77);
  CHECK(relayed->payload == std::vector<std::uint8_t>{5, 0, 0, 0});

  std::vector<std::uint8_t> left;
  PutU16(left, kSurvivorsCoopLeftOpcode);
  PutU16(left, kSurvivorsCoopLeftSize);
  PutU32(left, 77);
  PutU32(left, 900);
  const auto gone = DecodeSurvivorsCoopLeft(left);
  REQUIRE(gone);
  CHECK(*gone == SurvivorsCoopLeft{.char_id = 77, .through_tick = 900});
}

TEST_CASE("un nom du salon se lit en CP1252", "[survivors]") {
  std::vector<std::uint8_t> frame{0x59, 0x0F};
  const std::uint16_t length = 10U + 33U;
  PutU16(frame, length);
  frame.push_back(1);
  PutU32(frame, 0);
  frame.push_back(1);
  PutU32(frame, 150001);
  frame.push_back(0);
  // « Élan », où É vaut 0xC9 sur le fil.
  const std::array<std::uint8_t, 4> name{0xC9, 'l', 'a', 'n'};
  frame.insert(frame.end(), name.begin(), name.end());
  frame.resize(length);
  const auto lobby = DecodeSurvivorsCoopLobby(frame);
  REQUIRE(lobby);
  REQUIRE(lobby->members.size() == 1);
  CHECK(lobby->members.front().name == "\xC3\x89lan");
}

}  // namespace
}  // namespace ro::net
