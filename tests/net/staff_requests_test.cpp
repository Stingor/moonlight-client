// CE QUE LE MENU DU CLIC DROIT ÉMET, octet par octet.
//
// 🔴 CES PAQUETS NE SE VOIENT PAS EN JOUANT QUAND ILS SONT FAUX : le serveur lit
// ses champs à des décalages fixes, et une disposition décalée désigne un autre
// joueur — ou personne, en silence. Le banc les relit tels qu'ils partent.

#include "ro/net/staff_requests.h"

#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/connection.h"
#include "ro/net/guild.h"
#include "ro/net/world_update.h"
#include "ro/proto/guild_packets.h"
#include "ro/proto/map_packets.h"
#include "ro/proto/staff_packets.h"
#include "ro/sim/world.h"

#include "support/loopback_link.h"
#include "support/loopback_server.h"
#include "support/net_frames.h"
#include "support/wire_bytes.h"

using ro::test::Connect;
using ro::test::LoopbackServer;
using ro::test_support::ReadU16;
using ro::test_support::ReadU32;

namespace {

constexpr std::uint32_t kTarget = 2000123;
constexpr std::uint32_t kNpc = 3000007;
constexpr std::uint32_t kOwnAccount = 2000001;
constexpr std::uint32_t kOwnChar = 150000;
// L'opcode et la longueur, devant le texte d'une annonce.
constexpr std::size_t kAnnounceHeader = 4;

// Les numéros de `clif_bl_type`, tels que le paquet d'apparition les porte.
constexpr std::uint8_t kObjectNonPlayer = 0x6;
constexpr std::uint8_t kObjectPet = 0x7;
constexpr std::uint8_t kObjectHomunculus = 0x8;
constexpr std::uint8_t kObjectMercenary = 0x9;
constexpr std::uint8_t kObjectElemental = 0xA;

}  // namespace

TEST_CASE("Menu d'entité : l'expulsion part en 0x00CC, l'AID en deux", "[net][entity_menu]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  ro::net::Connection link = Connect(server);
  REQUIRE(ro::net::RequestKick(link, kTarget).has_value());
  const std::vector<std::uint8_t> sent = server.Receive(ro::proto::KickRequest::kWireSize);
  REQUIRE(sent.size() == ro::proto::KickRequest::kWireSize);
  CHECK(ReadU16(sent, 0) == 0x00CC);
  CHECK(ReadU32(sent, 2) == kTarget);
}

TEST_CASE("Menu d'entité : l'outillage PNJ porte sa longueur, le GID puis l'action",
          "[net][entity_menu]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  ro::net::Connection link = Connect(server);
  REQUIRE(ro::net::RequestNpcAdmin(link, kNpc, ro::net::NpcAdminAction::kUnload).has_value());
  const std::vector<std::uint8_t> sent = server.Receive(ro::proto::CzMoonlightNpcAdmin::kWireSize);
  REQUIRE(sent.size() == ro::proto::CzMoonlightNpcAdmin::kWireSize);
  CHECK(ReadU16(sent, 0) == 0x0F25);
  // ⚠ LA LONGUEUR EST LUE : à zéro, le serveur rejette sans un mot.
  CHECK(ReadU16(sent, 2) == ro::proto::CzMoonlightNpcAdmin::kWireSize);
  CHECK(ReadU32(sent, 4) == kNpc);
  CHECK(sent[8] == 1);
}

TEST_CASE("Menu d'entité : l'outillage joueur porte un delta SIGNÉ, même sans l'employer",
          "[net][entity_menu]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  ro::net::Connection link = Connect(server);
  constexpr std::int32_t kTaken = -50;
  REQUIRE(
      ro::net::RequestPlayerAdmin(link, kTarget, ro::net::PlayerAdminAction::kEventPoints, kTaken)
          .has_value());
  REQUIRE(ro::net::RequestPlayerAdmin(link, kTarget, ro::net::PlayerAdminAction::kJailToggle)
              .has_value());
  constexpr std::size_t kSize = ro::proto::CzMoonlightPlayerAdmin::kWireSize;
  const std::vector<std::uint8_t> sent = server.Receive(kSize * 2);
  REQUIRE(sent.size() == kSize * 2);
  CHECK(ReadU16(sent, 0) == 0x0F2B);
  CHECK(ReadU16(sent, 2) == kSize);
  CHECK(ReadU32(sent, 4) == kTarget);
  CHECK(sent[8] == 2);
  CHECK(static_cast<std::int32_t>(ReadU32(sent, 9)) == kTaken);
  // ⚠ LA TAILLE EST FIXE : le second paquet porte son paramètre, à zéro.
  CHECK(sent[kSize + 8] == 5);
  CHECK(ReadU32(sent, kSize + 9) == 0);
}

TEST_CASE("Menu d'entité : l'alliance désigne la cible, puis soi-même", "[net][entity_menu]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  ro::net::Connection link = Connect(server);
  REQUIRE(ro::net::RequestGuildAlliance(link, kTarget, kOwnAccount, kOwnChar).has_value());
  REQUIRE(ro::net::RequestGuildOpposition(link, kTarget).has_value());
  constexpr std::size_t kAlly = ro::proto::GuildAllianceRequest::kWireSize;
  constexpr std::size_t kFoe = ro::proto::GuildOppositionRequest::kWireSize;
  const std::vector<std::uint8_t> sent = server.Receive(kAlly + kFoe);
  REQUIRE(sent.size() == kAlly + kFoe);
  CHECK(ReadU16(sent, 0) == 0x0170);
  CHECK(ReadU32(sent, 2) == kTarget);
  CHECK(ReadU32(sent, 6) == kOwnAccount);
  CHECK(ReadU32(sent, 10) == kOwnChar);
  CHECK(ReadU16(sent, kAlly) == 0x0180);
  CHECK(ReadU32(sent, kAlly + 2) == kTarget);
}

TEST_CASE("Menu d'entité : un familier n'est pas un PNJ, et la guilde vient avec l'unité",
          "[net][entity_menu]") {
  CHECK(ro::net::FamiliarOf(kObjectNonPlayer) == ro::sim::Familiar::kNone);
  CHECK(ro::net::FamiliarOf(kObjectPet) == ro::sim::Familiar::kPet);
  CHECK(ro::net::FamiliarOf(kObjectHomunculus) == ro::sim::Familiar::kHomunculus);
  CHECK(ro::net::FamiliarOf(kObjectMercenary) == ro::sim::Familiar::kMercenary);
  CHECK(ro::net::FamiliarOf(kObjectElemental) == ro::sim::Familiar::kElemental);
  // Tous restent des non-joueurs pour le dessin.
  CHECK(ro::net::ActorKindOf(kObjectPet) == ro::sim::ActorKind::kNonPlayer);

  constexpr std::uint32_t kGuild = 42;
  ro::proto::PacketIdleUnit unit = ro::test_support::IdleUnit(kTarget, {.x = 10, .y = 10});
  unit.objecttype = kObjectPet;
  unit.GUID = kGuild;
  ro::sim::World world;
  REQUIRE(ro::net::ApplyFrame(ro::test_support::FrameOf(ro::proto::Encode(unit)), world, 0) ==
          ro::net::FrameOutcome::kApplied);
  const ro::sim::Actor* pet = world.Find(kTarget);
  REQUIRE(pet != nullptr);
  CHECK(pet->familiar == ro::sim::Familiar::kPet);
  CHECK(pet->guild_id == kGuild);
}

TEST_CASE("Annonces : /b part en 0x0099, longueur, texte et nul final", "[net][annonce]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  ro::net::Connection link = Connect(server);
  // « é » prouve la conversion : un octet dans la page de codes du jeu.
  REQUIRE(ro::net::RequestBroadcast(link, "Sting : été").has_value());
  constexpr std::size_t kText = 12;  // « Sting : été » en CP1252, puis le nul
  constexpr std::size_t kAcuteAt = 8;
  const std::vector<std::uint8_t> sent = server.Receive(kAnnounceHeader + kText);
  REQUIRE(sent.size() == kAnnounceHeader + kText);
  CHECK(ReadU16(sent, 0) == 0x0099);
  CHECK(ReadU16(sent, 2) == kAnnounceHeader + kText);
  CHECK(sent[kAnnounceHeader + kAcuteAt] == 0xE9);
  // ⚠ LE NUL FINAL : sans lui, la diffusion locale perd sa dernière lettre.
  CHECK(sent.back() == 0);
}

TEST_CASE("Annonces : /lb part en 0x019C", "[net][annonce]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  ro::net::Connection link = Connect(server);
  REQUIRE(ro::net::RequestLocalBroadcast(link, "abc").has_value());
  constexpr std::size_t kText = 4;  // « abc », puis le nul
  const std::vector<std::uint8_t> sent = server.Receive(kAnnounceHeader + kText);
  REQUIRE(sent.size() == kAnnounceHeader + kText);
  CHECK(ReadU16(sent, 0) == 0x019C);
  CHECK(ReadU16(sent, 2) == kAnnounceHeader + kText);
  CHECK(sent[kAnnounceHeader] == 'a');
  CHECK(sent.back() == 0);
}

TEST_CASE("Annonces : rien à annoncer ne part pas", "[net][annonce]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  ro::net::Connection link = Connect(server);
  CHECK_FALSE(ro::net::RequestBroadcast(link, "").has_value());
  CHECK_FALSE(ro::net::RequestLocalBroadcast(link, "").has_value());
}
