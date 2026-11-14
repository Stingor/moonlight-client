// L'HABIT DU GM DIT AU SERVEUR (`CZ 0x0F43`) : quand il part, et quand il se tait.

#include "game/gm_outfit_announcer.h"

#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/connection.h"
#include "ro/proto/map_packets.h"

#include "support/loopback_link.h"
#include "support/loopback_server.h"

using moonclient::GmOutfitAnnouncer;
using ro::test::Connect;
using ro::test::LoopbackServer;

namespace {

using Bytes = std::vector<std::uint8_t>;

constexpr std::size_t kRequestBytes = ro::proto::CzMoonlightGmOutfit::kWireSize;
// Assez pour qu'une demande partie arrive ; un silence se juge sur ce délai.
constexpr int kSilenceMs = 200;

// La demande telle que le serveur la lit : [op:2][len:2][porté:1].
[[nodiscard]] Bytes Request(bool worn) {
  return {0x43, 0x0F, static_cast<std::uint8_t>(kRequestBytes), 0x00,
          static_cast<std::uint8_t>(worn ? 1 : 0)};
}

}  // namespace

TEST_CASE("Habit du GM : dit à la vérification, puis à chaque changement", "[app][gm_outfit]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  ro::net::Connection link = Connect(server);
  GmOutfitAnnouncer announcer;

  // NON VÉRIFIÉ : le serveur n'écoute pas, rien ne part.
  announcer.Pump(link, false, true, true);
  CHECK_FALSE(announcer.told());
  CHECK(server.Receive(kRequestBytes, kSilenceMs).empty());

  // VÉRIFIÉ : le choix part, même « quitté » — il a pu changer hors ligne.
  announcer.Pump(link, true, true, false);
  CHECK(announcer.told());
  CHECK(server.Receive(kRequestBytes) == Request(false));

  // INCHANGÉ : silence, image après image.
  announcer.Pump(link, true, true, false);
  announcer.Pump(link, true, true, false);
  CHECK(server.Receive(kRequestBytes, kSilenceMs).empty());

  // LA CASE SE COCHE, puis se décoche.
  announcer.Pump(link, true, true, true);
  CHECK(server.Receive(kRequestBytes) == Request(true));
  announcer.Pump(link, true, true, false);
  CHECK(server.Receive(kRequestBytes) == Request(false));
}

TEST_CASE("Habit du GM : sans le droit d'administrateur, rien ne part", "[app][gm_outfit]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  ro::net::Connection link = Connect(server);
  GmOutfitAnnouncer announcer;

  announcer.Pump(link, true, false, true);
  CHECK_FALSE(announcer.told());
  CHECK(server.Receive(kRequestBytes, kSilenceMs).empty());
}

TEST_CASE("Habit du GM : une nouvelle poignée de main le redit", "[app][gm_outfit]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  ro::net::Connection link = Connect(server);
  GmOutfitAnnouncer announcer;

  announcer.Pump(link, true, true, true);
  CHECK(server.Receive(kRequestBytes) == Request(true));

  // Une entrée en zone refait la poignée de main : un autre serveur, peut-être.
  announcer.Pump(link, false, true, true);
  CHECK_FALSE(announcer.told());
  announcer.Pump(link, true, true, true);
  CHECK(server.Receive(kRequestBytes) == Request(true));

  // Et le réarmement explicite, pour une partie reconnue dès sa première image.
  announcer.Rearm();
  announcer.Pump(link, true, true, true);
  CHECK(server.Receive(kRequestBytes) == Request(true));
}
