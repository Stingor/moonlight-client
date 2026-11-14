// CE QUE LE TIC DE DEUX UNITÉS AJOUTE À LEUR VISUEL : le second effet
// d'Epiclesis, posé une fois, et les bulles que Poison Smoke tire au hasard.

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/formats/gnd.h"
#include "ro/formats/rsw.h"
#include "ro/render/effect_catalog.h"
#include "ro/render/effect_frame.h"
#include "ro/render/lightmap_atlas.h"
#include "ro/render/map_scene.h"
#include "ro/render/scene_light.h"
#include "ro/sim/actor.h"
#include "ro/sim/world.h"
#include "ro/vfs/vfs.h"

#include "effects/effect_node_view.h"
#include "effects/effect_view.h"
#include "support/effect_builder.h"
#include "support/grf_builder.h"
#include "support/map_builder.h"
#include "support/temp_directory.h"
#include "world/map_stage.h"

namespace {

constexpr std::uint32_t kUnitInstanceId = 130'001;
constexpr std::uint32_t kNowMs = 40'000;
constexpr std::uint16_t kEpiclesisUnit = 202;
constexpr std::uint32_t kEpiclesisCompanion = 1654;
constexpr std::uint16_t kPoisonSmokeUnit = 225;
constexpr std::array<std::uint32_t, 2> kPoisonBubbles = {1546, 1547};
// Assez de tics pour qu'une chance sur cinquante tombe, sans la noyer.
constexpr std::int32_t kWatchedTicks = 600;
constexpr std::int32_t kLastFrame = 30;

moonclient::MapStage FlatStage() {
  auto gnd =
      ro::formats::GndMap::Parse(ro::test_support::BuildGnd(ro::test_support::GndBuildOptions{}));
  REQUIRE(gnd.has_value());
  auto rsw =
      ro::formats::RswScene::Parse(ro::test_support::BuildRsw(ro::test_support::RswBuildOptions{}));
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

ro::vfs::Vfs EffectClient(const ro::test_support::TempDirectory& client) {
  std::vector<ro::test_support::GrfFileSpec> files;
  for (const std::string_view file :
       {"mineffect\\new_poisonsmoke\\new_poisonsmoke\\new_poisonsmoke_1.str",
        "mineffect\\new_poisonsmoke\\new_poisonsmoke\\new_poisonsmoke_2.str",
        "mineffect\\new_epiclesis\\new_epiclesis_combine.str"}) {
    files.push_back({"data\\texture\\effect\\" + std::string(file),
                     ro::test_support::BuildStr({.last_frame = kLastFrame})});
  }
  client.WriteFile("effet.grf", ro::test_support::BuildGrf(files));
  auto mounted = ro::vfs::Vfs::Mount({.root = client.path(), .archives = {"effet.grf"}});
  REQUIRE(mounted.has_value());
  return std::move(*mounted);
}

struct Bench {
  ro::test_support::TempDirectory client;
  ro::vfs::Vfs vfs = EffectClient(client);
  ro::render::EffectCatalog catalog{vfs};
  moonclient::EffectNodesInPlay nodes{vfs};
  moonclient::EffectsInPlay effects{catalog, nodes, vfs};
  ro::sim::World world;
  moonclient::MapStage stage = FlatStage();

  explicit Bench(std::uint16_t unit_id) {
    world.PlaceSkillUnit(ro::sim::SkillUnit{.id = kUnitInstanceId,
                                            .cell = {.x = 3, .y = 3},
                                            .unit_id = unit_id,
                                            .placed_ms = kNowMs,
                                            .look_since_ms = kNowMs});
  }
};

}  // namespace

TEST_CASE("Unité au sol : Epiclesis pose son second effet avec le premier", "[app][effets]") {
  Bench bench(kEpiclesisUnit);
  bench.effects.NoticeSkillUnitVisuals(bench.world, bench.stage, kNowMs);
  CHECK(bench.effects.Carries(kEpiclesisCompanion, kUnitInstanceId));
}

TEST_CASE("Unité au sol : Poison Smoke fait éclater des bulles, rarement", "[app][effets]") {
  Bench bench(kPoisonSmokeUnit);
  std::int32_t ticks_with_bubble = 0;
  for (std::int32_t tick = 0; tick < kWatchedTicks; ++tick) {
    const auto at_ms = kNowMs + static_cast<std::uint32_t>(tick) *
                                    static_cast<std::uint32_t>(ro::render::kEffectTickMs);
    bench.effects.NoticeSkillUnitVisuals(bench.world, bench.stage, at_ms);
    for (const std::uint32_t bubble : kPoisonBubbles) {
      if (bench.effects.Carries(bubble, kUnitInstanceId)) {
        ++ticks_with_bubble;
        break;
      }
    }
  }
  // Deux chances sur cent par tic : il en éclate, mais pas à chaque tic.
  CHECK(ticks_with_bubble > 0);
  CHECK(ticks_with_bubble < kWatchedTicks);
}
