// LE GRAFFITI : le serveur pose l'unité et son message par une seule trame.

#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/world_update.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/actor.h"
#include "ro/sim/world.h"

#include "support/net_frames.h"

using ro::net::ApplyFrame;
using ro::net::FrameOutcome;
using ro::test_support::FrameOf;

namespace {

constexpr std::uint32_t kUnitId = 110077;
constexpr std::uint32_t kCasterId = 150001;
constexpr std::uint8_t kGraffitiUnit = 0xB0;
constexpr std::uint32_t kPlacedAtMs = 9000;

ro::proto::PacketGraffitiEntry Entry(std::uint8_t has_message, const char* text) {
  ro::proto::PacketGraffitiEntry packet;
  packet.AID = kUnitId;
  packet.creatorAID = kCasterId;
  packet.xPos = 120;
  packet.yPos = 85;
  packet.job = kGraffitiUnit;
  packet.isVisible = 1;
  packet.isContens = has_message;
  packet.msg = text;
  return packet;
}

}  // namespace

TEST_CASE("Graffiti : la trame pose l'unité et le message qu'elle trace", "[net][graffiti]") {
  ro::sim::World world;
  // 🔴 LA TRAME PASSE PAR L'AIGUILLAGE ORDINAIRE : sans lui, le message
  // n'arrivait nulle part, et rien ne s'écrivait au sol.
  REQUIRE(ApplyFrame(FrameOf(Encode(Entry(1, "Moonlight"))), world, kPlacedAtMs) ==
          FrameOutcome::kApplied);
  const ro::sim::SkillUnit& unit = world.skill_units().at(kUnitId);
  CHECK(unit.scribbled == "Moonlight");
  CHECK(unit.unit_id == kGraffitiUnit);
  CHECK(unit.creator_id == kCasterId);
  CHECK(unit.cell.x == 120);
  CHECK(unit.cell.y == 85);
  CHECK(unit.placed_ms == kPlacedAtMs);
}

TEST_CASE("Graffiti : sans le drapeau du message, l'unité n'en porte aucun", "[net][graffiti]") {
  ro::sim::World world;
  REQUIRE(ApplyFrame(FrameOf(Encode(Entry(0, "Moonlight"))), world, kPlacedAtMs) ==
          FrameOutcome::kApplied);
  CHECK(world.skill_units().at(kUnitId).scribbled.empty());
}

TEST_CASE("Flag Graffiti : la trame de Moonlight pose l'emblème et son niveau", "[net][graffiti]") {
  ro::proto::ZcMoonlightFlagGraffiti packet;
  packet.unit_id = kUnitId;
  packet.creator_id = kCasterId;
  packet.x = 64;
  packet.y = 32;
  packet.guild_id = 42;
  packet.emblem_id = 7;
  packet.level = 4;
  ro::sim::World world;
  // 🔴 PAR L'AIGUILLAGE ORDINAIRE, comme toute trame du serveur.
  REQUIRE(ApplyFrame(FrameOf(Encode(packet)), world, kPlacedAtMs) == FrameOutcome::kApplied);
  const ro::sim::SkillUnit& unit = world.skill_units().at(kUnitId);
  CHECK(unit.unit_id == ro::sim::kFlagGraffitiUnit);
  CHECK(unit.emblem_guild_id == 42);
  CHECK(unit.emblem_version == 7);
  CHECK(unit.skill_level == 4);
  CHECK(unit.cell.x == 64);
  CHECK(unit.cell.y == 32);
  CHECK(unit.creator_id == kCasterId);
}
