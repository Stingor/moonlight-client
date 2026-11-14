// Les recettes de la tablée, sur le fil : la demande au serveur de personnages
// et sa réponse, et ce qu'un serveur ou un intermédiaire hostile pourrait forger.

#include "ro/net/char_select_styles.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/server_rules.h"
#include "ro/net/session.h"
#include "ro/net/world_update.h"
#include "ro/sim/world.h"

#include "support/char_session.h"
#include "support/loopback_server.h"
#include "support/wire_bytes.h"

using ro::net::CharSelectStylesAnswer;
using ro::net::Frame;
using ro::net::ReadCharSelectStylesFrame;
using ro::test_support::PutU16;
using ro::test_support::PutU32;
using ro::test_support::ReadU16;
using ro::test_support::ReadU32;

namespace {

constexpr std::uint16_t kRequestOpcode = 0x0F65;
constexpr std::uint16_t kAnswerOpcode = 0x0F66;
constexpr std::uint16_t kRulesOpcode = 0x0F38;
constexpr std::size_t kAnswerHead = 7;
constexpr std::uint8_t kZipped = 0x01;
constexpr std::uint8_t kMore = 0x02;
constexpr std::uint32_t kKnight = 150'001;
constexpr std::uint32_t kSmith = 150'002;
constexpr std::uint32_t kBodyKey = 0xC0FFEE01;
constexpr std::uint32_t kHatView = 17;
constexpr std::size_t kRampCount = 8;

// Ce qu'un personnage montre : un corps, une coiffe teinte.
void PutShown(std::vector<std::uint8_t>& into, std::uint32_t char_id, std::int16_t hue) {
  PutU32(into, char_id);
  into.push_back(1);
  into.push_back(7);     // version du fil
  into.push_back(0x04);  // le repli
  PutU32(into, kBodyKey);
  PutU16(into, 42);
  PutU16(into, 0xFFFF);
  PutU16(into, 0xFFFF);
  for (std::size_t ramp = 0; ramp < kRampCount; ++ramp) {
    PutU16(into, static_cast<std::uint16_t>(ramp == 0 ? hue : 0));
    into.push_back(0);
    into.push_back(0);
    into.push_back(0);
  }
  into.push_back(1);
  into.push_back(10 + 5);  // une entrée d'un réglage
  into.push_back(2);       // coiffe
  PutU32(into, kHatView);
  into.push_back(1);
  into.push_back(0);
  PutU16(into, 1);
  PutU16(into, static_cast<std::uint16_t>(hue));
  into.push_back(0);
  into.push_back(0);
  into.push_back(0);
}

[[nodiscard]] Frame Answer(std::span<const std::uint8_t> body, std::uint8_t flags,
                           std::size_t raw_size) {
  Frame frame;
  frame.opcode = kAnswerOpcode;
  PutU16(frame.bytes, kAnswerOpcode);
  PutU16(frame.bytes, static_cast<std::uint16_t>(kAnswerHead + body.size()));
  frame.bytes.push_back(flags);
  PutU16(frame.bytes, static_cast<std::uint16_t>(raw_size));
  frame.bytes.insert(frame.bytes.end(), body.begin(), body.end());
  return frame;
}

}  // namespace

TEST_CASE("La demande de la tablée annonce qu'on lit la réponse", "[net][char_select_styles]") {
  const std::vector<std::uint8_t> request = ro::net::EncodeCharSelectStylesRequest();
  REQUIRE(request.size() == 6);
  CHECK(ReadU16(request, 0) == kRequestOpcode);
  CHECK(ReadU32(request, 2) == ro::net::kCharSelectStylesReads);
  CHECK(ro::net::CharSelectStylesKnown());
}

TEST_CASE("La réponse de la tablée range chaque personnage, corps et calques",
          "[net][char_select_styles]") {
  std::vector<std::uint8_t> raw;
  PutShown(raw, kKnight, 30);
  PutShown(raw, kSmith, -60);
  const auto answer = ReadCharSelectStylesFrame(Answer(raw, 0, raw.size()));
  REQUIRE(answer.has_value());
  CHECK_FALSE(answer->more);
  REQUIRE(answer->blocks.size() == 2);
  CHECK(answer->blocks[0].char_id == kKnight);
  CHECK(answer->blocks[0].variants.fallback_body == kBodyKey);
  CHECK(answer->blocks[0].variants.For(kBodyKey)->ramps[0].hue == 30);
  REQUIRE(answer->blocks[1].layers.size() == 1);
  CHECK(answer->blocks[1].layers[0].key == kHatView);
  CHECK(answer->blocks[1].layers[0].adjusts[0].hue == -60);

  // Une réponse vide dit « rien à montrer » ; le drapeau de suite se lit.
  const auto empty = ReadCharSelectStylesFrame(Answer({}, kMore, 0));
  REQUIRE(empty.has_value());
  CHECK(empty->blocks.empty());
  CHECK(empty->more);
}

TEST_CASE("Une réponse de la tablée forgée ne range rien", "[net][char_select_styles]") {
  std::vector<std::uint8_t> good;
  PutShown(good, kKnight, 30);
  std::vector<std::uint8_t> cut = good;
  cut.pop_back();
  CHECK_FALSE(ReadCharSelectStylesFrame(Answer(cut, 0, cut.size())).has_value());
  // La taille brute qui ment, un flux compressé qui ne la donne pas, un drapeau inconnu.
  CHECK_FALSE(ReadCharSelectStylesFrame(Answer(good, 0, good.size() + 1)).has_value());
  CHECK_FALSE(ReadCharSelectStylesFrame(Answer(good, kZipped, good.size())).has_value());
  CHECK_FALSE(ReadCharSelectStylesFrame(Answer(good, 0x04, good.size())).has_value());
  // Plus de corps annoncés que la charge n'en porte.
  std::vector<std::uint8_t> forged;
  PutU32(forged, kKnight);
  forged.push_back(200);
  CHECK_FALSE(ReadCharSelectStylesFrame(Answer(forged, 0, forged.size())).has_value());
}

TEST_CASE("La phase demande les recettes une fois, au choix, et range la réponse",
          "[net][char_select_styles]") {
  ro::test::LoopbackServer server;
  REQUIRE(server.listening());
  ro::net::LoginPhase::Account account;
  account.account_id = 2'000'123;
  auto phase = ro::net::CharacterPhase::Start(
      ro::net::ServerAddress{ro::test_support::kLoopbackWireIp, server.port()}, account);
  REQUIRE(phase.has_value());
  REQUIRE(server.Accept());
  REQUIRE(server.Receive(ro::test_support::kEnterRequestSize).size() ==
          ro::test_support::kEnterRequestSize);
  // 🔴 AVANT LA LISTE, RIEN NE PART : le serveur fermerait sur un paquet qu'il
  // n'attend pas.
  phase->RequestStyles();
  CHECK(server.Receive(1, ro::test_support::kBriefWaitMs).empty());

  REQUIRE(server.Send(ro::test_support::BareAccountId(account.account_id)));
  REQUIRE(server.Send(ro::test_support::CharacterList()));
  REQUIRE(
      ro::test_support::SettleUntil(*phase, ro::net::CharacterPhase::Step::kAwaitingSelection) ==
      ro::net::PhaseState::kWaiting);
  phase->RequestStyles();
  phase->RequestStyles();
  const std::vector<std::uint8_t> asked = server.Receive(6);
  REQUIRE(asked.size() == 6);
  CHECK(ReadU16(asked, 0) == kRequestOpcode);
  // Une fois par phase, même redemandée à chaque image.
  CHECK(server.Receive(1, ro::test_support::kBriefWaitMs).empty());

  std::vector<std::uint8_t> raw;
  PutShown(raw, kKnight, 30);
  REQUIRE(server.Send(Answer(raw, 0, raw.size()).bytes));
  ro::test_support::Churn(*phase, ro::test_support::kRoundsPastTheList);
  const std::vector<CharSelectStylesAnswer> answers = phase->TakeStyles();
  REQUIRE(answers.size() == 1);
  REQUIRE(answers[0].blocks.size() == 1);
  CHECK(answers[0].blocks[0].char_id == kKnight);
  CHECK(phase->TakeStyles().empty());
  // Une réponse ne change rien à l'attente du choix.
  CHECK(phase->step() == ro::net::CharacterPhase::Step::kAwaitingSelection);
}

TEST_CASE("La règle 28 annonce que le serveur de personnages sert la tablée",
          "[net][char_select_styles]") {
  ro::sim::World world;
  CHECK_FALSE(world.session().style_char_select_served);
  Frame frame;
  frame.opcode = kRulesOpcode;
  PutU16(frame.bytes, kRulesOpcode);
  PutU16(frame.bytes, 0);
  PutU16(frame.bytes, 28);
  PutU32(frame.bytes, 1);
  frame.bytes[2] = static_cast<std::uint8_t>(frame.bytes.size());
  REQUIRE(ro::net::ApplyServerRulesFrame(frame, world) == ro::net::FrameOutcome::kApplied);
  CHECK(world.session().style_char_select_served);
}
