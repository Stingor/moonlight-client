// L'HABIT DU GM D'UN JOUEUR — `ZC_MOONLIGHT_GM_OUTFIT 0x0F44`, et ce que le monde
// en garde.

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
using ro::test_support::IdleUnit;

namespace {

constexpr std::uint32_t kAdminId = 2'000'099;
constexpr std::uint32_t kNowMs = 5000;

FrameOutcome TellOutfit(ro::sim::World& world, std::uint32_t player_id, bool worn) {
  ro::proto::ZcMoonlightGmOutfit told;
  told.GID = player_id;
  told.worn = worn ? 1 : 0;
  return ApplyFrame(FrameOf(Encode(told)), world, kNowMs);
}

bool Wears(const ro::sim::World& world, std::uint32_t player_id) {
  const ro::sim::Actor* actor = world.Find(player_id);
  return actor != nullptr && actor->gm_outfit;
}

}  // namespace

TEST_CASE("Habit du GM : la trame le pose sur le joueur apparu, et l'ôte", "[net][gm_outfit]") {
  ro::sim::World world;
  REQUIRE(ApplyFrame(FrameOf(Encode(IdleUnit(kAdminId, {100, 100}))), world, kNowMs) ==
          FrameOutcome::kApplied);
  CHECK_FALSE(Wears(world, kAdminId));

  // 🔴 PAR L'AIGUILLAGE ORDINAIRE, comme toute trame du serveur.
  REQUIRE(TellOutfit(world, kAdminId, true) == FrameOutcome::kApplied);
  CHECK(Wears(world, kAdminId));
  REQUIRE(TellOutfit(world, kAdminId, false) == FrameOutcome::kApplied);
  CHECK_FALSE(Wears(world, kAdminId));
}

TEST_CASE("Habit du GM : avant l'apparition, la trame est ignorée", "[net][gm_outfit]") {
  ro::sim::World world;
  // Le serveur l'envoie toujours après l'apparition, et le redit à la suivante.
  CHECK(TellOutfit(world, kAdminId, true) == FrameOutcome::kIgnored);
  CHECK(world.Find(kAdminId) == nullptr);
  REQUIRE(ApplyFrame(FrameOf(Encode(IdleUnit(kAdminId, {100, 100}))), world, kNowMs) ==
          FrameOutcome::kApplied);
  CHECK_FALSE(Wears(world, kAdminId));
}

TEST_CASE("Habit du GM : il s'en va avec l'acteur", "[net][gm_outfit]") {
  ro::sim::World world;
  REQUIRE(ApplyFrame(FrameOf(Encode(IdleUnit(kAdminId, {100, 100}))), world, kNowMs) ==
          FrameOutcome::kApplied);
  REQUIRE(TellOutfit(world, kAdminId, true) == FrameOutcome::kApplied);
  // Sorti de vue puis revenu : c'est au serveur de le redire.
  REQUIRE(world.Remove(kAdminId));
  REQUIRE(ApplyFrame(FrameOf(Encode(IdleUnit(kAdminId, {100, 100}))), world, kNowMs) ==
          FrameOutcome::kApplied);
  CHECK_FALSE(Wears(world, kAdminId));
}

TEST_CASE("Habit du GM : notre personnage suit notre choix", "[net][gm_outfit]") {
  ro::sim::World world;
  world.set_player_id(kAdminId);
  // Sans personnage, rien à habiller.
  world.DressPlayerAsGameMaster(true);
  REQUIRE(ApplyFrame(FrameOf(Encode(IdleUnit(kAdminId, {100, 100}))), world, kNowMs) ==
          FrameOutcome::kApplied);
  CHECK_FALSE(Wears(world, kAdminId));
  world.DressPlayerAsGameMaster(true);
  CHECK(Wears(world, kAdminId));
  world.DressPlayerAsGameMaster(false);
  CHECK_FALSE(Wears(world, kAdminId));
}

TEST_CASE("Habit du GM : la demande au serveur tient en cinq octets", "[net][gm_outfit]") {
  ro::proto::CzMoonlightGmOutfit request;
  request.packetLength = static_cast<std::int16_t>(ro::proto::CzMoonlightGmOutfit::kWireSize);
  request.worn = 1;
  const auto bytes = Encode(request);
  REQUIRE(bytes.size() == ro::proto::CzMoonlightGmOutfit::kWireSize);
  CHECK(bytes[0] == 0x43);
  CHECK(bytes[1] == 0x0F);
  CHECK(bytes[2] == ro::proto::CzMoonlightGmOutfit::kWireSize);
  CHECK(bytes[4] == 1);
}
