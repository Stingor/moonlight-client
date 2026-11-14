// Le boss d'un Convex Mirror, décodé : sa case sur la carte, et ce que la
// conversation en dit.

#include <cstdint>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/map_guidance.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/map_guidance.h"
#include "ro/sim/world.h"

#include "support/net_frames.h"

using ro::net::ApplyBossInfoFrame;
using ro::net::FrameOutcome;
using ro::sim::MapGuidance;
using ro::test_support::FrameOf;

namespace {

constexpr std::uint32_t kNowMs = 42'000;
constexpr std::uint32_t kWhite = 0xFFFFFF;
constexpr std::uint8_t kNone = 0;
constexpr std::uint8_t kAlive = 1;
constexpr std::uint8_t kAliveSaid = 2;
constexpr std::uint8_t kDead = 3;

ro::proto::ZcBossInfo Boss(std::uint8_t type) {
  ro::proto::ZcBossInfo packet;
  packet.type = type;
  packet.x = 120;
  packet.y = 84;
  packet.name = "Baphomet";
  return packet;
}

FrameOutcome Apply(const ro::proto::ZcBossInfo& packet, MapGuidance& guidance,
                   ro::sim::World& world) {
  return ApplyBossInfoFrame(FrameOf(ro::proto::Encode(packet)), guidance, world, kNowMs);
}

}  // namespace

TEST_CASE("Boss (net) : vivant, sa case se pose sans un mot", "[net][boss]") {
  MapGuidance guidance;
  ro::sim::World world;
  CHECK(Apply(Boss(kAlive), guidance, world) == FrameOutcome::kApplied);
  REQUIRE(guidance.boss.has_value());
  CHECK(guidance.boss->x == 120);
  CHECK(guidance.boss->y == 84);
  CHECK(world.chat_lines().empty());
}

TEST_CASE("Boss (net) : la première révélation se dit, en blanc", "[net][boss]") {
  MapGuidance guidance;
  ro::sim::World world;
  CHECK(Apply(Boss(kAliveSaid), guidance, world) == FrameOutcome::kApplied);
  REQUIRE(guidance.boss.has_value());
  REQUIRE(world.chat_lines().size() == 1);
  const ro::sim::ChatLine& line = world.chat_lines().back();
  CHECK(line.text.find("Baphomet") != std::string::npos);
  CHECK(line.color == kWhite);
  CHECK(line.type == ro::sim::ChatType::kPublic);
  CHECK(line.said_ms == kNowMs);
}

TEST_CASE("Boss (net) : la tombe efface la case et dit l'heure du retour", "[net][boss]") {
  MapGuidance guidance;
  ro::sim::World world;
  static_cast<void>(Apply(Boss(kAlive), guidance, world));
  ro::proto::ZcBossInfo dead = Boss(kDead);
  dead.minHours = 1;
  dead.minMinutes = 5;
  CHECK(Apply(dead, guidance, world) == FrameOutcome::kApplied);
  CHECK_FALSE(guidance.boss.has_value());
  REQUIRE(world.chat_lines().size() == 1);
  const std::string& text = world.chat_lines().back().text;
  CHECK(text.find("Baphomet") != std::string::npos);
  CHECK(text.find("01") != std::string::npos);
  CHECK(text.find("05") != std::string::npos);
}

TEST_CASE("Boss (net) : une minute pile, le retour est imminent", "[net][boss]") {
  MapGuidance guidance;
  ro::sim::World world;
  ro::proto::ZcBossInfo dead = Boss(kDead);
  dead.minHours = 0;
  dead.minMinutes = 1;
  static_cast<void>(Apply(dead, guidance, world));
  ro::proto::ZcBossInfo later = Boss(kDead);
  later.minHours = 0;
  later.minMinutes = 2;
  static_cast<void>(Apply(later, guidance, world));
  REQUIRE(world.chat_lines().size() == 2);
  // Les deux gabarits diffèrent : l'imminent ne porte aucun nombre.
  CHECK(world.chat_lines()[0].text.find("01") == std::string::npos);
  CHECK(world.chat_lines()[1].text.find("02") != std::string::npos);
}

TEST_CASE("Boss (net) : sans boss, la case s'efface et l'absence se dit", "[net][boss]") {
  MapGuidance guidance;
  ro::sim::World world;
  static_cast<void>(Apply(Boss(kAlive), guidance, world));
  CHECK(Apply(Boss(kNone), guidance, world) == FrameOutcome::kApplied);
  CHECK_FALSE(guidance.boss.has_value());
  CHECK(world.chat_lines().size() == 1);
}

TEST_CASE("Boss (net) : une annonce inconnue efface la case sans rien dire", "[net][boss]") {
  MapGuidance guidance;
  ro::sim::World world;
  static_cast<void>(Apply(Boss(kAlive), guidance, world));
  CHECK(Apply(Boss(9), guidance, world) == FrameOutcome::kIgnored);
  CHECK_FALSE(guidance.boss.has_value());
  CHECK(world.chat_lines().empty());
}
