// UN NŒUD QUI FAIT NAÎTRE UN AUTRE EFFET : la demande part de la troupe, et
// l'application la satisfait sur l'acteur que la suite de la compétence vise.
//
// Soul Change pose la lueur 503 sur sa cible, qui y fait clignoter 502 ; une
// compétence de Minstrel pose l'émetteur 849 sur sa cible ET sur son lanceur,
// chacun y faisant naître 850.

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/formats/gnd.h"
#include "ro/formats/rsw.h"
#include "ro/render/camera.h"
#include "ro/render/effect_catalog.h"
#include "ro/render/effect_node.h"
#include "ro/render/lightmap_atlas.h"
#include "ro/render/map_scene.h"
#include "ro/render/scene_light.h"
#include "ro/sim/actor.h"
#include "ro/sim/walk_path.h"
#include "ro/sim/world.h"
#include "ro/vfs/vfs.h"

#include "effects/effect_node_view.h"
#include "effects/effect_view.h"
#include "support/grf_builder.h"
#include "support/image_builder.h"
#include "support/map_builder.h"
#include "support/temp_directory.h"
#include "world/map_stage.h"

using ro::test_support::BuildGnd;
using ro::test_support::BuildGrf;
using ro::test_support::BuildRsw;
using ro::test_support::BuildTga;
using ro::test_support::GndBuildOptions;
using ro::test_support::GrfFileSpec;
using ro::test_support::RswBuildOptions;
using ro::test_support::TempDirectory;
using ro::test_support::TgaPixel;

namespace {

constexpr std::uint32_t kCasterId = 150'001;
constexpr std::uint32_t kTargetId = 150'002;
constexpr std::uint32_t kNowMs = 10'000;
// Soul Change : son annonce pose 503 sur sa cible, visée par la suite.
constexpr std::uint16_t kSkillPosing503 = 445;
constexpr std::uint32_t kFollowUpEffect = 502;
// Une compétence de Minstrel : son annonce pose 849 sur sa cible et sur son
// lanceur, chacun visant celui qui le porte.
constexpr std::uint16_t kSkillPosing849 = 2383;
constexpr std::uint32_t kSpiralChild = 850;
constexpr std::int32_t kTexturePixels = 4;

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

// Un client qui ne porte que l'image blanche que la lueur et les trois plans
// demandent.
ro::vfs::Vfs WhiteLightClient(const TempDirectory& client) {
  const std::vector<TgaPixel> white(static_cast<std::size_t>(kTexturePixels * kTexturePixels),
                                    TgaPixel{.red = 255, .green = 255, .blue = 255});
  client.WriteFile("lueur.grf",
                   BuildGrf({GrfFileSpec{"data\\texture\\effect\\whitelight.tga",
                                         BuildTga(kTexturePixels, kTexturePixels, white)}}));
  auto rise = ro::vfs::Vfs::Mount({.root = client.path(), .archives = {"lueur.grf"}});
  REQUIRE(rise.has_value());
  return std::move(*rise);
}

ro::sim::World TwoActors() {
  ro::sim::World world;
  for (const std::uint32_t id : {kCasterId, kTargetId}) {
    ro::sim::Actor actor;
    actor.id = id;
    actor.kind = ro::sim::ActorKind::kPlayer;
    world.Spawn(actor);
  }
  world.set_player_id(kCasterId);
  return world;
}

bool Records(const std::vector<moonclient::EffectsInPlay::ActorEffectToRecord>& poses,
             std::uint32_t actor_id, std::uint32_t effect_id) {
  for (const moonclient::EffectsInPlay::ActorEffectToRecord& pose : poses) {
    if (pose.actor_id == actor_id && pose.effect_id == effect_id) {
      return true;
    }
  }
  return false;
}

}  // namespace

TEST_CASE("la lueur de Soul Change fait clignoter sa cible, un tic après l'autre",
          "[effect_spawn_from_node]") {
  TempDirectory client;
  ro::vfs::Vfs vfs = WhiteLightClient(client);
  ro::render::EffectCatalog catalog(vfs);
  moonclient::EffectNodesInPlay nodes(vfs);
  moonclient::EffectsInPlay effects(catalog, nodes, vfs);
  ro::sim::World world = TwoActors();
  const moonclient::MapStage stage = FlatStage();
  ro::sim::WalkPlanner planner{moonclient::PlannedMapOf(stage)};
  const ro::render::Camera camera;

  effects.PlaySkillOn(world, stage, planner, *world.Find(kTargetId), kSkillPosing503, kCasterId,
                      kNowMs, 1, moonclient::SkillMoment::kAnnounced, 0);
  // Ce que la suite pose elle-même sur l'acteur n'est pas l'objet de l'essai.
  static_cast<void>(effects.TakeActorEffects());

  // LE PREMIER PASSAGE NE BAT AUCUN TIC : rien n'est demandé.
  static_cast<void>(nodes.Drawn(kNowMs, camera));
  effects.SowRequestedEffects(world, stage, planner, kNowMs);
  CHECK(effects.TakeActorEffects().empty());

  // UN TIC : la lueur se peint et demande 502 sur la CIBLE, que la suite vise.
  const auto tick_ms = static_cast<std::uint32_t>(ro::render::kEffectTickMs);
  static_cast<void>(nodes.Drawn(kNowMs + tick_ms, camera));
  effects.SowRequestedEffects(world, stage, planner, kNowMs + tick_ms);
  const std::vector<moonclient::EffectsInPlay::ActorEffectToRecord> poses =
      effects.TakeActorEffects();
  CHECK(Records(poses, kTargetId, kFollowUpEffect));
  CHECK_FALSE(Records(poses, kCasterId, kFollowUpEffect));

  // ⚠ UNE IMAGE PLUS COURTE QU'UN TIC NE REDEMANDE RIEN : la cadence est celle
  // des tics, pas celle des images.
  static_cast<void>(nodes.Drawn(kNowMs + tick_ms + 1U, camera));
  effects.SowRequestedEffects(world, stage, planner, kNowMs + tick_ms + 1U);
  CHECK(effects.TakeActorEffects().empty());
}

TEST_CASE("l'émetteur en spirale fait naître ses lueurs sur l'acteur que chaque étape vise",
          "[effect_spawn_from_node]") {
  TempDirectory client;
  ro::vfs::Vfs vfs = WhiteLightClient(client);
  ro::render::EffectCatalog catalog(vfs);
  moonclient::EffectNodesInPlay nodes(vfs);
  moonclient::EffectsInPlay effects(catalog, nodes, vfs);
  ro::sim::World world = TwoActors();
  const moonclient::MapStage stage = FlatStage();
  ro::sim::WalkPlanner planner{moonclient::PlannedMapOf(stage)};
  const ro::render::Camera camera;

  effects.PlaySkillOn(world, stage, planner, *world.Find(kTargetId), kSkillPosing849, kCasterId,
                      kNowMs, 1, moonclient::SkillMoment::kAnnounced, 0);
  CHECK_FALSE(nodes.CarriesEffect(kSpiralChild, kTargetId));

  const auto tick_ms = static_cast<std::uint32_t>(ro::render::kEffectTickMs);
  static_cast<void>(nodes.Drawn(kNowMs, camera));
  static_cast<void>(nodes.Drawn(kNowMs + tick_ms, camera));
  effects.SowRequestedEffects(world, stage, planner, kNowMs + tick_ms);

  // Deux étapes, deux émetteurs : l'un vise la cible, l'autre le lanceur, et
  // chacun y accroche sa lueur.
  CHECK(nodes.CarriesEffect(kSpiralChild, kTargetId));
  CHECK(nodes.CarriesEffect(kSpiralChild, kCasterId));
}
