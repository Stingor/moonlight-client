// LES SURFACES ANNONCÉES AU SERVEUR (`CZ 0x0F24`) : le masque calculé, et quand
// il se ré-annonce.

#include "game/surface_announcer.h"

#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/connection.h"
#include "ro/net/ui_caps.h"
#include "ro/proto/map_packets.h"

#include "support/loopback_link.h"
#include "support/loopback_server.h"

using moonclient::ShownSurfaces;
using moonclient::SurfaceAnnouncer;
using ro::test::Connect;
using ro::test::LoopbackServer;

namespace {

using Bytes = std::vector<std::uint8_t>;

constexpr std::size_t kAnnounceBytes = ro::proto::CzMoonlightUiCaps::kWireSize;
// Assez pour qu'une annonce partie arrive ; un silence se juge sur ce délai.
constexpr int kSilenceMs = 200;

// L'annonce telle que le serveur la lit : [op:2][len:2][caps:4].
[[nodiscard]] Bytes Announce(std::uint32_t caps) {
  return {0x24,
          0x0F,
          0x08,
          0x00,
          static_cast<std::uint8_t>(caps & 0xFFU),
          static_cast<std::uint8_t>((caps >> 8U) & 0xFFU),
          static_cast<std::uint8_t>((caps >> 16U) & 0xFFU),
          static_cast<std::uint8_t>(caps >> 24U)};
}

}  // namespace

TEST_CASE("Surfaces : le carnet MVP suit son interrupteur, le reste est toujours là",
          "[app][ui_caps]") {
  constexpr std::uint32_t kAlways =
      ro::net::kUiCapNpcDialog | ro::net::kUiCapChat | ro::net::kUiCapCardAlbum |
      ro::net::kUiCapCardAlbumDropper | ro::net::kUiCapFlagGraffiti | ro::net::kUiCapUnitMaster |
      ro::net::kUiCapServerRules | ro::net::kUiCapMvpTrackerExt | ro::net::kUiCapQuestEnd |
      ro::net::kUiCapCompanionMaster | ro::net::kUiCapChatAuthor | ro::net::kUiCapDiscordRich |
      ro::net::kUiCapEquipAttributes | ro::net::kUiCapInstanceInfo | ro::net::kUiCapMarket |
      ro::net::kUiCapGmOutfit | ro::net::kUiCapMapEdits | ro::net::kUiCapAttackElement |
      ro::net::kUiCapLongNpcTalk | ro::net::kUiCapLongMobName;
  STATIC_CHECK(ShownSurfaces(false) == kAlways);
  // Le Marché aussi : sans lui, un catalogue ouvrirait la recherche d'origine.
  STATIC_CHECK((ShownSurfaces(false) & ro::net::kUiCapMarket) != 0);
  // L'auteur de la parole et le relais Discord riche s'annoncent toujours : ce
  // client sait les lire.
  STATIC_CHECK((ShownSurfaces(true) & ro::net::kUiCapChatAuthor) != 0);
  STATIC_CHECK((ShownSurfaces(true) & ro::net::kUiCapDiscordRich) != 0);
  // Ce qu'apporte une pièce posée ou retirée aussi.
  STATIC_CHECK((ShownSurfaces(false) & ro::net::kUiCapEquipAttributes) != 0);
  // Le détail d'une instance aussi.
  STATIC_CHECK((ShownSurfaces(false) & ro::net::kUiCapInstanceInfo) != 0);
  // Les queues du carnet se lisent carnet éteint compris.
  STATIC_CHECK((ShownSurfaces(false) & ro::net::kUiCapMvpTrackerExt) != 0);
  STATIC_CHECK(ShownSurfaces(true) == (kAlways | ro::net::kUiCapMvpTracker));
  STATIC_CHECK((ShownSurfaces(false) & ro::net::kUiCapMvpTracker) == 0);
}

TEST_CASE("Surfaces : annoncées à la vérification, puis à chaque changement", "[app][ui_caps]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  ro::net::Connection link = Connect(server);
  SurfaceAnnouncer announcer;

  // NON VÉRIFIÉ : le serveur n'écoute pas, rien ne part.
  announcer.Pump(link, false, ShownSurfaces(false));
  CHECK_FALSE(announcer.announced());
  CHECK(server.Receive(kAnnounceBytes, kSilenceMs).empty());

  // VÉRIFIÉ : une annonce, carnet éteint.
  announcer.Pump(link, true, ShownSurfaces(false));
  CHECK(announcer.announced());
  CHECK(announcer.last_caps() == ShownSurfaces(false));
  CHECK(server.Receive(kAnnounceBytes) == Announce(ShownSurfaces(false)));

  // INCHANGÉ : silence, image après image.
  announcer.Pump(link, true, ShownSurfaces(false));
  announcer.Pump(link, true, ShownSurfaces(false));
  CHECK(server.Receive(kAnnounceBytes, kSilenceMs).empty());

  // LE CARNET S'ALLUME : le masque change, il se ré-annonce avec le bit 0x04.
  announcer.Pump(link, true, ShownSurfaces(true));
  const Bytes lit = server.Receive(kAnnounceBytes);
  CHECK(lit == Announce(ShownSurfaces(true)));
  REQUIRE(lit.size() == kAnnounceBytes);
  CHECK((lit[4] & ro::net::kUiCapMvpTracker) != 0);
  CHECK(announcer.last_caps() == ShownSurfaces(true));
  announcer.Pump(link, true, ShownSurfaces(true));
  CHECK(server.Receive(kAnnounceBytes, kSilenceMs).empty());

  // ET S'ÉTEINT : de nouveau.
  announcer.Pump(link, true, ShownSurfaces(false));
  CHECK(server.Receive(kAnnounceBytes) == Announce(ShownSurfaces(false)));
}

TEST_CASE("Surfaces : une nouvelle poignée de main ré-annonce le même masque", "[app][ui_caps]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  ro::net::Connection link = Connect(server);
  SurfaceAnnouncer announcer;

  announcer.Pump(link, true, ShownSurfaces(true));
  CHECK(server.Receive(kAnnounceBytes) == Announce(ShownSurfaces(true)));

  // Une entrée en zone refait la poignée de main : le serveur a tout oublié.
  announcer.Pump(link, false, ShownSurfaces(true));
  CHECK_FALSE(announcer.announced());
  CHECK(server.Receive(kAnnounceBytes, kSilenceMs).empty());

  announcer.Pump(link, true, ShownSurfaces(true));
  CHECK(announcer.announced());
  CHECK(server.Receive(kAnnounceBytes) == Announce(ShownSurfaces(true)));
}

TEST_CASE("Surfaces : une partie reconnue dès sa première image annonce quand même",
          "[app][ui_caps]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  ro::net::Connection link = Connect(server);
  // L'annonceur survit à la partie : la première l'a vérifiée et annoncée.
  SurfaceAnnouncer announcer;
  announcer.Pump(link, true, ShownSurfaces(true));
  CHECK(server.Receive(kAnnounceBytes) == Announce(ShownSurfaces(true)));

  // 🔴 LE PERSONNAGE SUIVANT : le serveur le reconnaît d'office, aucune image
  // non vérifiée ne passe. Seul le réarmement de la nouvelle partie fait partir
  // le masque, que la session neuve du serveur ignore.
  announcer.Rearm();
  CHECK_FALSE(announcer.announced());
  announcer.Pump(link, true, ShownSurfaces(true));
  CHECK(announcer.announced());
  CHECK(server.Receive(kAnnounceBytes) == Announce(ShownSurfaces(true)));
  announcer.Pump(link, true, ShownSurfaces(true));
  CHECK(server.Receive(kAnnounceBytes, kSilenceMs).empty());
}
