// Ce qu'un script montre sur la carte, décodé : le repère, la navigation, et la
// position d'un membre de la guilde.

#include "ro/net/map_guidance.h"

#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/guild.h"
#include "ro/proto/map_packets.h"
#include "ro/proto/navigation_packets.h"
#include "ro/sim/guild.h"
#include "ro/sim/map_guidance.h"

#include "support/net_frames.h"

using ro::net::ApplyGuildFrame;
using ro::net::ApplyMapGuidanceFrame;
using ro::net::FrameOutcome;
using ro::sim::MapGuidance;
using ro::sim::MapViewpoints;
using ro::sim::NavigationRequests;
using ro::sim::NavigationTarget;
using ro::test_support::FrameOf;

namespace {

constexpr std::uint32_t kNpc = 110'000'001;
constexpr std::uint32_t kNowMs = 42'000;
constexpr std::uint32_t kServerColor = 0x00336699;

ro::proto::ZcCompass Compass(std::uint32_t type, std::uint8_t point_id) {
  ro::proto::ZcCompass packet;
  packet.npcId = kNpc;
  packet.type = type;
  packet.xPos = 150;
  packet.yPos = 180;
  packet.id = point_id;
  packet.color = kServerColor;
  return packet;
}

FrameOutcome Apply(const std::vector<std::uint8_t>& bytes, MapGuidance& guidance) {
  return ApplyMapGuidanceFrame(FrameOf(bytes), guidance, kNowMs);
}

ro::proto::NavigationActive Navigation(std::uint8_t type, std::uint8_t flag) {
  ro::proto::NavigationActive packet;
  packet.type = type;
  packet.flag = flag;
  packet.hide_window = 1;
  packet.map = "prontera.gat";
  packet.x = 155;
  packet.y = 180;
  return packet;
}

}  // namespace

TEST_CASE("Repère (net) : l'action 1 pose un repère permanent, opaque, daté", "[net][viewpoints]") {
  MapGuidance guidance;
  MapViewpoints& marks = guidance.viewpoints;
  NavigationRequests& requests = guidance.navigation;
  CHECK(Apply(ro::proto::Encode(Compass(1, 3)), guidance) == FrameOutcome::kApplied);

  const ro::sim::Viewpoint* mark = marks.Find(kNpc, 3);
  REQUIRE(mark != nullptr);
  CHECK(mark->permanent);
  CHECK(mark->cell_x == 150);
  CHECK(mark->cell_y == 180);
  CHECK(mark->color_argb == 0xFF336699U);
  CHECK(mark->placed_ms == kNowMs);
  CHECK(requests.empty());
}

TEST_CASE("Repère (net) : l'action 0 pose un repère éphémère", "[net][viewpoints]") {
  MapGuidance guidance;
  MapViewpoints& marks = guidance.viewpoints;
  CHECK(Apply(ro::proto::Encode(Compass(0, 3)), guidance) == FrameOutcome::kApplied);
  REQUIRE(marks.Find(kNpc, 3) != nullptr);
  CHECK_FALSE(marks.Find(kNpc, 3)->permanent);
}

TEST_CASE("Repère (net) : l'action 2 retire le repère de ce numéro, et lui seul",
          "[net][viewpoints]") {
  MapGuidance guidance;
  MapViewpoints& marks = guidance.viewpoints;
  static_cast<void>(Apply(ro::proto::Encode(Compass(1, 3)), guidance));
  static_cast<void>(Apply(ro::proto::Encode(Compass(1, 4)), guidance));

  CHECK(Apply(ro::proto::Encode(Compass(2, 3)), guidance) == FrameOutcome::kApplied);
  CHECK(marks.Find(kNpc, 3) == nullptr);
  CHECK(marks.Find(kNpc, 4) != nullptr);
  // Retirer un repère absent ne change rien.
  CHECK(Apply(ro::proto::Encode(Compass(2, 3)), guidance) == FrameOutcome::kIgnored);
}

TEST_CASE("Repère (net) : une action inconnue est ignorée", "[net][viewpoints]") {
  MapGuidance guidance;
  MapViewpoints& marks = guidance.viewpoints;
  CHECK(Apply(ro::proto::Encode(Compass(7, 3)), guidance) == FrameOutcome::kIgnored);
  CHECK(marks.empty());
}

TEST_CASE("Navigation (net) : la demande se décode, le drapeau se lit chiffre par chiffre",
          "[net][navigation]") {
  MapGuidance guidance;
  MapViewpoints& marks = guidance.viewpoints;
  NavigationRequests& requests = guidance.navigation;
  CHECK(Apply(ro::proto::Encode(Navigation(0, 101)), guidance) == FrameOutcome::kApplied);

  const auto request = requests.Take();
  REQUIRE(request.has_value());
  CHECK(request->target == NavigationTarget::kCell);
  CHECK(request->services.transport);
  CHECK_FALSE(request->services.airship);
  CHECK(request->services.scrolls);
  CHECK(request->hide_window);
  // Le nom sort sans extension.
  CHECK(request->map == "prontera");
  CHECK(request->cell_x == 155);
  CHECK(request->cell_y == 180);
  CHECK(request->monster_id == 0);
  CHECK(marks.empty());
}

TEST_CASE("Navigation (net) : chaque chiffre du drapeau commande son service",
          "[net][navigation]") {
  MapGuidance guidance;
  NavigationRequests& requests = guidance.navigation;
  static_cast<void>(Apply(ro::proto::Encode(Navigation(0, 0)), guidance));
  static_cast<void>(Apply(ro::proto::Encode(Navigation(0, 10)), guidance));
  static_cast<void>(Apply(ro::proto::Encode(Navigation(0, 100)), guidance));
  static_cast<void>(Apply(ro::proto::Encode(Navigation(0, 111)), guidance));

  auto none = requests.Take();
  CHECK_FALSE((none->services.transport || none->services.airship || none->services.scrolls));
  auto airship = requests.Take();
  CHECK(airship->services.airship);
  CHECK_FALSE(airship->services.transport);
  CHECK_FALSE(airship->services.scrolls);
  auto transport = requests.Take();
  CHECK(transport->services.transport);
  CHECK_FALSE(transport->services.airship);
  auto all = requests.Take();
  CHECK((all->services.transport && all->services.airship && all->services.scrolls));
}

TEST_CASE("Navigation (net) : les quatre types de cible se distinguent", "[net][navigation]") {
  MapGuidance guidance;
  NavigationRequests& requests = guidance.navigation;
  ro::proto::NavigationActive monster = Navigation(3, 0);
  monster.x = 0;
  monster.y = 0;
  monster.mob_id = 1002;
  static_cast<void>(Apply(ro::proto::Encode(Navigation(1, 0)), guidance));
  static_cast<void>(Apply(ro::proto::Encode(Navigation(2, 0)), guidance));
  static_cast<void>(Apply(ro::proto::Encode(monster), guidance));

  CHECK(requests.Take()->target == NavigationTarget::kMap);
  CHECK(requests.Take()->target == NavigationTarget::kNpc);
  const auto seen = requests.Take();
  CHECK(seen->target == NavigationTarget::kMonster);
  CHECK(seen->monster_id == 1002);
}

TEST_CASE("Navigation (net) : un type de cible inconnu est ignoré", "[net][navigation]") {
  MapGuidance guidance;
  NavigationRequests& requests = guidance.navigation;
  CHECK(Apply(ro::proto::Encode(Navigation(4, 0)), guidance) == FrameOutcome::kIgnored);
  CHECK(requests.empty());
}

TEST_CASE("Navigation (net) : une trame tronquée est indéchiffrable", "[net][navigation]") {
  MapGuidance guidance;
  std::vector<std::uint8_t> bytes = ro::proto::Encode(Navigation(0, 0));
  bytes.pop_back();
  CHECK(Apply(bytes, guidance) == FrameOutcome::kUndecodable);
}

TEST_CASE("Guilde (net) : la position d'un membre se pose, puis la sentinelle la retire",
          "[net][guild]") {
  ro::sim::GuildBook guild;
  constexpr std::uint32_t kMember = 2'000'010;
  ro::proto::ZcNotifyPositionToGuildm moved;
  moved.aid = kMember;
  moved.xPos = 150;
  moved.yPos = 180;
  CHECK(ApplyGuildFrame(FrameOf(ro::proto::Encode(moved)), guild, 0) == FrameOutcome::kApplied);
  REQUIRE(guild.FindMemberCell(kMember) != nullptr);
  CHECK(guild.FindMemberCell(kMember)->x == 150);
  CHECK(guild.FindMemberCell(kMember)->y == 180);

  moved.xPos = -1;
  moved.yPos = -1;
  CHECK(ApplyGuildFrame(FrameOf(ro::proto::Encode(moved)), guild, 0) == FrameOutcome::kApplied);
  CHECK(guild.FindMemberCell(kMember) == nullptr);
}
