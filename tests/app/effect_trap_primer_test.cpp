// UN PIÈGE QUI SE DÉCLENCHE JOUE SON AMORCE : le Freezing Trap prend
// l'apparence du piège usé, et c'est à cet instant que sa glace paraît — une
// fois, sur sa case. Un piège qui n'en retient aucune reste muet.

#include <cstdint>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/formats/gnd.h"
#include "ro/formats/rsw.h"
#include "ro/formats/str.h"
#include "ro/render/effect_catalog.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_renderer.h"
#include "ro/render/effect_table.h"
#include "ro/render/lightmap_atlas.h"
#include "ro/render/map_scene.h"
#include "ro/render/scene_light.h"
#include "ro/render/skill_unit_primer.h"
#include "ro/sim/actor.h"
#include "ro/sim/skill_unit_look.h"
#include "ro/sim/walk_path.h"
#include "ro/sim/world.h"
#include "ro/vfs/vfs.h"

#include "effects/effect_node_view.h"
#include "effects/effect_view.h"
#include "support/effect_builder.h"
#include "support/grf_builder.h"
#include "support/map_builder.h"
#include "support/temp_directory.h"
#include "world/map_stage.h"

using ro::test_support::BuildGnd;
using ro::test_support::BuildGrf;
using ro::test_support::BuildRsw;
using ro::test_support::BuildStr;
using ro::test_support::GndBuildOptions;
using ro::test_support::GrfFileSpec;
using ro::test_support::RswBuildOptions;
using ro::test_support::StrBuildOptions;
using ro::test_support::StrKeyFrameSpec;
using ro::test_support::StrLayerSpec;
using ro::test_support::TempDirectory;

namespace {

constexpr std::uint32_t kPlacedMs = 10'000;
constexpr std::uint32_t kSprungMs = 12'000;
constexpr std::uint32_t kTrapId = 7'700;
constexpr std::uint32_t kFreezingTrap = 151;
constexpr std::uint32_t kSkidTrap = 144;
constexpr std::uint32_t kFreezingPrimer = 108;
constexpr std::uint32_t kLandMine = 147;
constexpr std::uint32_t kLandMinePrimer = 97;
constexpr std::uint8_t kLevel = 5;
constexpr ro::sim::CellPosition kCell{.x = 3, .y = 3};
constexpr std::int32_t kGroundCells = 6;
constexpr std::int32_t kStrLastFrame = 20;

moonclient::MapStage FlatStage() {
  GndBuildOptions ground;
  ground.width = kGroundCells;
  ground.height = kGroundCells;
  auto gnd = ro::formats::GndMap::Parse(BuildGnd(ground));
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

std::string PrimerPath(std::uint32_t effect_id = kFreezingPrimer) {
  return std::string(ro::render::kEffectFolder) + ro::render::EffectFile(effect_id, 0);
}

// Un client qui ne porte que le fichier de l'amorce, d'une seule couche.
ro::vfs::Vfs PrimerClient(const TempDirectory& client) {
  StrKeyFrameSpec image;
  image.offset_x = ro::formats::kStrOriginX;
  image.offset_y = ro::formats::kStrOriginY;
  StrLayerSpec layer;
  layer.key_frames = {image};
  StrBuildOptions str;
  str.last_frame = kStrLastFrame;
  str.layers = {layer};
  std::vector<GrfFileSpec> files;
  for (const std::uint32_t effect_id : {kFreezingPrimer, kLandMinePrimer}) {
    files.push_back(GrfFileSpec{.name = PrimerPath(effect_id), .content = BuildStr(str)});
  }
  client.WriteFile("primer.grf", BuildGrf(files));
  auto mounted = ro::vfs::Vfs::Mount({.root = client.path(), .archives = {"primer.grf"}});
  REQUIRE(mounted.has_value());
  return std::move(*mounted);
}

struct Bench {
  Bench() : vfs(PrimerClient(client)) {}

  void Place(std::uint32_t unit_id) {
    world.PlaceSkillUnit(ro::sim::SkillUnit{.id = kTrapId,
                                            .cell = kCell,
                                            .unit_id = unit_id,
                                            .skill_level = kLevel,
                                            .placed_ms = kPlacedMs,
                                            .look_since_ms = kPlacedMs,
                                            .placed_unit_id = unit_id});
    effects.NoticeSkills(world, stage, planner, kPlacedMs);
  }

  // Combien d'images de l'amorce sont à l'écran.
  [[nodiscard]] std::size_t PrimersDrawn(std::uint32_t now_ms,
                                         std::uint32_t effect_id = kFreezingPrimer) {
    const ro::render::LoadedEffect* primer = catalog.Load(PrimerPath(effect_id));
    REQUIRE(primer != nullptr);
    std::size_t count = 0;
    for (const ro::render::DrawnEffect& shown : effects.Drawn(now_ms, stage, world, planner)) {
      count += shown.effect == primer ? 1U : 0U;
    }
    return count;
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

TEST_CASE("Piège : le Freezing Trap joue sa glace en devenant le piège usé",
          "[effect_trap_primer]") {
  Bench bench;
  bench.Place(kFreezingTrap);
  // Posé, il ne montre pas encore son amorce.
  CHECK(bench.PrimersDrawn(kPlacedMs) == 0);

  REQUIRE(ro::sim::ChangeSkillUnitLook(bench.world, kTrapId, ro::render::kUsedTrapUnit, kSprungMs));
  bench.effects.NoticeSkills(bench.world, bench.stage, bench.planner, kSprungMs);
  // Rien ne naissait : l'amorce retenue à la pose n'était lue nulle part.
  CHECK(bench.PrimersDrawn(kSprungMs) == 1);
  // Une fois par changement : le même changement revu ne la relance pas.
  bench.effects.NoticeSkills(bench.world, bench.stage, bench.planner, kSprungMs + 1);
  CHECK(bench.PrimersDrawn(kSprungMs + 1) == 1);

  // Primitive, elle s'en va avec l'unité qui la porte.
  REQUIRE(bench.world.RemoveSkillUnit(kTrapId));
  CHECK(bench.PrimersDrawn(kSprungMs + 2) == 0);
}

TEST_CASE("Piège : le Skid Trap usé reste muet", "[effect_trap_primer]") {
  Bench bench;
  bench.Place(kSkidTrap);
  REQUIRE(ro::sim::ChangeSkillUnitLook(bench.world, kTrapId, ro::render::kUsedTrapUnit, kSprungMs));
  bench.effects.NoticeSkills(bench.world, bench.stage, bench.planner, kSprungMs);
  CHECK(bench.PrimersDrawn(kSprungMs) == 0);
}

TEST_CASE("Piège : la Land Mine usée joue l'explosion que ce client lui ajoute",
          "[effect_trap_primer]") {
  // Le client d'origine ne lui donne aucune amorce : elle restait muette.
  Bench bench;
  bench.Place(kLandMine);
  REQUIRE(ro::sim::ChangeSkillUnitLook(bench.world, kTrapId, ro::render::kUsedTrapUnit, kSprungMs));
  bench.effects.NoticeSkills(bench.world, bench.stage, bench.planner, kSprungMs);
  CHECK(bench.PrimersDrawn(kSprungMs, kLandMinePrimer) == 1);
  // Comme une amorce du client d'origine, le piège l'emporte en disparaissant.
  REQUIRE(bench.world.RemoveSkillUnit(kTrapId));
  CHECK(bench.PrimersDrawn(kSprungMs + 1, kLandMinePrimer) == 0);
}
