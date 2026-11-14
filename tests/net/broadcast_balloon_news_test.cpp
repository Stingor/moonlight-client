#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/world_notice.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/broadcast_balloon.h"
#include "ro/sim/world.h"

#include "support/net_frames.h"

// LES ANNONCES SE POSENT AUSSI EN HAUT DE L'ÉCRAN : chaque annonce montrée dans
// la conversation dépose sa bulle, avec la même couleur et le même texte.

using ro::net::ApplyBroadcastFrame;
using ro::net::BroadcastFilter;
using ro::net::FrameOutcome;
using ro::sim::BalloonFamily;
using ro::sim::BalloonNotice;
using ro::sim::World;
using ro::test_support::FrameOf;

namespace {

constexpr std::uint32_t kHeardMs = 4200;

FrameOutcome Broadcast(World& world, std::string message, const BroadcastFilter& filter = {}) {
  ro::proto::ZcBroadcast announcement;
  announcement.message = std::move(message);
  return ApplyBroadcastFrame(FrameOf(Encode(announcement)), world, filter, kHeardMs);
}

FrameOutcome Colored(World& world, std::string message, std::uint32_t font_color,
                     std::int16_t font_size) {
  ro::proto::ZcBroadcast2 announcement;
  announcement.fontColor = font_color;
  announcement.fontSize = font_size;
  announcement.message = std::move(message);
  return ApplyBroadcastFrame(FrameOf(Encode(announcement)), world, BroadcastFilter{}, kHeardMs);
}

// « micc » + le nom sur 24 octets + la couleur + le texte.
std::string Voice(std::string name, std::string_view text) {
  constexpr std::size_t kNameSize = 24;
  name.resize(kNameSize, '\0');
  return "micc" + name + "00FF00" + std::string(text);
}

}  // namespace

TEST_CASE("Bulle d'annonce : l'annonce simple dépose sa bulle, repliable", "[net][balloon]") {
  World world;
  REQUIRE(Broadcast(world, "Stingor: test broadcast") == FrameOutcome::kApplied);
  const std::vector<BalloonNotice> taken = world.balloon_news().Take();
  REQUIRE(taken.size() == 1);
  CHECK(taken[0].family == BalloonFamily::kAnnouncement);
  CHECK(taken[0].heard_ms == kHeardMs);
  CHECK(taken[0].font_px == ro::sim::kBalloonFontPixels);
  // ⚠ SEULE L'ANNONCE SIMPLE SE REPLIE chez le client.
  CHECK(taken[0].wraps);
  REQUIRE(taken[0].lines.size() == 1);
  CHECK(taken[0].lines[0].text == "Stingor: test broadcast");
  // Le jaune des annonces sans étiquette.
  CHECK(taken[0].lines[0].color == 0xFFFF00);
  // Prise, la file est vide.
  CHECK(world.balloon_news().Take().empty());
}

TEST_CASE("Bulle d'annonce : l'étiquette donne sa couleur, et ne s'écrit pas", "[net][balloon]") {
  World world;
  REQUIRE(Broadcast(world, "blueUne annonce bleue.") == FrameOutcome::kApplied);
  REQUIRE(Broadcast(world, "toolFF8040Une annonce orange.") == FrameOutcome::kApplied);
  REQUIRE(Broadcast(world, "ssssLes portes s'ouvrent.") == FrameOutcome::kApplied);
  const std::vector<BalloonNotice> taken = world.balloon_news().Take();
  REQUIRE(taken.size() == 3);
  CHECK(taken[0].lines[0].text == "Une annonce bleue.");
  CHECK(taken[0].lines[0].color == 0x00FFFF);
  CHECK(taken[1].lines[0].text == "Une annonce orange.");
  CHECK(taken[1].lines[0].color == 0xFF8040);
  // L'annonce de siège a SA case dans la conversation, mais aussi sa bulle.
  CHECK(taken[2].lines[0].text == "Les portes s'ouvrent.");
}

TEST_CASE("Bulle d'annonce : la voix d'un nom ignoré ne dépose rien", "[net][balloon]") {
  const BroadcastFilter filter{.ignores = [](std::string_view name) { return name == "Bob"; }};
  World world;
  CHECK(Broadcast(world, Voice("Bob", "Achetez !"), filter) == FrameOutcome::kIgnored);
  CHECK(world.balloon_news().Take().empty());
  CHECK(Broadcast(world, Voice("Alice", "Bonjour."), filter) == FrameOutcome::kApplied);
  const std::vector<BalloonNotice> taken = world.balloon_news().Take();
  REQUIRE(taken.size() == 1);
  CHECK(taken[0].lines[0].text == "Bonjour.");
  CHECK(taken[0].lines[0].color == 0x00FF00);
}

TEST_CASE("Bulle d'annonce : l'annonce colorée porte sa taille, sur une seule rangée",
          "[net][balloon]") {
  constexpr std::int16_t kAskedPx = 16;
  World world;
  REQUIRE(Colored(world, "Alice#1234: bonjour", 0x80FF40, kAskedPx) == FrameOutcome::kApplied);
  // Une taille nulle ou absurde garde le corps des bulles, ou son plafond.
  REQUIRE(Colored(world, "nulle", 0x00FFFFFF, 0) == FrameOutcome::kApplied);
  REQUIRE(Colored(world, "geante", 0x00FFFFFF, 1000) == FrameOutcome::kApplied);
  const std::vector<BalloonNotice> taken = world.balloon_news().Take();
  REQUIRE(taken.size() == 3);
  CHECK(taken[0].lines[0].text == "Alice: bonjour");
  CHECK(taken[0].lines[0].color == 0x80FF40);
  CHECK(taken[0].font_px == kAskedPx);
  CHECK_FALSE(taken[0].wraps);
  CHECK(taken[1].font_px == ro::sim::kBalloonFontPixels);
  CHECK(taken[2].font_px > kAskedPx);
  CHECK(taken[2].font_px < 1000);
}
