// CE QUE L'INCANTATION MONTRE PAR COMPÉTENCE : le cas qui fait naître ses effets
// sur le lanceur, la branche de `/mineffect`, et les tables par défaut.

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/formats/gnd.h"
#include "ro/formats/rsw.h"
#include "ro/render/camera.h"
#include "ro/render/effect_catalog.h"
#include "ro/render/lightmap_atlas.h"
#include "ro/render/map_scene.h"
#include "ro/render/scene_light.h"
#include "ro/sim/actor.h"
#include "ro/sim/walk_path.h"
#include "ro/sim/world.h"
#include "ro/vfs/vfs.h"

#include "effects/effect_node_view.h"
#include "effects/effect_view.h"
#include "support/effect_builder.h"
#include "support/grf_builder.h"
#include "support/image_builder.h"
#include "support/map_builder.h"
#include "support/temp_directory.h"
#include "world/map_stage.h"

namespace {

constexpr std::uint32_t kCasterId = 120'001;
constexpr std::uint32_t kNowMs = 30'000;
constexpr std::uint32_t kCastMs = 1'500;

constexpr std::uint16_t kIgnitionBreak = 2006;
constexpr std::uint32_t kIgnitionBreakCast = 1626;
constexpr std::uint16_t kWallOfThorn = 2482;
constexpr std::uint32_t kThornAlways = 1620;
constexpr std::uint32_t kThornFullOnly = 1621;
constexpr std::uint16_t kNiflheimRequiem = 5363;
constexpr std::uint32_t kRequiemCast = 2076;
// Une compétence que l'aiguillage ne nomme pas, et que la table par défaut sert.
constexpr std::uint16_t kDiamondStorm = 5369;
constexpr std::uint32_t kDiamondStormFirst = 2028;

constexpr std::string_view kFolder = "data\\texture\\effect\\";
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
       {"new_ignitionbreak\\new_ignitionbreak_cast\\new_ignitionbreak_cast.str",
        "mineffect\\new_wallofthorn\\new_wallofthorn_cast\\new_wallofthorn_cast.str",
        "new_wallofthorn\\new_wallofthorn_cast_bottom\\new_wallofthorn_cast_bottom.str",
        "nipelheim\\nipelheim_cast\\nipelheim_cast.str",
        "diamond_storm\\diamond_storm_cast\\diamond_storm_cast.str"}) {
    files.push_back({std::string(kFolder) + std::string(file),
                     ro::test_support::BuildStr({.last_frame = kLastFrame})});
  }
  // L'image du cercle de visée, quatre texels opaques.
  files.push_back(
      {std::string(kFolder) + "magic_target.tga",
       ro::test_support::BuildTga(2, 2,
                                  std::vector<ro::test_support::TgaPixel>(
                                      4, ro::test_support::TgaPixel{255, 255, 255, 255}))});
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
  ro::sim::WalkPlanner planner{moonclient::PlannedMapOf(stage)};

  Bench() {
    ro::sim::Actor caster;
    caster.id = kCasterId;
    caster.kind = ro::sim::ActorKind::kPlayer;
    caster.position = {.x = 10, .y = 10};
    world.Spawn(caster);
  }

  void Cast(std::uint16_t skill_id, ro::sim::CellPosition target_cell = {}) {
    ro::sim::Cast cast;
    cast.skill_id = skill_id;
    cast.target_cell = target_cell;
    cast.started_ms = kNowMs;
    cast.duration_ms = kCastMs;
    REQUIRE(world.RecordCast(kCasterId, cast));
    effects.NoticeSkills(world, stage, planner, kNowMs);
  }
};

}  // namespace

TEST_CASE("Incantation : Ignition Break fait naître son effet sur le lanceur", "[app][effets]") {
  Bench bench;
  bench.Cast(kIgnitionBreak);
  CHECK(bench.effects.Carries(kIgnitionBreakCast, kCasterId));
}

TEST_CASE("Incantation : Wall of Thorn perd son second effet sous /mineffect", "[app][effets]") {
  Bench full;
  full.Cast(kWallOfThorn);
  CHECK(full.effects.Carries(kThornAlways, kCasterId));
  CHECK(full.effects.Carries(kThornFullOnly, kCasterId));

  Bench minimal;
  minimal.effects.SetEffectSwitches(/*shown=*/true, /*minimal=*/true);
  minimal.Cast(kWallOfThorn);
  CHECK(minimal.effects.Carries(kThornAlways, kCasterId));
  CHECK_FALSE(minimal.effects.Carries(kThornFullOnly, kCasterId));
}

TEST_CASE("Incantation : un cas en fin de table se retrouve", "[app][effets]") {
  Bench bench;
  bench.Cast(kNiflheimRequiem);
  CHECK(bench.effects.Carries(kRequiemCast, kCasterId));
}

TEST_CASE("Incantation : une compétence sans cas passe par la table par défaut", "[app][effets]") {
  Bench bench;
  bench.Cast(kDiamondStorm);
  CHECK(bench.effects.Carries(kDiamondStormFirst, kCasterId));
}

TEST_CASE("Incantation au sol : Meteor Storm peint un cercle de onze cases de côté",
          "[app][effets]") {
  constexpr std::uint16_t kMeteorStorm = 83;
  constexpr std::size_t kMeteorScopeCells = 11 * 11;
  Bench bench;
  bench.Cast(kMeteorStorm, ro::sim::CellPosition{.x = 12, .y = 12});
  static_cast<void>(bench.nodes.Drawn(kNowMs, ro::render::Camera{}));
  REQUIRE(bench.nodes.GroundCells().size() == 1);
  CHECK(bench.nodes.GroundCells().front().cells.size() == kMeteorScopeCells);
  // Passée la fin annoncée, il s'en va.
  static_cast<void>(bench.nodes.Drawn(kNowMs + kCastMs + 1, ro::render::Camera{}));
  CHECK(bench.nodes.GroundCells().empty());
}
