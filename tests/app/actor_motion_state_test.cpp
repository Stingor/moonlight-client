// LES ÉTATS D'ACTION QUE LE PAS DU MONDE TIENT OU FIGE : la garde d'Auto
// Counter, l'état figé de Spiral Pierce et de Death Bound, le sommeil, l'arrêt
// que le serveur impose, et les cinq comptes dont l'attente se déroule.

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
#include "ro/render/ground_skill_visual.h"
#include "ro/render/skill_impl.h"
#include "ro/render/skill_use_motion.h"
#include "ro/render/sprite_frame.h"
#include "ro/sim/actor.h"
#include "ro/sim/combat_notice.h"
#include "ro/sim/world.h"

#include "support/actor_client.h"
#include "support/sprite_builder.h"
#include "world/actor_view.h"

using ro::render::Motion;
using ro::render::SpriteLayout;

namespace {

constexpr std::uint32_t kKnight = 41;
constexpr std::uint32_t kPoring = 42;
constexpr std::uint32_t kVendor = 43;
constexpr std::uint16_t kSpiralPierce = 397;
constexpr std::uint16_t kDeathBound = 2003;
constexpr std::uint16_t kMeteorStorm = 83;
constexpr std::int8_t kFrozenState = 11;
constexpr std::uint32_t kStartMs = 5'000;
// Un long moment après : bien au-delà de toute barre et de tout geste.
constexpr std::uint32_t kLongAfterMs = 60'000;

// Un corps de joueur d'essai : deux images par rang, cinq au rang onze.
constexpr std::size_t kRankCount = 13;
constexpr std::size_t kElevenRank = 11;
constexpr std::size_t kElevenFrames = 5;
constexpr std::size_t kOtherFrames = 2;

std::vector<std::uint8_t> BodyAnimationBytes() {
  ro::test_support::ActBuildOptions options;
  for (std::size_t rank = 0; rank < kRankCount; ++rank) {
    const std::size_t frames = rank == kElevenRank ? kElevenFrames : kOtherFrames;
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

struct DressedBody {
  DressedBody()
      : sprite(*ro::formats::SprSprite::Parse(ro::test_support::SpriteBytes())),
        animation(*ro::formats::ActAnimation::Parse(BodyAnimationBytes())),
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

ro::sim::World WorldWith(std::uint32_t id, ro::sim::ActorKind kind) {
  ro::sim::World world;
  ro::sim::Actor actor;
  actor.id = id;
  actor.kind = kind;
  actor.position = ro::sim::CellPosition{20, 20, 0};
  world.Spawn(actor);
  return world;
}

moonclient::MotionEvents NoEvents(std::uint32_t at_ms) {
  return moonclient::MotionEvents{std::nullopt, {}, at_ms};
}

ro::sim::SupportHit AutoCounter(std::int32_t level, bool succeeded) {
  ro::sim::SupportHit hit;
  hit.skill_id = ro::sim::kAutoCounterSkill;
  hit.source_id = kKnight;
  hit.amount = level;
  hit.succeeded = succeeded;
  hit.started_ms = kStartMs;
  return hit;
}

// Le coup ordinaire qu'il a porté avant : de quoi se reposer en garde.
void Swung(ro::sim::World& world, std::uint32_t id, std::uint32_t at_ms) {
  ro::sim::Strike strike;
  strike.attacker = id;
  strike.target = id + 1;
  strike.started_ms = at_ms;
  REQUIRE(world.RecordStrike(strike));
}

std::optional<moonclient::PlayedMotion> UsePosture(DressedBody& body, const ro::sim::Actor& actor,
                                                   std::uint32_t at_ms) {
  return moonclient::SkillUsePostureAt(nullptr, actor, body.outfit, SpriteLayout::kPlayer,
                                       Motion::kIdle, NoEvents(at_ms), at_ms);
}

}  // namespace

TEST_CASE("État 13 : Auto Counter fige la première image de l'attaque, le temps de sa barre",
          "[app][posture]") {
  // 🔴 LE CLIENT POSE UNE GARDE, ET NON UN COUP : la première image du rang
  // d'attaque, tenue quatre cents millisecondes par niveau, puis l'attente.
  // Nous jouions une passe d'attaque, puis la garde.
  DressedBody body;
  ro::sim::World world = WorldWith(kKnight, ro::sim::ActorKind::kPlayer);
  REQUIRE(world.RecordSupport(kKnight, AutoCounter(3, true)));
  const ro::sim::Actor& knight = *world.Find(kKnight);
  constexpr std::uint32_t kBarMs = 1'200;
  const std::optional<moonclient::PlayedMotion> stance =
      UsePosture(body, knight, kStartMs + kBarMs - 1);
  REQUIRE(stance.has_value());
  CHECK(stance->motion == Motion::kAttack);
  CHECK(stance->animation == ro::render::AnimationFor(Motion::kAttack, SpriteLayout::kPlayer));
  CHECK(stance->held_frame == std::optional<std::size_t>{0});
  CHECK_FALSE(stance->loops);
  CHECK_FALSE(UsePosture(body, knight, kStartMs + kBarMs).has_value());
  // Il en sort vers l'attente, pas vers la garde de l'attaque.
  CHECK(moonclient::RestMotionAt(knight, NoEvents(kStartMs + kBarMs), kStartMs + kBarMs) ==
        Motion::kIdle);
}

TEST_CASE("État 13 : un Auto Counter manqué ne prend pas la garde", "[app][posture]") {
  DressedBody body;
  ro::sim::World world = WorldWith(kKnight, ro::sim::ActorKind::kPlayer);
  REQUIRE(world.RecordSupport(kKnight, AutoCounter(3, false)));
  const std::optional<moonclient::PlayedMotion> played =
      UsePosture(body, *world.Find(kKnight), kStartMs + 1);
  CHECK((!played.has_value() || played->held_frame != std::optional<std::size_t>{0}));
}

TEST_CASE("État 11 : Spiral Pierce fige son rang au-delà de la barre", "[app][posture]") {
  // 🔴 LE PAS DU MONDE N'AVANCE PAS CET ÉTAT : image zéro, jusqu'à la motion
  // suivante. Nous jouions le geste une fois, puis le repos.
  REQUIRE(ro::render::SkillImplFor(kSpiralPierce).CastState() == kFrozenState);
  DressedBody body;
  ro::sim::World world = WorldWith(kKnight, ro::sim::ActorKind::kPlayer);
  ro::sim::Cast cast;
  cast.skill_id = kSpiralPierce;
  cast.started_ms = kStartMs;
  cast.duration_ms = 700;
  REQUIRE(world.RecordCast(kKnight, cast));
  const std::optional<moonclient::PlayedMotion> posture =
      moonclient::CastPostureAt(nullptr, *world.Find(kKnight), body.outfit, SpriteLayout::kPlayer,
                                Motion::kIdle, NoEvents(kLongAfterMs), kLongAfterMs);
  REQUIRE(posture.has_value());
  CHECK(posture->frozen);
  CHECK(posture->held_frame == std::optional<std::size_t>{0});
  CHECK(posture->animation ==
        ro::render::ActionAnimationFor(kFrozenState, SpriteLayout::kPlayer, 0));
}

TEST_CASE("État 11 : Death Bound fige son rang jusqu'à la motion suivante", "[app][posture]") {
  REQUIRE(ro::render::SkillUseStateFor(kDeathBound) == kFrozenState);
  DressedBody body;
  ro::sim::World world = WorldWith(kKnight, ro::sim::ActorKind::kPlayer);
  ro::sim::SupportHit hit;
  hit.skill_id = kDeathBound;
  hit.source_id = kKnight;
  hit.succeeded = true;
  hit.started_ms = kStartMs;
  REQUIRE(world.RecordSupport(kKnight, hit));
  const std::optional<moonclient::PlayedMotion> posture =
      UsePosture(body, *world.Find(kKnight), kLongAfterMs);
  REQUIRE(posture.has_value());
  CHECK(posture->frozen);
  CHECK(posture->held_frame == std::optional<std::size_t>{0});
  // Une marche la termine.
  ro::sim::CellMovement walk;
  walk.from = ro::sim::CellPosition{20, 20, 0};
  walk.to = ro::sim::CellPosition{25, 20, 0};
  REQUIRE(world.StartMoving(kKnight, walk, kLongAfterMs - 10));
  CHECK_FALSE(UsePosture(body, *world.Find(kKnight), kLongAfterMs).has_value());
}

TEST_CASE("État 11 : l'action à part d'une compétence de case se dessine sur l'état figé",
          "[app][posture]") {
  // 🔴 SUR UNE MOTION FIGÉE, LE CLIENT DESSINE L'ACTION À PART ELLE-MÊME : le
  // rang onze, direction zéro, sa dernière image — sans la borner au rang figé,
  // qui n'en a que deux.
  DressedBody body;
  ro::sim::World world = WorldWith(kKnight, ro::sim::ActorKind::kPlayer);
  ro::sim::Cast cast;
  cast.skill_id = kSpiralPierce;
  cast.started_ms = kStartMs;
  cast.duration_ms = 700;
  REQUIRE(world.RecordCast(kKnight, cast));
  ro::sim::GroundSkillUse storm;
  storm.skill_id = kMeteorStorm;
  storm.level = 10;
  storm.cell = ro::sim::CellPosition{30, 30, 0};
  storm.started_ms = kStartMs + 300;
  REQUIRE(world.RecordGroundSkill(kKnight, storm));
  const ro::sim::Actor& knight = *world.Find(kKnight);
  const auto posture_at = [&](std::uint32_t at_ms) {
    return moonclient::CastPostureAt(nullptr, knight, body.outfit, SpriteLayout::kPlayer,
                                     Motion::kIdle, NoEvents(at_ms), at_ms)
        .value_or(moonclient::PlayedMotion{});
  };
  const moonclient::MotionAtInstant before = [&](std::uint32_t at_ms,
                                                 std::span<const ro::render::ActorEffectPlay>) {
    return posture_at(at_ms);
  };
  const std::uint32_t now_ms = kStartMs + 5'000;
  const std::optional<moonclient::PlayedMotion> drawn =
      moonclient::GroundSkillFrozenImageAt(nullptr, knight, body.outfit, SpriteLayout::kPlayer,
                                           posture_at(now_ms), NoEvents(now_ms), now_ms, before);
  REQUIRE(drawn.has_value());
  CHECK(drawn->animation == ro::render::kGroundSkillHeldImageAnimation);
  CHECK(drawn->held_direction ==
        std::optional<std::size_t>{ro::render::kGroundSkillHeldImageDirection});
  CHECK(drawn->held_frame == std::optional<std::size_t>{kElevenFrames - 1});
  // L'image que la même compétence borne sur une motion qui tient la sienne ne
  // s'applique pas ici.
  CHECK_FALSE(moonclient::GroundSkillHeldFrameAt(nullptr, knight, body.outfit,
                                                 SpriteLayout::kPlayer, posture_at(now_ms),
                                                 NoEvents(now_ms), now_ms, before)
                  .has_value());
}

TEST_CASE("Sommeil : une créature se fige sur la première image de son attente", "[app][posture]") {
  ro::sim::World world = WorldWith(kPoring, ro::sim::ActorKind::kMonster);
  REQUIRE(world.SetStates(kPoring, ro::sim::kBodyAsleep, 0, 0, kStartMs, false));
  const auto asleep_at = [&](std::uint32_t at_ms) {
    return moonclient::AsleepCreatureMotionAt(nullptr, *world.Find(kPoring),
                                              SpriteLayout::kCreature, NoEvents(at_ms), at_ms);
  };
  const std::optional<moonclient::PlayedMotion> asleep = asleep_at(kStartMs + 2'000);
  REQUIRE(asleep.has_value());
  CHECK(asleep->motion == Motion::kIdle);
  CHECK(asleep->held_frame == std::optional<std::size_t>{0});
  CHECK_FALSE(asleep->loops);
  // Le réveil la rend à son attente.
  REQUIRE(world.SetStates(kPoring, 0, 0, 0, kStartMs + 3'000, false));
  CHECK_FALSE(asleep_at(kStartMs + 3'500).has_value());
  // Un PNJ ne se fige pas.
  ro::sim::World town = WorldWith(kVendor, ro::sim::ActorKind::kNonPlayer);
  REQUIRE(town.SetStates(kVendor, ro::sim::kBodyAsleep, 0, 0, kStartMs, false));
  CHECK_FALSE(moonclient::AsleepCreatureMotionAt(nullptr, *town.Find(kVendor),
                                                 SpriteLayout::kCreature, NoEvents(kStartMs + 10),
                                                 kStartMs + 10)
                  .has_value());
}

TEST_CASE("Sommeil : le réveil d'un joueur en garde le rend à l'attente", "[app][posture]") {
  // Le sommeil laisse la garde telle qu'elle était ; le réveil repose le repos
  // en quittant le sommeil.
  ro::sim::World world = WorldWith(kKnight, ro::sim::ActorKind::kPlayer);
  Swung(world, kKnight, kStartMs);
  REQUIRE(world.SetStates(kKnight, ro::sim::kBodyAsleep, 0, 0, kStartMs + 1'000, false));
  const auto rest_at = [&](std::uint32_t at_ms) {
    return moonclient::RestMotionAt(*world.Find(kKnight), NoEvents(at_ms), at_ms);
  };
  CHECK(rest_at(kStartMs + 2'000) == Motion::kReady);
  REQUIRE(world.SetStates(kKnight, 0, 0, 0, kStartMs + 3'000, false));
  CHECK(rest_at(kStartMs + 3'500) == Motion::kIdle);
}

TEST_CASE("Arrêt : l'arrêt qui fait glisser quitte la marche, et la garde avec elle",
          "[app][posture]") {
  ro::sim::World world = WorldWith(kKnight, ro::sim::ActorKind::kPlayer);
  Swung(world, kKnight, kStartMs);
  const auto rest_at = [&](std::uint32_t at_ms) {
    return moonclient::RestMotionAt(*world.Find(kKnight), NoEvents(at_ms), at_ms);
  };
  // Arrêté sur sa propre case : il ne glisse pas, et garde sa garde.
  REQUIRE(world.PlaceAt(kKnight, ro::sim::CellPosition{20, 20, 0}, kStartMs + 500));
  CHECK(rest_at(kStartMs + 600) == Motion::kReady);
  // Arrêté ailleurs : il glisse, puis se repose en attente.
  REQUIRE(world.PlaceAt(kKnight, ro::sim::CellPosition{22, 20, 0}, kStartMs + 1'000));
  CHECK(rest_at(kStartMs + 1'100) == Motion::kIdle);
}

TEST_CASE("Attente : cinq comptes la déroulent au lieu de tenir le regard", "[app][posture]") {
  ro::sim::Actor player;
  player.kind = ro::sim::ActorKind::kPlayer;
  player.id = 2'000'001;
  CHECK(moonclient::IdleHoldsLookImage(player));
  for (const std::uint32_t account : {2919549U, 2919550U, 2919551U, 2919552U, 2919554U}) {
    player.id = account;
    CHECK_FALSE(moonclient::IdleHoldsLookImage(player));
  }
  player.id = 2919553;
  CHECK(moonclient::IdleHoldsLookImage(player));
}

TEST_CASE("Vignette : un joueur au repos ou assis regarde droit, et marche", "[app][posture]") {
  // 🔴 LES TROIS IMAGES DU REPOS ET DE L'ASSISE SONT LE REGARD : les dérouler
  // fait dire non au pantin d'un aperçu.
  ro::sim::Actor player;
  player.kind = ro::sim::ActorKind::kPlayer;
  player.id = 2'000'001;
  CHECK(moonclient::VignetteHeldFrame(player, ro::render::kAnimationIdle) ==
        moonclient::kLookAheadImage);
  CHECK(moonclient::VignetteHeldFrame(player, ro::render::kAnimationSit) ==
        moonclient::kLookAheadImage);
  CHECK_FALSE(moonclient::VignetteHeldFrame(player, ro::render::kAnimationWalk).has_value());
  // Un monstre déroule son attente : c'est une vraie animation.
  ro::sim::Actor monster;
  monster.kind = ro::sim::ActorKind::kMonster;
  monster.id = 110'001;
  CHECK_FALSE(moonclient::VignetteHeldFrame(monster, ro::render::kAnimationIdle).has_value());
}
