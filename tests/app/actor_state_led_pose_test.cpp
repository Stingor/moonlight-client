// LES ÉTATS D'ACTION QUI MÈNENT LEUR POSE, au chemin de l'emploi et du coup :
// la garde du taekwon, le coup de pied descendant qui joue l'attaque, le
// tirage de l'état 38, les poses qui finissent au repos — et les effets que
// trois familles d'états font naître sur leur porteur en se posant.

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/formats/act.h"
#include "ro/formats/gnd.h"
#include "ro/formats/rsw.h"
#include "ro/formats/spr.h"
#include "ro/render/actor_sprite.h"
#include "ro/render/actor_wardrobe.h"
#include "ro/render/effect_catalog.h"
#include "ro/render/led_pose.h"
#include "ro/render/lightmap_atlas.h"
#include "ro/render/map_scene.h"
#include "ro/render/scene_light.h"
#include "ro/render/skill_impl.h"
#include "ro/render/skill_use_motion.h"
#include "ro/render/sprite_frame.h"
#include "ro/sim/actor.h"
#include "ro/sim/combat_notice.h"
#include "ro/sim/walk_path.h"
#include "ro/sim/world.h"
#include "ro/vfs/vfs.h"

#include "effects/effect_node_view.h"
#include "effects/effect_view.h"
#include "support/actor_client.h"
#include "support/client_vfs.h"
#include "support/map_builder.h"
#include "support/temp_directory.h"
#include "world/actor_view.h"
#include "world/map_stage.h"

using moonclient::MotionEvents;
using ro::render::Motion;
using ro::render::SpriteLayout;
using ro::test_support::EmptyClient;

namespace {

constexpr std::uint16_t kTaekwon = 4046;
constexpr std::uint16_t kSummoner = 4218;
// Les compétences d'état : la garde pivotante (23), le coup de pied
// descendant (31), une garde d'orage (21), le jet du canonnier (38), et les
// deux emplois qui finissent au repos (49, 51).
constexpr std::uint16_t kReadyTurn = 416;
constexpr std::uint16_t kDownKick = 415;
constexpr std::uint16_t kReadyStorm = 412;
constexpr std::uint16_t kFling = 501;
constexpr std::uint16_t kScratch = 5021;
constexpr std::uint16_t kFlashKick = 2580;
// L'incantation d'état 19.
constexpr std::uint16_t kLedCastSkill = 426;
constexpr std::uint32_t kStartMs = 10'000;
constexpr std::uint32_t kTickMs = 16;
constexpr std::size_t kCastRank = 12;
constexpr std::size_t kAttackRank = 10;
constexpr std::uint32_t kCasterId = 160'041;
constexpr std::int32_t kGroundCells = 4;
constexpr ro::sim::CellPosition kCasterCell{.x = 1, .y = 1};

ro::sim::Actor Employing(std::uint16_t job, std::uint16_t skill, std::uint32_t started_ms) {
  ro::sim::Actor subject;
  subject.id = 1;
  subject.kind = ro::sim::ActorKind::kPlayer;
  subject.look.job = job;
  subject.skill_use = ro::sim::SkillUse{skill, started_ms};
  return subject;
}

// Un corps de joueur de treize rangs, deux images chacun.
struct Body {
  Body()
      : sprite(*ro::formats::SprSprite::Parse(ro::test_support::SpriteBytes())),
        animation(
            *ro::formats::ActAnimation::Parse(ro::test_support::AnimationBytes(kPlayerAnimations))),
        outfit(Pieces(sprite, animation)) {}

  static constexpr std::size_t kPlayerAnimations = 13;

  static std::vector<ro::render::ActorPiece> Pieces(const ro::formats::SprSprite& body_sprite,
                                                    const ro::formats::ActAnimation& body_act) {
    ro::render::ActorPiece body;
    body.sprite = &body_sprite;
    body.animation = &body_act;
    body.part = ro::render::ActorPart::kBody;
    return {body};
  }

  std::optional<moonclient::PlayedMotion> UseAt(const ro::sim::Actor& actor, std::uint32_t at_ms) {
    return moonclient::SkillUsePostureAt(nullptr, actor, outfit, SpriteLayout::kPlayer,
                                         Motion::kIdle, MotionEvents{}, at_ms);
  }

  // L'image tenue que l'emploi montre, ou rien quand il ne montre rien.
  std::optional<std::size_t> HeldFrameAt(const ro::sim::Actor& actor, std::uint32_t at_ms) {
    const std::optional<moonclient::PlayedMotion> played = UseAt(actor, at_ms);
    return played.has_value() ? played->held_frame : std::nullopt;
  }

  ro::formats::SprSprite sprite;
  ro::formats::ActAnimation animation;
  ro::render::ActorOutfit outfit;
};

std::optional<ro::render::HeldPoseFrame> LedAt(const ro::sim::Actor& actor, std::uint32_t at_ms) {
  return moonclient::LedPoseAt(nullptr, actor, SpriteLayout::kPlayer, Motion::kIdle, MotionEvents{},
                               at_ms);
}

}  // namespace

TEST_CASE("État mené à l'emploi : la garde pivotante tient l'image 3, même après ses deux secondes",
          "[app][posture]") {
  // 🔴 L'ÉTAT 23 NE DÉROULE PAS SON RANG : le client en montre l'image 3 deux
  // secondes, puis la laisse jusqu'à la motion suivante. Nous jouions une
  // passe du rang d'incantation, puis le repos.
  REQUIRE(ro::render::SkillUseStateFor(kReadyTurn) == 23);
  Body body;
  const ro::sim::Actor taekwon = Employing(kTaekwon, kReadyTurn, kStartMs);
  for (const std::uint32_t after_ms : {0U, 100U, 1'999U, 2'000U, 10'000U}) {
    const std::optional<moonclient::PlayedMotion> held = body.UseAt(taekwon, kStartMs + after_ms);
    REQUIRE(held.has_value());
    CHECK(held->animation == kCastRank);
    CHECK(held->held_frame == std::optional<std::size_t>{3});
    CHECK_FALSE(held->loops);
  }
  const auto led = LedAt(taekwon, kStartMs + 100);
  REQUIRE(led.has_value());
  CHECK(led->frame == 3);
  // La blessure reste écartée deux secondes, pas davantage.
  CHECK(moonclient::LedPoseHoldsAt(nullptr, taekwon, SpriteLayout::kPlayer, Motion::kIdle, {},
                                   kStartMs + 1'999));
  CHECK_FALSE(moonclient::LedPoseHoldsAt(nullptr, taekwon, SpriteLayout::kPlayer, Motion::kIdle, {},
                                         kStartMs + 2'000));
  // Une marche la termine.
  ro::sim::Actor walking = taekwon;
  walking.movement = ro::sim::CellMovement{};
  walking.movement_started_ms = kStartMs + 3'000;
  CHECK_FALSE(body.UseAt(walking, kStartMs + 3'100).has_value());
}

TEST_CASE("État mené à l'emploi : le coup de pied descendant joue l'attaque, sans rang d'état",
          "[app][posture]") {
  // 🔴 L'ÉTAT 31 N'A PAS DE RANG DANS LA TABLE, et nous ne montrions rien : le
  // client impose l'action d'attaque, images 3, 5, 7, 8 et 2, cinq avances
  // chacune.
  REQUIRE(ro::render::SkillUseStateFor(kDownKick) == 31);
  REQUIRE_FALSE(ro::render::ActionAnimationFor(31, SpriteLayout::kPlayer, kTaekwon).has_value());
  Body body;
  const ro::sim::Actor taekwon = Employing(kTaekwon, kDownKick, kStartMs);
  constexpr std::uint32_t kStepMs = 5 * kTickMs;
  constexpr std::array<std::size_t, 5> kFrames = {3, 5, 7, 8, 2};
  for (std::size_t step = 0; step < kFrames.size(); ++step) {
    const std::optional<moonclient::PlayedMotion> kick =
        body.UseAt(taekwon, kStartMs + static_cast<std::uint32_t>(step) * kStepMs);
    REQUIRE(kick.has_value());
    CHECK(kick->animation == kAttackRank);
    CHECK(kick->held_frame == std::optional<std::size_t>{kFrames[step]});
  }
  // Et l'emploi compte comme un changement de motion : il termine ce qui
  // précédait.
  ro::sim::Actor after_cast = taekwon;
  ro::sim::Cast cast;
  cast.skill_id = kLedCastSkill;
  cast.started_ms = kStartMs - 500;
  cast.duration_ms = 5'000;
  after_cast.cast = cast;
  CHECK(moonclient::MotionChangedSince(nullptr, after_cast, SpriteLayout::kPlayer, Motion::kCast,
                                       MotionEvents{},
                                       moonclient::ChangeWindow{kStartMs - 500, kStartMs, true}));
}

TEST_CASE("État mené à l'emploi : le tirage de l'état 38 se lit sur l'instant de l'emploi",
          "[app][posture]") {
  // Le client tire rand() & 1 en posant l'état : nous prenons la parité de
  // l'instant d'annonce, qui ne change pas d'une image à l'autre.
  REQUIRE(ro::render::SkillUseStateFor(kFling) == 38);
  Body body;
  constexpr std::uint32_t kStepMs = 6 * kTickMs;
  const ro::sim::Actor even = Employing(kTaekwon, kFling, kStartMs);
  const ro::sim::Actor odd = Employing(kTaekwon, kFling, kStartMs + 1);
  CHECK(body.HeldFrameAt(even, kStartMs) == std::optional<std::size_t>{0});
  CHECK(body.HeldFrameAt(even, kStartMs + 3 * kStepMs) == std::optional<std::size_t>{3});
  CHECK(body.HeldFrameAt(odd, kStartMs + 1) == std::optional<std::size_t>{3});
  CHECK(body.HeldFrameAt(odd, kStartMs + 1 + 3 * kStepMs) == std::optional<std::size_t>{0});
}

TEST_CASE("État mené à l'emploi : 49 et 51 finissent au repos, 51 en garde", "[app][posture]") {
  // 🔴 LEUR POSE ÉCHUE, LE CLIENT REPOSE L'ACTEUR : 49 s'inscrit comme
  // l'emploi ordinaire, qui se repose en attente ; 51 passe à l'attaque, qui se
  // repose en garde.
  REQUIRE(ro::render::SkillUseStateFor(kScratch) == 49);
  REQUIRE(ro::render::SkillUseStateFor(kFlashKick) == 51);
  Body body;
  const ro::sim::Actor forty_nine = Employing(0, kScratch, kStartMs);
  CHECK(body.UseAt(forty_nine, kStartMs + 999).has_value());
  CHECK_FALSE(body.UseAt(forty_nine, kStartMs + 1'000).has_value());
  CHECK_FALSE(LedAt(forty_nine, kStartMs + 1'000).has_value());
  CHECK(moonclient::RestMotionAt(forty_nine, MotionEvents{}, kStartMs + 1'000) == Motion::kIdle);

  const ro::sim::Actor fifty_one = Employing(0, kFlashKick, kStartMs);
  CHECK(body.HeldFrameAt(fifty_one, kStartMs + 499) == std::optional<std::size_t>{4});
  CHECK_FALSE(body.UseAt(fifty_one, kStartMs + 500).has_value());
  CHECK(moonclient::RestMotionAt(fifty_one, MotionEvents{}, kStartMs + 500) == Motion::kReady);
}

TEST_CASE("État mené au coup : le coup de pied pivotant mène sa pose", "[app][posture]") {
  // Le coup pose l'état d'emploi par la même motion : l'état 32 mène les
  // images 4, 5, 6, 7 et 2 de son rang onze, et la dernière reste.
  constexpr std::uint16_t kTurnKick = 417;
  REQUIRE(ro::render::SkillUseStateFor(kTurnKick) == 32);
  ro::sim::Actor taekwon = Employing(kTaekwon, kTurnKick, kStartMs);
  taekwon.skill_use.reset();
  ro::sim::Strike kick;
  kick.attacker = 1;
  kick.target = 2;
  kick.skill_id = kTurnKick;
  kick.started_ms = kStartMs;
  taekwon.attack = kick;
  constexpr std::uint32_t kStepMs = 5 * kTickMs;
  const auto frame_at = [&taekwon](std::uint32_t at_ms) -> std::optional<int> {
    const std::optional<ro::render::HeldPoseFrame> led = LedAt(taekwon, at_ms);
    return led.has_value() ? std::optional<int>{led->frame} : std::nullopt;
  };
  REQUIRE(LedAt(taekwon, kStartMs).has_value());
  CHECK(LedAt(taekwon, kStartMs)->rank == 11);
  CHECK(frame_at(kStartMs) == std::optional<int>{4});
  CHECK(frame_at(kStartMs + kStepMs) == std::optional<int>{5});
  CHECK(frame_at(kStartMs + 4'000) == std::optional<int>{2});
  CHECK_FALSE(moonclient::StrikeFinishedByLedPose(taekwon, SpriteLayout::kPlayer, Motion::kIdle,
                                                  kStartMs + 4'000));
  // Un coup d'état 51 finit sa passe avec sa pose.
  taekwon.attack->skill_id = kFlashKick;
  CHECK_FALSE(moonclient::StrikeFinishedByLedPose(taekwon, SpriteLayout::kPlayer, Motion::kIdle,
                                                  kStartMs + 499));
  CHECK(moonclient::StrikeFinishedByLedPose(taekwon, SpriteLayout::kPlayer, Motion::kIdle,
                                            kStartMs + 500));
}

namespace {

moonclient::MapStage FlatStage() {
  ro::test_support::GndBuildOptions ground;
  ground.width = kGroundCells;
  ground.height = kGroundCells;
  auto gnd = ro::formats::GndMap::Parse(ro::test_support::BuildGnd(ground));
  REQUIRE(gnd.has_value());
  auto rsw = ro::formats::RswScene::Parse(ro::test_support::BuildRsw({}));
  REQUIRE(rsw.has_value());
  ro::render::LightmapAtlas atlas = ro::render::BuildLightmapAtlas(*gnd);
  const ro::render::SceneLight light = ro::render::MakeSceneLight(rsw->light());
  return moonclient::MapStage{.name = "plate",
                              .scene = ro::render::MapScene{
                                  .gnd = std::move(*gnd),
                                  .rsw = std::move(*rsw),
                                  .atlas = std::move(atlas),
                                  .light = light,
                              }};
}

struct Bench {
  explicit Bench(std::uint16_t job) : vfs(EmptyClient(client)) {
    ro::sim::Actor caster;
    caster.id = kCasterId;
    caster.kind = ro::sim::ActorKind::kPlayer;
    caster.look.job = job;
    caster.position = kCasterCell;
    world.Spawn(caster);
  }

  // Ce que les effets posent sur le lanceur, et ce qu'ils lui retirent.
  std::vector<std::pair<std::uint32_t, bool>> ChangesOnCaster() {
    std::vector<std::pair<std::uint32_t, bool>> changes;
    for (const moonclient::EffectsInPlay::ActorEffectToRecord& change :
         effects.TakeActorEffects()) {
      if (change.actor_id == kCasterId) {
        changes.emplace_back(change.effect_id, change.withdrawn);
      }
    }
    return changes;
  }

  bool Launched(std::uint32_t effect_id) {
    const std::vector<moonclient::LaunchedEffect> launched = effects.TakeLaunchedEffects();
    return std::ranges::any_of(launched, [effect_id](const moonclient::LaunchedEffect& born) {
      return born.effect_id == effect_id && born.bearer_id == kCasterId;
    });
  }

  ro::test_support::TempDirectory client;
  ro::vfs::Vfs vfs;
  ro::render::EffectCatalog catalog{vfs};
  moonclient::EffectNodesInPlay nodes{vfs};
  moonclient::EffectsInPlay effects{catalog, nodes, vfs};
  ro::sim::World world;
  moonclient::MapStage stage = FlatStage();
  ro::sim::WalkPlanner planner{moonclient::PlannedMapOf(stage)};
};

bool Contains(const std::vector<std::pair<std::uint32_t, bool>>& changes, std::uint32_t effect_id,
              bool withdrawn) {
  return std::ranges::find(changes, std::pair{effect_id, withdrawn}) != changes.end();
}

}  // namespace

TEST_CASE("Effet d'état : la garde d'orage fait naître 449 sur le lanceur", "[app][effets]") {
  // 🔴 RIEN NE LE FAISAIT NAÎTRE : le client le pose avec l'état 21 à 25, à
  // l'emploi comme au coup, et aucune suite de compétence ne le nomme.
  Bench bench(kTaekwon);
  ro::sim::SupportHit hit;
  hit.skill_id = kReadyStorm;
  hit.source_id = kCasterId;
  hit.succeeded = true;
  hit.started_ms = kStartMs;
  REQUIRE(bench.world.RecordSupport(kCasterId, hit));
  bench.effects.NoticeSkills(bench.world, bench.stage, bench.planner, kStartMs);
  CHECK(Contains(bench.ChangesOnCaster(), 449, false));
  // Une seule fois par emploi.
  bench.effects.NoticeSkills(bench.world, bench.stage, bench.planner, kStartMs + 16);
  CHECK_FALSE(Contains(bench.ChangesOnCaster(), 449, false));
}

TEST_CASE("Effet d'état : l'incantation d'état 19 retire puis repose, sauf chez un Doram",
          "[app][effets]") {
  REQUIRE(ro::render::SkillImplFor(kLedCastSkill).CastState() == 19);
  ro::sim::Cast cast;
  cast.skill_id = kLedCastSkill;
  cast.started_ms = kStartMs;
  cast.duration_ms = 1'000;

  Bench taekwon(kTaekwon);
  REQUIRE(taekwon.world.RecordCast(kCasterId, cast));
  taekwon.effects.NoticeSkills(taekwon.world, taekwon.stage, taekwon.planner, kStartMs);
  const std::vector<std::pair<std::uint32_t, bool>> changes = taekwon.ChangesOnCaster();
  CHECK(Contains(changes, 446, true));
  CHECK(Contains(changes, 445, true));
  CHECK(Contains(changes, 445, false));
  CHECK(taekwon.Launched(411));

  Bench summoner(kSummoner);
  REQUIRE(summoner.world.RecordCast(kCasterId, cast));
  summoner.effects.NoticeSkills(summoner.world, summoner.stage, summoner.planner, kStartMs);
  CHECK_FALSE(Contains(summoner.ChangesOnCaster(), 445, false));
  CHECK_FALSE(summoner.Launched(411));
}

TEST_CASE("Effet d'état : le coup d'état 21 le fait naître aussi", "[app][effets]") {
  Bench bench(kTaekwon);
  ro::sim::Actor target;
  target.id = kCasterId + 1;
  target.kind = ro::sim::ActorKind::kMonster;
  target.position = ro::sim::CellPosition{.x = 2, .y = 1};
  bench.world.Spawn(target);
  ro::sim::Strike strike;
  strike.attacker = kCasterId;
  strike.target = target.id;
  strike.skill_id = kReadyStorm;
  strike.started_ms = kStartMs;
  strike.damage = 1;
  strike.hits = 1;
  REQUIRE(bench.world.RecordStrike(strike));
  bench.effects.NoticeSkills(bench.world, bench.stage, bench.planner, kStartMs);
  CHECK(Contains(bench.ChangesOnCaster(), 449, false));
}
