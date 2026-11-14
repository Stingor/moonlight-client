// LES SPHÈRES SPIRITUELLES DU MONK VUES PAR L'APPLICATION : le nombre que le
// paquet des sphères annonce devient une gerbe de carrés autour du porteur,
// qui s'allume tic après tic.

#include <cstddef>
#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/formats/gnd.h"
#include "ro/formats/rsw.h"
#include "ro/render/camera.h"
#include "ro/render/effect_catalog.h"
#include "ro/render/effect_frame.h"
#include "ro/render/effect_node_pool.h"
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

using ro::test_support::BmpPixel;
using ro::test_support::BuildBmp;
using ro::test_support::BuildGnd;
using ro::test_support::BuildGrf;
using ro::test_support::BuildRsw;
using ro::test_support::GndBuildOptions;
using ro::test_support::GrfFileSpec;
using ro::test_support::RswBuildOptions;
using ro::test_support::TempDirectory;

namespace {

constexpr std::uint32_t kPlayerId = 150'001;
constexpr std::uint16_t kMonkJob = 15;
constexpr std::int16_t kSpheres = 3;
constexpr std::uint32_t kNowMs = 10'000;
constexpr std::int32_t kPixels = 4;
// Assez de tics pour que l'alpha, qui monte d'un cran par tic, soit franc.
constexpr std::int32_t kTicksToGlow = 60;

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

std::size_t CountSpheres(const moonclient::EffectNodesInPlay& nodes) {
  std::size_t count = 0;
  for (const ro::render::PooledEffectNode& node : nodes.living_nodes()) {
    count += node.shape == ro::render::PooledNodeShape::kConcentricSquare ? 1U : 0U;
  }
  return count;
}

struct Bench {
  TempDirectory client;
  ro::vfs::Vfs vfs;
  ro::render::EffectCatalog catalog;
  moonclient::EffectNodesInPlay nodes;
  moonclient::EffectsInPlay effects;
  ro::sim::World world;
  moonclient::MapStage stage = FlatStage();
  ro::sim::WalkPlanner planner{moonclient::PlannedMapOf(stage)};

  Bench() : vfs(Mount(client)), catalog(vfs), nodes(vfs), effects(catalog, nodes, vfs) {
    world.set_player_id(kPlayerId);
    ro::sim::Actor monk;
    monk.id = kPlayerId;
    monk.kind = ro::sim::ActorKind::kPlayer;
    monk.look.job = kMonkJob;
    monk.position = {.x = 3, .y = 4};
    world.Spawn(monk);
  }

  static ro::vfs::Vfs Mount(const TempDirectory& client) {
    const std::vector<BmpPixel> pixels(static_cast<std::size_t>(kPixels * kPixels),
                                       BmpPixel{.red = 200, .green = 200, .blue = 255});
    client.WriteFile("spheres.grf",
                     BuildGrf({GrfFileSpec{"data\\texture\\effect\\thunder_center.bmp",
                                           BuildBmp(kPixels, kPixels, pixels)}}));
    auto mounted = ro::vfs::Vfs::Mount({.root = client.path(), .archives = {"spheres.grf"}});
    REQUIRE(mounted.has_value());
    return std::move(*mounted);
  }
};

}  // namespace

TEST_CASE("trois sphères annoncées font trois carrés qui s'allument autour du Monk",
          "[effect_spirit_spheres_app]") {
  Bench bench;
  REQUIRE(bench.world.RecordSpiritSpheres(kPlayerId, kSpheres, kNowMs));
  bench.effects.NoticeEffectRemovals(bench.world);
  bench.effects.Notice(bench.world, bench.stage, bench.planner);
  bench.effects.NoticeSkills(bench.world, bench.stage, bench.planner, kNowMs);
  bench.effects.NoticeOptionEffects(bench.world, bench.stage, bench.planner, kNowMs);
  bench.effects.NoticeStateChanges(bench.world, bench.stage, bench.planner, kNowMs);
  bench.effects.NoticeStateEffects(bench.world, bench.stage, bench.planner, kNowMs);
  bench.effects.NoticeHatEffects(bench.world, bench.stage, bench.planner, kNowMs);
  CHECK(CountSpheres(bench.nodes) == static_cast<std::size_t>(kSpheres));

  const ro::render::Camera camera;
  const std::uint32_t later_ms =
      kNowMs + static_cast<std::uint32_t>(kTicksToGlow * ro::render::kEffectTickMs);
  static_cast<void>(bench.nodes.Drawn(kNowMs, camera));
  static_cast<void>(bench.nodes.Drawn(later_ms, camera));
  CHECK(CountSpheres(bench.nodes) == static_cast<std::size_t>(kSpheres));
  std::size_t quads = 0;
  for (const ro::render::DrawnWorldQuads& lot : bench.nodes.WorldQuads()) {
    quads += lot.quads.size();
  }
  // Deux couches par carré quand la gerbe est petite : la pleine et l'étroite.
  CHECK(quads == static_cast<std::size_t>(kSpheres) * 2U);
}
