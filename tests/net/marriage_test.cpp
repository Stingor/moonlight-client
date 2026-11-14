// LE MARIAGE : la cérémonie sur qui la reçoit, le divorce dans la conversation,
// et le nom du conjoint, que le client garde sans le montrer.

#include "ro/net/marriage.h"

#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/proto/map_packets.h"
#include "ro/sim/world.h"

#include "support/net_frames.h"

using ro::net::ApplyMarriageFrame;
using ro::net::FrameOutcome;
using ro::test_support::FrameOf;

namespace {

constexpr std::uint32_t kWeddingEffect = 347;
constexpr std::uint32_t kBrideId = 2'000'001;
constexpr std::uint32_t kPlayerId = 2'000'002;
constexpr std::uint32_t kNowMs = 7'300;
constexpr std::uint32_t kYellow = 0xFFFF00;

bool Carries(const ro::sim::Actor* actor, std::uint32_t effect_id) {
  return actor != nullptr && std::ranges::find(actor->effects, effect_id,
                                               &ro::sim::PlayedEffect::id) != actor->effects.end();
}

std::vector<std::uint8_t> CongratulationOf(std::uint32_t actor_id) {
  ro::proto::ZcCongratulation packet;
  packet.GID = static_cast<std::int32_t>(actor_id);
  return ro::proto::Encode(packet);
}

std::vector<std::uint8_t> DivorceFrom(const char* former_spouse) {
  ro::proto::ZcDivorce packet;
  packet.name = former_spouse;
  return ro::proto::Encode(packet);
}

}  // namespace

TEST_CASE("Mariage : la cérémonie naît sur l'acteur nommé, le joueur compris", "[net][marriage]") {
  ro::sim::World world;
  ro::sim::Actor bride;
  bride.id = kBrideId;
  world.Spawn(bride);
  ro::sim::Actor player;
  player.id = kPlayerId;
  world.Spawn(player);
  world.set_player_id(kPlayerId);

  CHECK(ApplyMarriageFrame(FrameOf(CongratulationOf(kBrideId)), world, kNowMs) ==
        FrameOutcome::kApplied);
  CHECK(Carries(world.Find(kBrideId), kWeddingEffect));
  CHECK(ApplyMarriageFrame(FrameOf(CongratulationOf(kPlayerId)), world, kNowMs) ==
        FrameOutcome::kApplied);
  CHECK(Carries(world.Find(kPlayerId), kWeddingEffect));
}

TEST_CASE("Mariage : une cérémonie hors de vue ne montre rien", "[net][marriage]") {
  ro::sim::World world;
  CHECK(ApplyMarriageFrame(FrameOf(CongratulationOf(kBrideId)), world, kNowMs) ==
        FrameOutcome::kIgnored);
  CHECK(ApplyMarriageFrame(FrameOf(CongratulationOf(0)), world, kNowMs) == FrameOutcome::kIgnored);
}

TEST_CASE("Mariage : le divorce nomme le joueur et son ex-conjoint, en jaune", "[net][marriage]") {
  ro::sim::World world;
  world.set_player_name("Alice");
  CHECK(ApplyMarriageFrame(FrameOf(DivorceFrom("Bob")), world, kNowMs) == FrameOutcome::kApplied);
  REQUIRE(world.chat_lines().size() == 1);
  CHECK(world.chat_lines().back().text == "Alice et Bob ont divorcé.");
  CHECK(world.chat_lines().back().color == kYellow);
}

TEST_CASE("Mariage : le nom du conjoint ne s'écrit nulle part", "[net][marriage]") {
  ro::sim::World world;
  ro::proto::ZcCouplename packet;
  packet.name = "Bob";
  CHECK(ApplyMarriageFrame(FrameOf(ro::proto::Encode(packet)), world, kNowMs) ==
        FrameOutcome::kIgnored);
  CHECK(world.chat_lines().empty());
}

TEST_CASE("Mariage : une autre trame ne le regarde pas, une trame tronquée ne se lit pas",
          "[net][marriage]") {
  ro::sim::World world;
  ro::proto::ZcNotifyTime other;
  CHECK(ApplyMarriageFrame(FrameOf(ro::proto::Encode(other)), world, kNowMs) ==
        FrameOutcome::kUnhandled);
  std::vector<std::uint8_t> cut = DivorceFrom("Bob");
  cut.pop_back();
  CHECK(ApplyMarriageFrame(FrameOf(std::move(cut)), world, kNowMs) == FrameOutcome::kUndecodable);
  CHECK(world.chat_lines().empty());
}
