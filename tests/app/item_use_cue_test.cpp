// L'USAGE D'UN OBJET VU ET ENTENDU, trame par trame : l'acteur choisi par son
// AID — soi, un voisin en vue, un absent —, le refus qui ne joue rien, et la
// ligne du manuel d'expérience, pour soi seul.

#include "game/item_use_cue.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

#include "ro/proto/map_packets.h"
#include "ro/sim/actor.h"
#include "ro/sim/item_use_effect.h"
#include "ro/sim/world.h"

#include "support/net_frames.h"

using moonclient::CueItemUse;
using ro::net::Frame;
using ro::sim::World;
using ro::test_support::FrameOf;

namespace {

constexpr std::uint32_t kHero = 2000001;
constexpr std::uint32_t kNeighbour = 2000002;
constexpr std::uint32_t kStranger = 2000003;
constexpr std::uint32_t kNowMs = 5000;

constexpr std::uint32_t kRedPotion = 501;
constexpr std::uint16_t kRedPotionEffect = 204;
constexpr std::uint32_t kFireCracker = 12018;
constexpr std::uint16_t kFireCrackerEffect = 508;
constexpr std::uint32_t kExpManual = 12563;
constexpr std::uint16_t kRuneKnight = 4054;
constexpr std::uint16_t kKnight = 7;

constexpr std::uint8_t kUseGranted = 1;
constexpr std::uint8_t kUseRefused = 0;

[[nodiscard]] Frame UseAck(std::uint32_t item_id, std::uint32_t user_id,
                           std::uint8_t result = kUseGranted) {
  ro::proto::ZcUseItemAck packet;
  packet.index = 2;
  packet.itemId = item_id;
  packet.AID = user_id;
  packet.amount = 9;
  packet.result = result;
  return FrameOf(Encode(packet));
}

[[nodiscard]] ro::sim::Actor PlayerActor(std::uint32_t id, std::uint16_t job) {
  ro::sim::Actor actor;
  actor.id = id;
  actor.kind = ro::sim::ActorKind::kPlayer;
  actor.look.job = job;
  return actor;
}

[[nodiscard]] World WorldWithHeroAndNeighbour(std::uint16_t hero_job = kKnight) {
  World world;
  world.set_player_id(kHero);
  world.Spawn(PlayerActor(kHero, hero_job));
  world.Spawn(PlayerActor(kNeighbour, kKnight));
  return world;
}

[[nodiscard]] bool Carries(const World& world, std::uint32_t actor_id, std::uint32_t effect_id) {
  const ro::sim::Actor* actor = world.Find(actor_id);
  return actor != nullptr &&
         std::ranges::any_of(actor->effects, [effect_id](const ro::sim::PlayedEffect& played) {
           return played.id == effect_id;
         });
}

[[nodiscard]] std::size_t EffectsOf(const World& world, std::uint32_t actor_id) {
  const ro::sim::Actor* actor = world.Find(actor_id);
  return actor != nullptr ? actor->effects.size() : 0;
}

}  // namespace

TEST_CASE("Usage d'objet : ma potion se montre sur moi", "[app][item_use]") {
  World world = WorldWithHeroAndNeighbour();
  CHECK_FALSE(CueItemUse(UseAck(kRedPotion, kHero), world, kNowMs).has_value());
  CHECK(Carries(world, kHero, kRedPotionEffect));
  CHECK(EffectsOf(world, kNeighbour) == 0);
}

TEST_CASE("Usage d'objet : la potion d'un voisin en vue se montre sur lui", "[app][item_use]") {
  World world = WorldWithHeroAndNeighbour();
  static_cast<void>(CueItemUse(UseAck(kRedPotion, kNeighbour), world, kNowMs));
  CHECK(Carries(world, kNeighbour, kRedPotionEffect));
  CHECK(EffectsOf(world, kHero) == 0);
}

TEST_CASE("Usage d'objet : le son se rend pour soi comme pour un voisin", "[app][item_use]") {
  World world = WorldWithHeroAndNeighbour();
  const std::optional<std::string_view> mine =
      CueItemUse(UseAck(kFireCracker, kHero), world, kNowMs);
  REQUIRE(mine.has_value());
  CHECK(*mine == "effect\\itempokjuk.wav");
  const std::optional<std::string_view> theirs =
      CueItemUse(UseAck(kFireCracker, kNeighbour), world, kNowMs);
  REQUIRE(theirs.has_value());
  CHECK(*theirs == "effect\\itempokjuk.wav");
  CHECK(Carries(world, kNeighbour, kFireCrackerEffect));
}

TEST_CASE("Usage d'objet : un voisin hors de vue ne joue rien", "[app][item_use]") {
  World world = WorldWithHeroAndNeighbour();
  CHECK_FALSE(CueItemUse(UseAck(kFireCracker, kStranger), world, kNowMs).has_value());
  CHECK(EffectsOf(world, kHero) == 0);
  CHECK(EffectsOf(world, kNeighbour) == 0);
}

TEST_CASE("Usage d'objet : un usage refusé ne joue rien", "[app][item_use]") {
  World world = WorldWithHeroAndNeighbour();
  CHECK_FALSE(CueItemUse(UseAck(kFireCracker, kHero, kUseRefused), world, kNowMs).has_value());
  CHECK_FALSE(CueItemUse(UseAck(kFireCracker, kNeighbour, kUseRefused), world, kNowMs).has_value());
  CHECK(EffectsOf(world, kHero) == 0);
  CHECK(EffectsOf(world, kNeighbour) == 0);
}

TEST_CASE("Usage d'objet : une autre trame n'est pas lue", "[app][item_use]") {
  World world = WorldWithHeroAndNeighbour();
  ro::proto::ZcNotifyVanish vanish;
  vanish.gid = kNeighbour;
  CHECK_FALSE(CueItemUse(FrameOf(Encode(vanish)), world, kNowMs).has_value());
  CHECK(EffectsOf(world, kNeighbour) == 0);
}

TEST_CASE("Usage d'objet : le manuel écrit sa ligne pour soi seul, selon le métier",
          "[app][item_use]") {
  World knight = WorldWithHeroAndNeighbour(kKnight);
  static_cast<void>(CueItemUse(UseAck(kExpManual, kHero), knight, kNowMs));
  REQUIRE(knight.chat_lines().size() == 1);
  const ro::sim::ChatLine& line = knight.chat_lines().front();
  CHECK(line.type == ro::sim::kItemUseLineType);
  CHECK(line.color == ro::sim::kItemUseLineColor);
  CHECK(line.text.find("trois quarts") != std::string::npos);

  World rune_knight = WorldWithHeroAndNeighbour(kRuneKnight);
  static_cast<void>(CueItemUse(UseAck(kExpManual, kHero), rune_knight, kNowMs));
  REQUIRE(rune_knight.chat_lines().size() == 1);
  CHECK(rune_knight.chat_lines().front().text.find("moitié") != std::string::npos);

  World watching = WorldWithHeroAndNeighbour();
  static_cast<void>(CueItemUse(UseAck(kExpManual, kNeighbour), watching, kNowMs));
  CHECK(watching.chat_lines().empty());
}
