// CE QU'UN ÉTAT D'ACTION FAIT D'UNE CRÉATURE : rien le plus souvent, un rang
// pour une poignée d'états, une pose figée pour ceux que le pas du monde
// n'avance pas — et la direction figée avec elle —, le sommeil et ses bulles.

#include "ro/render/creature_motion.h"

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
#include "ro/render/skill_impl.h"
#include "ro/render/skill_use_motion.h"
#include "ro/render/sleep_bubbles.h"
#include "ro/render/sprite_frame.h"
#include "ro/sim/actor.h"
#include "ro/sim/combat_notice.h"
#include "ro/sim/status_effects.h"
#include "ro/sim/world.h"

#include "support/actor_client.h"
#include "support/sprite_builder.h"
#include "world/actor_view.h"
#include "world/frozen_poses.h"

using ro::render::CreatureMotionFamily;
using ro::render::Motion;
using ro::render::SpriteLayout;

namespace {

constexpr std::uint32_t kPoring = 42;
constexpr std::uint32_t kLif = 43;
constexpr std::uint32_t kVendor = 44;
constexpr std::uint32_t kPoringJob = 1002;
constexpr std::uint32_t kLifJob = 6001;
constexpr std::uint32_t kVendorJob = 86;
constexpr std::uint16_t kFireBolt = 19;
constexpr std::uint16_t kSpiralPierce = 397;
constexpr std::uint16_t kHealingHands = 8001;
constexpr std::int8_t kCastState = 8;
constexpr std::int8_t kFrozenState = 11;
constexpr std::int8_t kUseState = 7;
constexpr std::uint32_t kStartMs = 5'000;
constexpr std::uint32_t kCastMs = 1'500;
constexpr std::uint32_t kLongAfterMs = 60'000;
constexpr std::size_t kRankCount = 13;
constexpr std::size_t kSkillRank = 5;

struct DressedCreature {
  DressedCreature()
      : sprite(*ro::formats::SprSprite::Parse(ro::test_support::SpriteBytes())),
        animation(*ro::formats::ActAnimation::Parse(ro::test_support::AnimationBytes(kRankCount))),
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

ro::sim::World WorldWith(std::uint32_t id, ro::sim::ActorKind kind, ro::sim::Familiar familiar,
                         std::uint32_t job) {
  ro::sim::World world;
  ro::sim::Actor actor;
  actor.id = id;
  actor.kind = kind;
  actor.familiar = familiar;
  actor.look.job = static_cast<std::uint16_t>(job);
  actor.position = ro::sim::CellPosition{20, 20, 0};
  world.Spawn(actor);
  return world;
}

moonclient::MotionEvents NoEvents(std::uint32_t at_ms) {
  return moonclient::MotionEvents{std::nullopt, {}, at_ms};
}

void Casts(ro::sim::World& world, std::uint32_t id, std::uint16_t skill_id) {
  ro::sim::Cast cast;
  cast.skill_id = skill_id;
  cast.started_ms = kStartMs;
  cast.duration_ms = kCastMs;
  REQUIRE(world.RecordCast(id, cast));
}

std::optional<moonclient::PlayedMotion> CastPosture(DressedCreature& body,
                                                    const ro::sim::Actor& actor,
                                                    std::uint32_t at_ms) {
  return moonclient::CastPostureAt(nullptr, actor, body.outfit, SpriteLayout::kCreature,
                                   Motion::kIdle, NoEvents(at_ms), at_ms);
}

}  // namespace

TEST_CASE("Créature : l'incantation ordinaire la laisse continuer ce qu'elle jouait",
          "[app][creature]") {
  // 🔴 LE CLIENT N'A PAS DE POSE D'INCANTATION POUR UNE CRÉATURE : ni rang, ni
  // image. Nous la figions sur la première image de son attente.
  REQUIRE(ro::render::SkillImplFor(kFireBolt).CastState() == kCastState);
  DressedCreature body;
  ro::sim::World world =
      WorldWith(kPoring, ro::sim::ActorKind::kMonster, ro::sim::Familiar::kNone, kPoringJob);
  Casts(world, kPoring, kFireBolt);
  CHECK_FALSE(CastPosture(body, *world.Find(kPoring), kStartMs + 100).has_value());
}

TEST_CASE("Créature : l'état onze la fige là où elle en est, au-delà de la barre",
          "[app][creature]") {
  REQUIRE(ro::render::SkillImplFor(kSpiralPierce).CastState() == kFrozenState);
  DressedCreature body;
  ro::sim::World world =
      WorldWith(kPoring, ro::sim::ActorKind::kMonster, ro::sim::Familiar::kNone, kPoringJob);
  Casts(world, kPoring, kSpiralPierce);
  const std::optional<moonclient::PlayedMotion> posture =
      CastPosture(body, *world.Find(kPoring), kLongAfterMs);
  REQUIRE(posture.has_value());
  CHECK(posture->frozen);
  CHECK(posture->keeps_shown_image);
  CHECK(posture->frozen_since_ms == kStartMs);
}

TEST_CASE("Créature : l'emploi d'un homoncule joue le rang cinq, celui d'un PNJ rien",
          "[app][creature]") {
  REQUIRE(ro::render::SkillUseStateFor(kHealingHands) == kUseState);
  DressedCreature body;
  ro::sim::World world =
      WorldWith(kLif, ro::sim::ActorKind::kNonPlayer, ro::sim::Familiar::kHomunculus, kLifJob);
  ro::sim::SupportHit hit;
  hit.skill_id = kHealingHands;
  hit.source_id = kLif;
  hit.succeeded = true;
  hit.started_ms = kStartMs;
  REQUIRE(world.RecordSupport(kLif, hit));
  const std::optional<moonclient::PlayedMotion> posture = moonclient::SkillUsePostureAt(
      nullptr, *world.Find(kLif), body.outfit, SpriteLayout::kCreature, Motion::kIdle,
      NoEvents(kStartMs + 10), kStartMs + 10);
  REQUIRE(posture.has_value());
  CHECK(posture->animation == kSkillRank);

  CHECK(ro::render::CreatureActionAnimationFor(kUseState, CreatureMotionFamily::kMercenary, 0) ==
        std::optional{ro::render::kCreatureAnimationAttack});
  CHECK_FALSE(ro::render::CreatureActionAnimationFor(kUseState, CreatureMotionFamily::kOrdinary, 0)
                  .has_value());
  CHECK(ro::render::CreatureActionAnimationFor(kCastState, CreatureMotionFamily::kNpc, 0) ==
        std::optional{ro::render::kAnimationIdle});
  CHECK_FALSE(ro::render::CreatureActionAnimationFor(kCastState, CreatureMotionFamily::kOrdinary, 0)
                  .has_value());
}

TEST_CASE("Créature : le PNJ, la tombe et le PNJ au sprite de monstre", "[app][creature]") {
  ro::sim::Actor vendor;
  vendor.kind = ro::sim::ActorKind::kNonPlayer;
  vendor.look.job = kVendorJob;
  CHECK(moonclient::CreatureFamilyOf(vendor) == CreatureMotionFamily::kNpc);
  vendor.object_type = 0xC;
  CHECK(moonclient::CreatureFamilyOf(vendor) == CreatureMotionFamily::kOrdinary);
  ro::sim::Actor bionic;
  bionic.kind = ro::sim::ActorKind::kMonster;
  bionic.object_type = 0xE;
  bionic.look.job = kPoringJob;
  CHECK(moonclient::CreatureFamilyOf(bionic) == CreatureMotionFamily::kSkillRank);
}

TEST_CASE("Pose figée : la direction et l'image de l'instant où elle se fige", "[app][creature]") {
  // 🔴 NI UN REGARD, NI LA CAMÉRA : le client ne recalcule plus l'action d'un
  // état figé. Nous la tournions avec eux.
  moonclient::FrozenPoses poses;
  poses.NoteShown(kPoring, {ro::render::kAnimationWalk, 3, 1});
  const moonclient::FrozenPoses::Shown held =
      poses.Hold(kPoring, kStartMs, true, {ro::render::kAnimationIdle, 5, 0});
  CHECK(held.animation == ro::render::kAnimationWalk);
  CHECK(held.frame == 1);
  CHECK(held.direction == 5);
  poses.NoteShown(kPoring, {held.animation, held.direction, held.frame});
  // La caméra pivote, l'acteur se tourne : rien ne bouge.
  const moonclient::FrozenPoses::Shown later =
      poses.Hold(kPoring, kStartMs, true, {ro::render::kAnimationIdle, 1, 0});
  CHECK(later.direction == 5);
  CHECK(later.frame == 1);
  // Une pose neuve se note à nouveau ; un joueur y garde son rang et son image.
  const moonclient::FrozenPoses::Shown fresh = poses.Hold(kPoring, kStartMs + 1, false, {4, 2, 0});
  CHECK(fresh.animation == 4);
  CHECK(fresh.direction == 2);
}

TEST_CASE("Sommeil : un homoncule se fige, un PNJ non", "[app][creature]") {
  // Le PNJ se range par son GENRE, et le familier n'en est pas un : nous
  // laissions l'homoncule à son attente.
  ro::sim::World world =
      WorldWith(kLif, ro::sim::ActorKind::kNonPlayer, ro::sim::Familiar::kHomunculus, kLifJob);
  REQUIRE(world.SetStates(kLif, ro::sim::kBodyAsleep, 0, 0, kStartMs, false));
  CHECK(moonclient::AsleepCreatureMotionAt(nullptr, *world.Find(kLif), SpriteLayout::kCreature,
                                           NoEvents(kStartMs + 10), kStartMs + 10)
            .has_value());
  ro::sim::World town =
      WorldWith(kVendor, ro::sim::ActorKind::kNonPlayer, ro::sim::Familiar::kNone, kVendorJob);
  REQUIRE(town.SetStates(kVendor, ro::sim::kBodyAsleep, 0, 0, kStartMs, false));
  CHECK_FALSE(moonclient::AsleepCreatureMotionAt(nullptr, *town.Find(kVendor),
                                                 SpriteLayout::kCreature, NoEvents(kStartMs + 10),
                                                 kStartMs + 10)
                  .has_value());
}

TEST_CASE("Sommeil : le sommeil profond endort comme l'état du corps", "[app][creature]") {
  ro::sim::World world =
      WorldWith(kPoring, ro::sim::ActorKind::kMonster, ro::sim::Familiar::kNone, kPoringJob);
  REQUIRE(world.SetStatusEffect(kPoring, ro::sim::kEfstDeepSleep, true, std::nullopt, std::nullopt,
                                kStartMs));
  CHECK(world.Find(kPoring)->asleep_since_ms == std::optional{kStartMs});
  // L'état du corps qui s'y ajoute n'endort pas une seconde fois.
  REQUIRE(world.SetStates(kPoring, ro::sim::kBodyAsleep, 0, 0, kStartMs + 100, false));
  CHECK(world.Find(kPoring)->asleep_since_ms == std::optional{kStartMs});
  // Le premier qui cesse réveille.
  REQUIRE(world.SetStates(kPoring, 0, 0, 0, kStartMs + 500, false));
  CHECK_FALSE(world.Find(kPoring)->asleep_since_ms.has_value());
  CHECK(world.Find(kPoring)->awoke_ms == std::optional{kStartMs + 500});
  REQUIRE(world.SetStatusEffect(kPoring, ro::sim::kEfstDeepSleep, false, std::nullopt, std::nullopt,
                                kStartMs + 900));
  CHECK(world.Find(kPoring)->awoke_ms == std::optional{kStartMs + 500});
}

TEST_CASE("Sommeil : les bulles tournent à la cadence de leur fichier", "[app][creature]") {
  const ro::formats::SprSprite sprite =
      *ro::formats::SprSprite::Parse(ro::test_support::SpriteBytes());
  const ro::formats::ActAnimation animation =
      *ro::formats::ActAnimation::Parse(ro::test_support::AnimationBytes(1));
  const auto frame_at = [&](float asleep_ms) {
    return ro::render::ComposeSleepBubbles(sprite, animation, asleep_ms).frame_index;
  };
  CHECK(frame_at(0.0F) == 0);
  CHECK(frame_at(ro::test_support::kFrameIntervalMs - 1.0F) == 0);
  CHECK(frame_at(ro::test_support::kFrameIntervalMs) == 1);
  // Deux images, en boucle.
  CHECK(frame_at(2.0F * ro::test_support::kFrameIntervalMs) == 0);
  CHECK_FALSE(ro::render::ComposeSleepBubbles(sprite, animation, 0.0F).quads.empty());
}

TEST_CASE("Créature : sans geste de table, l'emploi d'une attaque la fait frapper",
          "[app][creature]") {
  // Magnum Break a l'attaque pour état d'emploi ; Power Up, un état qu'une
  // créature ne joue pas.
  constexpr std::uint16_t kMagnumBreak = 7;
  constexpr std::uint16_t kNpcPowerUp = 349;
  constexpr std::int8_t kAttackState = 2;
  REQUIRE(ro::render::SkillUseStateFor(kMagnumBreak) == kAttackState);
  REQUIRE(ro::render::SkillUseStateFor(kNpcPowerUp) != kAttackState);
  for (const std::uint16_t skill_id : {kMagnumBreak, kNpcPowerUp}) {
    DressedCreature body;
    ro::sim::World world =
        WorldWith(kPoring, ro::sim::ActorKind::kMonster, ro::sim::Familiar::kNone, kPoringJob);
    ro::sim::SupportHit hit;
    hit.skill_id = skill_id;
    hit.source_id = kPoring;
    hit.succeeded = true;
    hit.started_ms = kStartMs;
    REQUIRE(world.RecordSupport(kPoring, hit));
    const std::optional<moonclient::PlayedMotion> posture = moonclient::SkillUsePostureAt(
        nullptr, *world.Find(kPoring), body.outfit, SpriteLayout::kCreature, Motion::kIdle,
        NoEvents(kStartMs + 10), kStartMs + 10);
    INFO("compétence " << skill_id);
    if (skill_id == kMagnumBreak) {
      REQUIRE(posture.has_value());
      CHECK(posture->animation == ro::render::kCreatureAnimationAttack);
    } else {
      CHECK_FALSE(posture.has_value());
    }
  }
}
