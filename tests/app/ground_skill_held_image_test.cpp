// L'IMAGE QU'UNE COMPÉTENCE DE CASE IMPOSE à la motion de son lanceur, quand
// elle ne pose pas de motion d'emploi : la dernière du rang onze, bornée à ce
// que porte la motion qui tient la sienne.

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/formats/act.h"
#include "ro/formats/spr.h"
#include "ro/render/actor_effect_table.h"
#include "ro/render/actor_sprite.h"
#include "ro/render/actor_wardrobe.h"
#include "ro/render/skill_impl.h"
#include "ro/sim/actor.h"
#include "ro/sim/combat_notice.h"
#include "ro/sim/world.h"

#include "support/actor_client.h"
#include "support/sprite_builder.h"
#include "world/actor_view.h"

using ro::render::Motion;
using ro::render::SpriteLayout;

namespace {

constexpr std::uint32_t kWizard = 31;
constexpr std::uint16_t kMeteorStorm = 83;
constexpr std::uint16_t kSafetyWall = 12;
// Une compétence qu'on incante en état huit : l'incantation ordinaire.
constexpr std::uint16_t kGuardingCast = 256;
constexpr std::uint32_t kStormMs = 2'000;

// Les images de chaque rang d'un corps de joueur d'essai : trois pour
// l'attente et l'assise — les regards —, six pour l'incantation, et autant
// qu'on veut pour le rang onze.
constexpr std::size_t kRankCount = 13;
constexpr std::size_t kIdleRank = 0;
constexpr std::size_t kSitRank = 2;
constexpr std::size_t kElevenRank = 11;
constexpr std::size_t kCastRank = 12;
constexpr std::size_t kLookFrames = 3;
constexpr std::size_t kCastFrames = 6;
constexpr std::size_t kOtherFrames = 2;

std::vector<std::uint8_t> BodyAnimationBytes(std::size_t eleven_frames) {
  ro::test_support::ActBuildOptions options;
  for (std::size_t rank = 0; rank < kRankCount; ++rank) {
    std::size_t frames = kOtherFrames;
    if (rank == kIdleRank || rank == kSitRank) {
      frames = kLookFrames;
    } else if (rank == kCastRank) {
      frames = kCastFrames;
    } else if (rank == kElevenRank) {
      frames = eleven_frames;
    }
    for (std::size_t direction = 0; direction < ro::test_support::kDirectionCount; ++direction) {
      ro::test_support::ActActionSpec spec;
      for (std::size_t frame = 0; frame < frames; ++frame) {
        ro::test_support::ActFrameSpec frame_spec;
        ro::test_support::ActLayerSpec layer;
        layer.frame_index = static_cast<std::int32_t>(frame % kOtherFrames);
        frame_spec.layers.push_back(layer);
        frame_spec.anchors.push_back(ro::test_support::ActAnchorSpec{});
        spec.frames.push_back(std::move(frame_spec));
      }
      options.actions.push_back(std::move(spec));
      options.speeds.push_back(ro::test_support::kFrameSpeed);
    }
  }
  return ro::test_support::BuildAct(options);
}

struct DressedWizard {
  explicit DressedWizard(std::size_t eleven_frames)
      : sprite(*ro::formats::SprSprite::Parse(ro::test_support::SpriteBytes())),
        animation(*ro::formats::ActAnimation::Parse(BodyAnimationBytes(eleven_frames))),
        outfit(Pieces(sprite, animation)) {}

  static std::vector<ro::render::ActorPiece> Pieces(const ro::formats::SprSprite& body_sprite,
                                                    const ro::formats::ActAnimation& body_act) {
    ro::render::ActorPiece body;
    body.sprite = &body_sprite;
    body.animation = &body_act;
    body.part = ro::render::ActorPart::kBody;
    return {body};
  }

  // Ce que la scène compose sans l'image imposée, dans l'ordre où elle
  // consulte les postures : le repos, l'incantation, l'emploi, puis l'image du
  // regard d'un joueur à l'attente.
  moonclient::PlayedMotion Composed(const ro::sim::Actor& actor, std::uint32_t at_ms) {
    const moonclient::MotionEvents events{std::nullopt, {}, at_ms};
    moonclient::PlayedMotion played;
    played.motion = moonclient::RestMotionAt(actor, events, at_ms);
    played.animation = ro::render::AnimationFor(played.motion, SpriteLayout::kPlayer);
    if (actor.cast.has_value()) {
      played = moonclient::CastPostureAt(nullptr, actor, outfit, SpriteLayout::kPlayer,
                                         played.motion, events, at_ms)
                   .value_or(played);
    }
    if (played.motion != Motion::kCast && actor.skill_use.has_value()) {
      played = moonclient::SkillUsePostureAt(nullptr, actor, outfit, SpriteLayout::kPlayer,
                                             played.motion, events, at_ms)
                   .value_or(played);
    }
    if (played.motion == Motion::kIdle || played.motion == Motion::kSit) {
      played.held_frame = actor.head_direction;
    }
    return played;
  }

  // Puis l'image imposée, comme la scène la pose par-dessus.
  moonclient::PlayedMotion At(const ro::sim::World& world, std::uint32_t at_ms) {
    const ro::sim::Actor& actor = *world.Find(kWizard);
    moonclient::PlayedMotion played = Composed(actor, at_ms);
    const moonclient::MotionAtInstant before = [&](std::uint32_t then_ms,
                                                   std::span<const ro::render::ActorEffectPlay>) {
      return Composed(actor, then_ms);
    };
    if (const auto image = moonclient::GroundSkillHeldFrameAt(
            nullptr, actor, outfit, SpriteLayout::kPlayer, played,
            moonclient::MotionEvents{std::nullopt, {}, at_ms}, at_ms, before)) {
      played.held_frame = image;
    }
    return played;
  }

  ro::formats::SprSprite sprite;
  ro::formats::ActAnimation animation;
  ro::render::ActorOutfit outfit;
};

ro::sim::World WorldWithWizard() {
  ro::sim::World world;
  ro::sim::Actor wizard;
  wizard.id = kWizard;
  wizard.kind = ro::sim::ActorKind::kPlayer;
  world.Spawn(wizard);
  return world;
}

ro::sim::GroundSkillUse GroundSkill(std::uint16_t skill_id) {
  ro::sim::GroundSkillUse use;
  use.skill_id = skill_id;
  use.level = 10;
  use.cell = ro::sim::CellPosition{20, 20, 0};
  use.started_ms = kStormMs;
  return use;
}

}  // namespace

TEST_CASE("Case : Meteor Storm tourne la tête de qui attend, jusqu'au regard suivant",
          "[app][posture]") {
  // 🔴 LE CLIENT ÉCRIT LA DERNIÈRE IMAGE DU RANG ONZE sur celle de l'attente,
  // qui est l'image du regard, et l'attente la garde, bornée à ses trois
  // images. Le lanceur jouait la motion d'emploi, que le client ne pose pas.
  DressedWizard wizard(5);
  ro::sim::World world = WorldWithWizard();
  REQUIRE(world.RecordGroundSkill(kWizard, GroundSkill(kMeteorStorm)));
  const moonclient::PlayedMotion played = wizard.At(world, kStormMs + 500);
  CHECK(played.motion == Motion::kIdle);
  CHECK(played.held_frame == std::optional<std::size_t>{kLookFrames - 1});

  // Un regard reçu depuis rend l'image de la tête.
  REQUIRE(world.Turn(kWizard, 0, 1));
  CHECK(wizard.At(world, kStormMs + 600).held_frame == std::optional<std::size_t>{1});
}

TEST_CASE("Case : l'image imposée est la dernière du rang onze quand elle tient",
          "[app][posture]") {
  DressedWizard wizard(2);
  ro::sim::World world = WorldWithWizard();
  REQUIRE(world.RecordGroundSkill(kWizard, GroundSkill(kMeteorStorm)));
  CHECK(wizard.At(world, kStormMs + 500).held_frame == std::optional<std::size_t>{1});
  // Avant la compétence, rien n'était imposé.
  CHECK(wizard.At(world, kStormMs - 1).held_frame == std::optional<std::size_t>{0});
}

TEST_CASE("Case : l'incantation ordinaire tient l'image imposée jusqu'à sa fin", "[app][posture]") {
  REQUIRE(ro::render::SkillImplFor(kGuardingCast).CastState() == 8);
  DressedWizard wizard(5);
  ro::sim::World world = WorldWithWizard();
  ro::sim::Cast cast;
  cast.skill_id = kGuardingCast;
  cast.started_ms = kStormMs - 1'000;
  cast.duration_ms = 3'000;
  REQUIRE(world.RecordCast(kWizard, cast));
  CHECK(wizard.At(world, kStormMs - 1).held_frame == std::optional<std::size_t>{0});
  REQUIRE(world.RecordGroundSkill(kWizard, GroundSkill(kMeteorStorm)));
  const moonclient::PlayedMotion played = wizard.At(world, kStormMs + 500);
  CHECK(played.motion == Motion::kCast);
  CHECK(played.animation == kCastRank);
  CHECK(played.held_frame == std::optional<std::size_t>{4});
  // Un regard ne touche pas à l'image de l'incantation.
  REQUIRE(world.Turn(kWizard, 0, 1));
  CHECK(wizard.At(world, kStormMs + 600).held_frame == std::optional<std::size_t>{4});
}

TEST_CASE("Case : une compétence ordinaire n'impose aucune image", "[app][posture]") {
  DressedWizard wizard(5);
  ro::sim::World world = WorldWithWizard();
  REQUIRE(world.RecordGroundSkill(kWizard, GroundSkill(kSafetyWall)));
  const moonclient::PlayedMotion played = wizard.At(world, kStormMs + 5'000);
  CHECK(played.motion == Motion::kIdle);
  CHECK(played.held_frame == std::optional<std::size_t>{0});
}
