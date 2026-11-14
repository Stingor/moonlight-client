#include <algorithm>
#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/world_update.h"
#include "ro/proto/map_packets.h"
#include "ro/render/effect_sound_table.h"
#include "ro/sim/world.h"

#include "support/net_frames.h"

// La bannière que le serveur annonce quand un boss tombe : sur qui elle naît,
// et ce qu'elle fait entendre.
using ro::net::ApplyFrame;
using ro::net::FrameOutcome;
using ro::test_support::FrameOf;

namespace {

constexpr std::uint32_t kMvpBannerEffect = 68;
constexpr std::uint32_t kKillerId = 2'000'001;
constexpr std::uint32_t kPlayerId = 2'000'002;
constexpr std::uint32_t kAnnouncedMs = 4'200;

bool Carries(const ro::sim::Actor* actor, std::uint32_t effect_id) {
  return actor != nullptr && std::ranges::find(actor->effects, effect_id,
                                               &ro::sim::PlayedEffect::id) != actor->effects.end();
}

ro::proto::ZcMvp MvpOf(std::uint32_t account_id) {
  ro::proto::ZcMvp packet;
  packet.AID = account_id;
  return packet;
}

}  // namespace

TEST_CASE("MVP : le paquet se décode sur six octets", "[net][mvp]") {
  ro::proto::ZcMvp decoded;
  REQUIRE(ro::proto::Decode(Encode(MvpOf(kKillerId)), decoded));
  CHECK(decoded.AID == kKillerId);
  CHECK(ro::proto::ZcMvp::kWireSize == 6);
}

TEST_CASE("MVP : la bannière naît sur le personnage qui a abattu le boss", "[net][mvp]") {
  ro::sim::World world;
  ro::sim::Actor killer;
  killer.id = kKillerId;
  world.Spawn(killer);

  CHECK(ApplyFrame(FrameOf(Encode(MvpOf(kKillerId))), world, kAnnouncedMs) ==
        FrameOutcome::kApplied);
  const ro::sim::Actor* found = world.Find(kKillerId);
  REQUIRE(Carries(found, kMvpBannerEffect));
  const auto banner =
      std::ranges::find(found->effects, kMvpBannerEffect, &ro::sim::PlayedEffect::id);
  CHECK(banner->since_ms == kAnnouncedMs);
}

TEST_CASE("MVP : le joueur porte sa propre bannière", "[net][mvp]") {
  // ⚠ À L'INVERSE DE L'EFFET DIVERS, qui ne cherche que parmi les autres : le
  // MVP nommé peut être le joueur lui-même, et c'est le cas le plus fréquent.
  ro::sim::World world;
  ro::sim::Actor player;
  player.id = kPlayerId;
  world.Spawn(player);
  world.set_player_id(kPlayerId);

  CHECK(ApplyFrame(FrameOf(Encode(MvpOf(kPlayerId))), world, kAnnouncedMs) ==
        FrameOutcome::kApplied);
  CHECK(Carries(world.Find(kPlayerId), kMvpBannerEffect));
}

TEST_CASE("MVP : un personnage hors de vue ne montre rien", "[net][mvp]") {
  ro::sim::World world;
  CHECK(ApplyFrame(FrameOf(Encode(MvpOf(kKillerId))), world, kAnnouncedMs) ==
        FrameOutcome::kIgnored);
}

TEST_CASE("MVP : la bannière fait entendre sa fanfare au premier tic", "[net][mvp]") {
  const auto sounds = ro::render::EffectSounds(kMvpBannerEffect);
  REQUIRE(sounds.size() == 1);
  CHECK(sounds.front().sound == "effect\\st_mvp.wav");
  CHECK(sounds.front().delay_ticks == 0);
}
