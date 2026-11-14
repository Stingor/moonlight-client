// UN EFFET QU'UNE COMPÉTENCE POSE ET QUI CHANGE L'ACTEUR se pose sur son
// porteur au lieu de se dessiner : la suite le met de côté, et le jeu
// l'inscrit sur l'acteur comme le serveur le fait pour ceux qu'il nomme.

#include <cstdint>
#include <span>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/formats/gnd.h"
#include "ro/formats/rsw.h"
#include "ro/render/effect_catalog.h"
#include "ro/render/effect_sound_table.h"
#include "ro/render/lightmap_atlas.h"
#include "ro/render/map_scene.h"
#include "ro/render/scene_light.h"
#include "ro/sim/actor.h"
#include "ro/sim/walk_path.h"
#include "ro/sim/world.h"
#include "ro/vfs/vfs.h"

#include "effects/effect_layers.h"
#include "effects/effect_node_view.h"
#include "effects/effect_view.h"
#include "support/client_vfs.h"
#include "support/map_builder.h"
#include "support/temp_directory.h"
#include "world/map_stage.h"

using ro::test_support::BuildGnd;
using ro::test_support::BuildRsw;
using ro::test_support::EmptyClient;
using ro::test_support::GndBuildOptions;
using ro::test_support::RswBuildOptions;
using ro::test_support::TempDirectory;

namespace {

constexpr std::uint32_t kCasterId = 150'001;
constexpr std::uint32_t kTargetId = 150'002;
constexpr std::uint32_t kNowMs = 10'000;
// Une des compétences dont l'annonce pose 424 sur sa cible : un drapeau de
// rendu de l'acteur, sans fichier ni nœud.
constexpr std::uint16_t kSkillPosing424 = 445;
constexpr std::uint32_t kRenderFlagEffect = 424;
// CR_SHRINK : son annonce pose 599 sur le lanceur, une teinte qui clignote,
// sans fichier ni nœud, et dont le cas joue `EF_BeginSpell.wav` au cinquième tic.
constexpr std::uint16_t kShrinkSkill = 1002;
constexpr std::uint32_t kShrinkTintEffect = 599;
constexpr std::uint16_t kShrinkSoundTick = 5;

moonclient::MapStage FlatStage() {
  auto gnd = ro::formats::GndMap::Parse(BuildGnd(GndBuildOptions{}));
  REQUIRE(gnd.has_value());
  auto rsw = ro::formats::RswScene::Parse(BuildRsw(RswBuildOptions{}));
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

}  // namespace

TEST_CASE("un effet d'acteur posé par une compétence va sur sa cible", "[effect_actor_change]") {
  TempDirectory client;
  ro::vfs::Vfs vfs = EmptyClient(client);
  ro::render::EffectCatalog catalog(vfs);
  moonclient::EffectNodesInPlay nodes(vfs);
  moonclient::EffectsInPlay effects(catalog, nodes, vfs);
  ro::sim::World world;
  for (const std::uint32_t id : {kCasterId, kTargetId}) {
    ro::sim::Actor actor;
    actor.id = id;
    actor.kind = ro::sim::ActorKind::kPlayer;
    world.Spawn(actor);
  }
  const moonclient::MapStage stage = FlatStage();
  ro::sim::WalkPlanner planner{moonclient::PlannedMapOf(stage)};

  effects.PlaySkillOn(world, stage, planner, *world.Find(kTargetId), kSkillPosing424, kCasterId,
                      kNowMs, 1, moonclient::SkillMoment::kAnnounced, 0);

  const std::vector<moonclient::EffectsInPlay::ActorEffectToRecord> poses =
      effects.TakeActorEffects();
  bool vu = false;
  for (const moonclient::EffectsInPlay::ActorEffectToRecord& pose : poses) {
    if (pose.effect_id == kRenderFlagEffect) {
      vu = true;
      CHECK(pose.actor_id == kTargetId);
      CHECK(pose.started_ms == kNowMs);
    }
  }
  CHECK(vu);
  // Emportée, la file est vide.
  CHECK(effects.TakeActorEffects().empty());
}

TEST_CASE("un effet d'acteur posé par une compétence fait entendre son bruit",
          "[effect_actor_change]") {
  // Le bruit est bien celui du cas : sans lui, l'essai ne dirait rien.
  const std::span<const ro::render::EffectSoundEntry> sounds =
      ro::render::EffectSounds(kShrinkTintEffect);
  REQUIRE(sounds.size() == 1);
  CHECK(sounds.front().sound == "effect\\EF_BeginSpell.wav");
  CHECK(sounds.front().delay_ticks == kShrinkSoundTick);

  TempDirectory client;
  ro::vfs::Vfs vfs = EmptyClient(client);
  ro::render::EffectCatalog catalog(vfs);
  moonclient::EffectNodesInPlay nodes(vfs);
  moonclient::EffectsInPlay effects(catalog, nodes, vfs);
  ro::sim::World world;
  ro::sim::Actor crusader;
  crusader.id = kCasterId;
  crusader.kind = ro::sim::ActorKind::kPlayer;
  world.Spawn(crusader);
  const moonclient::MapStage stage = FlatStage();
  ro::sim::WalkPlanner planner{moonclient::PlannedMapOf(stage)};

  // Shrink se lance sur soi : le lanceur est aussi la cible annoncée.
  effects.PlaySkillOn(world, stage, planner, *world.Find(kCasterId), kShrinkSkill, kCasterId,
                      kNowMs, 1, moonclient::SkillMoment::kAnnounced, 0);

  const std::vector<moonclient::EffectsInPlay::ActorEffectToRecord> poses =
      effects.TakeActorEffects();
  REQUIRE(poses.size() == 1);
  CHECK(poses.front().effect_id == kShrinkTintEffect);
  CHECK(poses.front().actor_id == kCasterId);
  const std::vector<moonclient::LaunchedEffect> launched = effects.TakeLaunchedEffects();
  REQUIRE(launched.size() == 1);
  CHECK(launched.front().effect_id == kShrinkTintEffect);
  CHECK(launched.front().bearer_id == kCasterId);
}

namespace {

// Un croisé seul sur une carte plate, et les effets qui jouent autour de lui.
struct ShrinkBench {
  ShrinkBench() : vfs(EmptyClient(client)) {
    ro::sim::Actor crusader;
    crusader.id = kCasterId;
    crusader.kind = ro::sim::ActorKind::kPlayer;
    world.Spawn(crusader);
  }

  TempDirectory client;
  ro::vfs::Vfs vfs;
  ro::render::EffectCatalog catalog{vfs};
  moonclient::EffectNodesInPlay nodes{vfs};
  moonclient::EffectsInPlay effects{catalog, nodes, vfs};
  ro::sim::World world;
  moonclient::MapStage stage = FlatStage();
  ro::sim::WalkPlanner planner{moonclient::PlannedMapOf(stage)};
};

}  // namespace

TEST_CASE("un effet d'acteur que le serveur annonce fait entendre son bruit",
          "[effect_actor_change]") {
  ShrinkBench bench;
  REQUIRE(bench.world.RecordEffect(kCasterId, kShrinkTintEffect, kNowMs));

  bench.effects.Notice(bench.world, bench.stage, bench.planner);
  const std::vector<moonclient::LaunchedEffect> launched = bench.effects.TakeLaunchedEffects();
  REQUIRE(launched.size() == 1);
  CHECK(launched.front().effect_id == kShrinkTintEffect);
  CHECK(launched.front().bearer_id == kCasterId);
  // La même instance, revue à l'image suivante, ne sonne plus.
  bench.effects.Notice(bench.world, bench.stage, bench.planner);
  CHECK(bench.effects.TakeLaunchedEffects().empty());
}

TEST_CASE("un effet d'acteur que nous posons ne sonne qu'une fois", "[effect_actor_change]") {
  // La suite le fait entendre en naissant ; inscrit sur l'acteur, il repasse
  // par `Notice`, qui ne doit pas le faire entendre une seconde fois.
  ShrinkBench bench;
  bench.effects.PlaySkillOn(bench.world, bench.stage, bench.planner, *bench.world.Find(kCasterId),
                            kShrinkSkill, kCasterId, kNowMs, 1, moonclient::SkillMoment::kAnnounced,
                            0);
  CHECK(bench.effects.TakeLaunchedEffects().size() == 1);
  moonclient::SettleActorEffects(bench.world, bench.effects.TakeActorEffects());

  bench.effects.Notice(bench.world, bench.stage, bench.planner);
  CHECK(bench.effects.TakeLaunchedEffects().empty());
}

TEST_CASE("les voiles du Gunslinger se posent sur le bon porteur", "[effect_actor_change]") {
  // GS_MADNESSCANCEL (504) voile son lanceur de bleu à l'annonce ; GS_FLING
  // (501) rougit sa cible au coup.
  constexpr std::uint16_t kMadnessCancel = 504;
  constexpr std::uint32_t kBlueVeilEffect = 625;
  constexpr std::uint16_t kFling = 501;
  constexpr std::uint32_t kRedFlushEffect = 548;
  TempDirectory client;
  ro::vfs::Vfs vfs = EmptyClient(client);
  ro::render::EffectCatalog catalog(vfs);
  moonclient::EffectNodesInPlay nodes(vfs);
  moonclient::EffectsInPlay effects(catalog, nodes, vfs);
  ro::sim::World world;
  for (const std::uint32_t id : {kCasterId, kTargetId}) {
    ro::sim::Actor actor;
    actor.id = id;
    actor.kind = ro::sim::ActorKind::kPlayer;
    world.Spawn(actor);
  }
  const moonclient::MapStage stage = FlatStage();
  ro::sim::WalkPlanner planner{moonclient::PlannedMapOf(stage)};

  effects.PlaySkillOn(world, stage, planner, *world.Find(kCasterId), kMadnessCancel, kCasterId,
                      kNowMs, 1, moonclient::SkillMoment::kAnnounced, 0);
  std::vector<moonclient::EffectsInPlay::ActorEffectToRecord> poses = effects.TakeActorEffects();
  REQUIRE(poses.size() == 1);
  CHECK(poses.front().effect_id == kBlueVeilEffect);
  CHECK(poses.front().actor_id == kCasterId);

  effects.PlaySkillOn(world, stage, planner, *world.Find(kTargetId), kFling, kCasterId, kNowMs, 1,
                      moonclient::SkillMoment::kImpact, 0);
  poses = effects.TakeActorEffects();
  REQUIRE(poses.size() == 1);
  CHECK(poses.front().effect_id == kRedFlushEffect);
  CHECK(poses.front().actor_id == kTargetId);
}
