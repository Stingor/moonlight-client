// L'ADOPTION DANS LA PARTIE, contre un serveur en boucle locale : la boîte qui
// s'ouvre une fois par demande, la réponse qui part, les refus qui se disent.

#include "game/adoption_stage.h"

#include <cstdint>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/connection.h"
#include "ro/sim/adoption.h"
#include "ro/sim/chat_line.h"
#include "ro/sim/world.h"

#include "support/loopback_link.h"
#include "support/loopback_server.h"

using moonclient::AdoptionBox;
using moonclient::AdoptionStage;
using ro::sim::AdoptionInvitation;
using ro::sim::AdoptionRefusal;
using ro::test::Connect;

namespace {

constexpr std::uint32_t kNowMs = 1000;
constexpr std::uint32_t kParent = 2000010;
constexpr std::uint32_t kSpouse = 2000011;
constexpr std::size_t kAnswerBytes = 14;
constexpr int kSilenceMs = 150;

[[nodiscard]] AdoptionInvitation FromAna() {
  return AdoptionInvitation{
      .parent_account_id = kParent, .spouse_account_id = kSpouse, .parent_name = "Ana"};
}

}  // namespace

TEST_CASE("Adoption : la boîte s'ouvre une fois par demande, et la réponse part",
          "[app][adoption]") {
  ro::test::LoopbackServer server;
  REQUIRE(server.listening());
  ro::net::Connection link = Connect(server);
  ro::sim::World world;
  AdoptionStage stage;
  AdoptionBox box;

  stage.Advance(box, link, world, kNowMs);
  CHECK_FALSE(box.ask);
  CHECK(box.question.empty());

  stage.desk().TellInvitation(FromAna());
  stage.Advance(box, link, world, kNowMs);
  CHECK(box.ask);
  CHECK(box.question.find("Ana") != std::string::npos);
  // L'image suivante ne rouvre pas la boîte : elle attend.
  box.ask = false;
  stage.Advance(box, link, world, kNowMs);
  CHECK_FALSE(box.ask);
  CHECK_FALSE(box.question.empty());
  CHECK(server.Receive(kAnswerBytes, kSilenceMs).empty());

  box.answer = false;
  stage.Advance(box, link, world, kNowMs);
  const std::vector<std::uint8_t> sent = server.Receive(kAnswerBytes);
  REQUIRE(sent.size() == kAnswerBytes);
  CHECK(sent[0] == 0xF7);
  CHECK(sent[1] == 0x01);
  CHECK(sent[10] == 0);
  CHECK_FALSE(stage.desk().invitation().has_value());
  CHECK(box.question.empty());
  CHECK_FALSE(box.answer.has_value());
}

TEST_CASE("Adoption : chaque refus du serveur se dit dans la conversation", "[app][adoption]") {
  ro::test::LoopbackServer server;
  REQUIRE(server.listening());
  ro::net::Connection link = Connect(server);
  ro::sim::World world;
  AdoptionStage stage;
  AdoptionBox box;

  stage.desk().TellRefusal(AdoptionRefusal::kParentLevel);
  stage.Advance(box, link, world, kNowMs);
  REQUIRE(world.chat_lines().size() == 1);
  CHECK(world.chat_lines().front().text ==
        moonclient::AdoptionRefusalText(AdoptionRefusal::kParentLevel));
  stage.Advance(box, link, world, kNowMs);
  CHECK(world.chat_lines().size() == 1);
}

TEST_CASE("Adoption : une partie neuve oublie la demande", "[app][adoption]") {
  ro::test::LoopbackServer server;
  REQUIRE(server.listening());
  ro::net::Connection link = Connect(server);
  ro::sim::World world;
  AdoptionStage stage;
  AdoptionBox box;
  stage.desk().TellInvitation(FromAna());
  stage.StartSession();
  stage.Advance(box, link, world, kNowMs);
  CHECK_FALSE(box.ask);
  CHECK(box.question.empty());
}
