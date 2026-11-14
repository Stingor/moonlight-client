// L'AGONIE D'UNE CRÉATURE : quand elle part — après le dernier dégât encore en
// route —, ce qu'elle dure dans la direction où l'on voit le corps, et qui en
// a une.

#include "world/corpse_agony.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/formats/act.h"
#include "ro/formats/spr.h"
#include "ro/render/actor_sprite.h"
#include "ro/render/actor_wardrobe.h"
#include "ro/render/floating_number.h"
#include "ro/render/sprite_frame.h"
#include "ro/sim/actor.h"
#include "ro/sim/combat_notice.h"
#include "ro/sim/death_rules.h"

#include "support/actor_client.h"
#include "support/sprite_builder.h"
#include "world/battle_log_rules.h"

namespace {

constexpr std::uint32_t kPoring = 42;
constexpr std::uint32_t kKnight = 7;
constexpr std::uint32_t kPoringJob = 1002;
constexpr std::uint32_t kKafraJob = 113;
constexpr std::uint32_t kDeathMs = 5'000;
constexpr std::size_t kRankCount = 13;
// La direction où l'action de mort a ses images et son délai à elle.
constexpr std::size_t kOddDirection = 3;
constexpr std::int32_t kOddFrames = 3;
constexpr float kOddDelayUnits = 6.0F;
constexpr std::int32_t kPlainFrames = 2;
constexpr float kPlainDelayUnits = 4.0F;
constexpr float kStepMs = 24.0F;

[[nodiscard]] std::size_t DieRank() {
  return ro::render::AnimationFor(ro::render::Motion::kDie, ro::render::SpriteLayout::kCreature);
}

// Un .act dont une seule direction de la mort diffère des autres.
[[nodiscard]] std::vector<std::uint8_t> DeathAnimationBytes() {
  ro::test_support::ActBuildOptions options;
  const std::size_t odd_action = DieRank() * ro::render::kDirectionCount + kOddDirection;
  for (std::size_t action = 0; action < kRankCount * ro::render::kDirectionCount; ++action) {
    const bool odd = action == odd_action;
    ro::test_support::ActActionSpec spec;
    for (std::int32_t frame = 0; frame < (odd ? kOddFrames : kPlainFrames); ++frame) {
      ro::test_support::ActFrameSpec frame_spec;
      ro::test_support::ActLayerSpec layer;
      layer.frame_index = frame % kPlainFrames;
      frame_spec.layers.push_back(layer);
      frame_spec.anchors.push_back(ro::test_support::ActAnchorSpec{});
      spec.frames.push_back(std::move(frame_spec));
    }
    options.actions.push_back(std::move(spec));
    options.speeds.push_back(odd ? kOddDelayUnits : kPlainDelayUnits);
  }
  return ro::test_support::BuildAct(options);
}

struct DyingCreature {
  DyingCreature()
      : sprite(*ro::formats::SprSprite::Parse(ro::test_support::SpriteBytes())),
        animation(*ro::formats::ActAnimation::Parse(DeathAnimationBytes())),
        outfit(Pieces(sprite, animation)) {}

  static std::vector<ro::render::ActorPiece> Pieces(const ro::formats::SprSprite& body_sprite,
                                                    const ro::formats::ActAnimation& body_act) {
    ro::render::ActorPiece body;
    body.sprite = &body_sprite;
    body.animation = &body_act;
    body.part = ro::render::ActorPart::kBody;
    return {body};
  }

  ro::formats::SprSprite sprite;
  ro::formats::ActAnimation animation;
  ro::render::ActorOutfit outfit;
};

[[nodiscard]] ro::sim::Actor Dead(ro::sim::ActorKind kind, std::uint8_t object_type,
                                  std::uint32_t job) {
  ro::sim::Actor actor;
  actor.id = kPoring;
  actor.kind = kind;
  actor.object_type = object_type;
  actor.look.job = static_cast<std::uint16_t>(job);
  actor.posture = ro::sim::Posture::kDead;
  actor.posture_since_ms = kDeathMs;
  return actor;
}

constexpr std::uint8_t kObjectMonster = 5;

}  // namespace

TEST_CASE("Agonie : la direction où l'on voit le corps décide de sa durée", "[app][corpse]") {
  DyingCreature creature;
  const ro::sim::Actor poring = Dead(ro::sim::ActorKind::kMonster, kObjectMonster, kPoringJob);
  const std::optional<moonclient::Agony> odd =
      moonclient::AgonyOf(poring, creature.outfit, kOddDirection, kDeathMs);
  REQUIRE(odd.has_value());
  CHECK(odd->start_ms == kDeathMs);
  CHECK(odd->duration_ms == kOddFrames * kOddDelayUnits * kStepMs);
  // Trois images de six pas de vingt-quatre millisecondes.
  constexpr std::uint32_t kOddAgonyMs = 432;
  CHECK(odd->EndMs() == kDeathMs + kOddAgonyMs);
  const std::optional<moonclient::Agony> plain =
      moonclient::AgonyOf(poring, creature.outfit, 0, kDeathMs);
  REQUIRE(plain.has_value());
  CHECK(plain->duration_ms == kPlainFrames * kPlainDelayUnits * kStepMs);
}

TEST_CASE("Agonie : seule une créature en a une", "[app][corpse]") {
  DyingCreature creature;
  // Un PNJ reste debout ; un joueur reste à terre.
  CHECK_FALSE(moonclient::AgonyOf(
                  Dead(ro::sim::ActorKind::kNonPlayer, ro::sim::kObjectTypeNonPlayer, kKafraJob),
                  creature.outfit, 0, kDeathMs)
                  .has_value());
  CHECK_FALSE(moonclient::AgonyOf(Dead(ro::sim::ActorKind::kPlayer, 0, kKnight), creature.outfit, 0,
                                  kDeathMs)
                  .has_value());
  // Et un vivant n'agonise pas.
  ro::sim::Actor alive = Dead(ro::sim::ActorKind::kMonster, kObjectMonster, kPoringJob);
  alive.posture = ro::sim::Posture::kStanding;
  CHECK_FALSE(moonclient::AgonyOf(alive, creature.outfit, 0, kDeathMs).has_value());
}

TEST_CASE("Agonie : elle attend le dernier dégât encore en route", "[app][corpse]") {
  // Le coup fatal frappe trois fois ; la mort est annoncée avant la première.
  constexpr std::uint32_t kFirstHitMs = kDeathMs + 300;
  constexpr auto kStep = static_cast<std::uint32_t>(ro::render::kMultiHitIntervalMs);
  ro::sim::Strike strike;
  strike.attacker = kKnight;
  strike.target = kPoring;
  strike.damage = 300;
  strike.type = ro::render::kMultipleBlows;
  strike.hits = 3;
  const moonclient::StrikeSides sides{.attacker_is_known = true, .attacker_is_player = true};
  const std::vector<moonclient::BattleMessage> messages =
      moonclient::BattleMessagesOf(strike, sides, kFirstHitMs, false, true);
  CHECK(moonclient::AgonyStartAfter(kDeathMs, kPoring, messages) == kFirstHitMs + 2 * kStep);
  // Ce qu'un autre reçoit ne retient pas cette mort-ci.
  CHECK(moonclient::AgonyStartAfter(kDeathMs, kKnight, messages) == kDeathMs);
  // Et un dégât déjà rejoué non plus.
  CHECK(moonclient::AgonyStartAfter(kFirstHitMs + 3 * kStep, kPoring, messages) ==
        kFirstHitMs + 3 * kStep);
}
