// LA LISTE DES SORTS D'AUTOSPELL : celle que le serveur propose, et la réponse,
// qui vaut zéro quand le joueur refuse.

#include "ro/net/spell_list.h"

#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/connection.h"
#include "ro/proto/autospell_packets.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/spell_list.h"

#include "support/loopback_link.h"
#include "support/loopback_server.h"
#include "support/net_frames.h"

using ro::net::ApplySpellListFrame;
using ro::net::FrameOutcome;
using ro::net::RequestSpellChoice;
using ro::test::Connect;
using ro::test::LoopbackServer;
using ro::test_support::FrameOf;

namespace {

constexpr std::int32_t kFireBolt = 19;
constexpr std::int32_t kColdBolt = 14;

}  // namespace

TEST_CASE("Liste des sorts : la trame porte sa longueur et quatre octets par sort",
          "[net][autospell]") {
  const std::vector<std::uint8_t> bytes =
      ro::proto::EncodeAutoSpellList(ro::proto::AutoSpellList{{kColdBolt, kFireBolt}});
  REQUIRE(bytes.size() ==
          ro::proto::AutoSpellList::kHeaderSize + 2 * ro::proto::AutoSpellList::kEntrySize);
  CHECK(bytes[0] == 0xFB);
  CHECK(bytes[1] == 0x0A);
  CHECK(bytes[2] == bytes.size());

  const auto decoded = ro::proto::DecodeAutoSpellList(bytes);
  REQUIRE(decoded.has_value());
  CHECK(decoded->skill_ids == std::vector<std::int32_t>{kColdBolt, kFireBolt});

  std::vector<std::uint8_t> ragged = bytes;
  ragged.pop_back();
  CHECK_FALSE(ro::proto::DecodeAutoSpellList(ragged).has_value());
}

TEST_CASE("Liste des sorts : les cases vides ne s'affichent pas, une liste vide n'ouvre rien",
          "[net][autospell]") {
  ro::sim::SpellListOffer offer;
  CHECK(ApplySpellListFrame(
            FrameOf(ro::proto::EncodeAutoSpellList(ro::proto::AutoSpellList{{0, -1}})), offer) ==
        FrameOutcome::kIgnored);
  CHECK_FALSE(offer.open());

  CHECK(ApplySpellListFrame(FrameOf(ro::proto::EncodeAutoSpellList(
                                ro::proto::AutoSpellList{{kColdBolt, 0, kFireBolt, -1}})),
                            offer) == FrameOutcome::kApplied);
  REQUIRE(offer.open());
  CHECK(std::vector<std::uint16_t>(offer.skill_ids().begin(), offer.skill_ids().end()) ==
        std::vector<std::uint16_t>{kColdBolt, kFireBolt});
}

TEST_CASE("Liste des sorts : le choix part tel quel, le refus part à zéro", "[net][autospell]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  ro::net::Connection link = Connect(server);

  REQUIRE(RequestSpellChoice(link, kFireBolt).has_value());
  REQUIRE(RequestSpellChoice(link, 0).has_value());
  for (const std::uint32_t expected : {static_cast<std::uint32_t>(kFireBolt), 0U}) {
    const std::vector<std::uint8_t> sent = server.Receive(ro::proto::CzSelectautospell::kWireSize);
    REQUIRE(sent.size() == ro::proto::CzSelectautospell::kWireSize);
    CHECK(sent[0] == 0xCE);
    CHECK(sent[1] == 0x01);
    CHECK(sent[2] == (expected & 0xFFU));
    CHECK(sent[3] == 0);
  }
}

TEST_CASE("Liste des sorts : le niveau armé suit la règle du serveur", "[net][autospell]") {
  using ro::sim::AutospellArmedLevel;
  constexpr std::uint16_t kSoulStrike = 13;
  constexpr std::uint16_t kFireBall = 17;
  constexpr std::uint16_t kHeal = 28;
  // Les éclairs : un, deux, puis trois à partir d'Autospell 4 — plafonnés par
  // le niveau appris.
  CHECK(AutospellArmedLevel(kFireBolt, 2, 10, false) == 1);
  CHECK(AutospellArmedLevel(kFireBolt, 3, 10, false) == 2);
  CHECK(AutospellArmedLevel(kColdBolt, 10, 10, false) == 3);
  CHECK(AutospellArmedLevel(kColdBolt, 10, 2, false) == 2);
  // Le lien d'âme du Sage les porte à dix.
  CHECK(AutospellArmedLevel(kFireBolt, 2, 10, true) == 10);
  CHECK(AutospellArmedLevel(kSoulStrike, 6, 10, false) == 2);
  CHECK(AutospellArmedLevel(kFireBall, 9, 10, false) == 2);
  // Ce qu'Autospell n'arme pas, ou ce qu'on n'a pas appris : rien.
  CHECK(AutospellArmedLevel(kHeal, 10, 10, false) == 0);
  CHECK(AutospellArmedLevel(kFireBolt, 10, 0, false) == 0);
}
