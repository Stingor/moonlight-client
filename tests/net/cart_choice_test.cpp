// LA LISTE DES CHARRETTES : celle que le serveur propose, et la réponse, dont
// le paquet dépend de l'identité que la liste portait.

#include "ro/net/cart_choice.h"

#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/connection.h"
#include "ro/proto/cart_packets.h"
#include "ro/sim/bank.h"
#include "ro/sim/cart_choice.h"

#include "support/loopback_link.h"
#include "support/loopback_server.h"
#include "support/net_frames.h"

using ro::net::ApplyCartChoiceFrame;
using ro::net::FrameOutcome;
using ro::net::RequestCartChoice;
using ro::test::Connect;
using ro::test::LoopbackServer;
using ro::test_support::FrameOf;

TEST_CASE("Liste des charrettes : celle du serveur ouvre la fenêtre, vide elle n'ouvre rien",
          "[net][cart]") {
  ro::sim::CartChoice choice;
  CHECK(ApplyCartChoiceFrame(
            FrameOf(ro::proto::EncodeSelectCartOffer(ro::proto::SelectCartOffer{7, {}})), choice) ==
        FrameOutcome::kIgnored);
  CHECK_FALSE(choice.open());

  CHECK(ApplyCartChoiceFrame(FrameOf(ro::proto::EncodeSelectCartOffer(
                                 ro::proto::SelectCartOffer{2'000'001, {10, 11, 12}})),
                             choice) == FrameOutcome::kApplied);
  REQUIRE(choice.open());
  CHECK(choice.offer().identity == 2'000'001);
  CHECK(choice.offer().cart_types == std::vector<std::uint8_t>{10, 11, 12});
}

TEST_CASE("Liste des charrettes : l'identité de la liste choisit le paquet de la réponse",
          "[net][cart]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  ro::net::Connection link = Connect(server);

  REQUIRE(RequestCartChoice(link, 0, 4).has_value());
  const std::vector<std::uint8_t> change = server.Receive(ro::proto::ChangeCartRequest::kWireSize);
  CHECK(change == std::vector<std::uint8_t>{0xAF, 0x01, 0x04, 0x00});

  REQUIRE(RequestCartChoice(link, 0x0A0B0C0D, 12).has_value());
  const std::vector<std::uint8_t> select = server.Receive(ro::proto::SelectCartRequest::kWireSize);
  CHECK(select == std::vector<std::uint8_t>{0x80, 0x09, 0x0D, 0x0C, 0x0B, 0x0A, 0x0C});
}
