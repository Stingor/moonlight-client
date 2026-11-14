// LA LANCE DE BRANDISH SPEAR, l'effet 144, ne naît qu'UNE fois par emploi.
//
// Le serveur annonce une incantation de sept cents millisecondes, puis envoie un
// message de dégâts par victime : la cible principale en sorte 8, les autres en
// éclaboussure, sorte 5. Le client refuse l'effet d'incantation de cette
// compétence, écarte les éclaboussures avant l'effet d'emploi, et ne pose donc
// la lance qu'au coup de la cible principale, sur le lanceur.

#include <cstdint>
#include <string>
#include <utility>

#include <catch2/catch_test_macros.hpp>

#include "ro/formats/gnd.h"
#include "ro/formats/rsw.h"
#include "ro/formats/str.h"
#include "ro/render/effect_catalog.h"
#include "ro/render/lightmap_atlas.h"
#include "ro/render/map_scene.h"
#include "ro/render/scene_light.h"
#include "ro/render/skill_hit_effect.h"
#include "ro/render/skill_impl.h"
#include "ro/render/skill_use_motion.h"
#include "ro/sim/actor.h"
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
using ro::test_support::RswBuildOptions;
using ro::test_support::StrBuildOptions;
using ro::test_support::StrKeyFrameSpec;
using ro::test_support::StrLayerSpec;
using ro::test_support::TempDirectory;

namespace {

constexpr std::uint16_t kBrandishSpear = 57;
constexpr std::uint16_t kBowlingBash = 62;
constexpr std::uint16_t kColdBolt = 14;
constexpr std::int16_t kLance = 144;
constexpr const char* kLanceFile = "data\\texture\\effect\\Brandish2.str";
constexpr std::uint32_t kCasterId = 150'021;
constexpr std::uint32_t kTargetId = 150'022;
constexpr std::uint32_t kNowMs = 10'000;
// L'incantation que rAthena pré-renouveau annonce : la fiche de la compétence.
constexpr std::uint32_t kCastMs = 700;
constexpr std::int32_t kStrLastFrame = 30;
constexpr std::int32_t kGroundCells = 4;
constexpr ro::sim::CellPosition kCasterCell{.x = 1, .y = 1};
constexpr ro::sim::CellPosition kTargetCell{.x = 2, .y = 1};
// Les sortes que le serveur donne aux messages de dégâts : la cible principale
// reçoit la sorte ordinaire d'une compétence, les autres l'éclaboussure.
constexpr std::uint8_t kMainTargetDamageType = 8;
constexpr std::uint8_t kSplashDamageType = 5;
constexpr int kSplashVictims = 3;

moonclient::MapStage FlatStage() {
  GndBuildOptions sol;
  sol.width = kGroundCells;
  sol.height = kGroundCells;
  auto gnd = ro::formats::GndMap::Parse(BuildGnd(sol));
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

// Un client qui ne porte que le fichier de la lance : s'il naît, il joue.
ro::vfs::Vfs ClientWithLance(const TempDirectory& client) {
  StrKeyFrameSpec image;
  image.offset_x = ro::formats::kStrOriginX;
  image.offset_y = ro::formats::kStrOriginY;
  StrLayerSpec layer;
  layer.key_frames = {image};
  StrBuildOptions file;
  file.last_frame = kStrLastFrame;
  file.layers = {layer};
  client.WriteFile("effet.grf", BuildGrf({{.name = kLanceFile, .content = BuildStr(file)}}));
  auto mounted = ro::vfs::Vfs::Mount({.root = client.path(), .archives = {"effet.grf"}});
  REQUIRE(mounted.has_value());
  return std::move(*mounted);
}

struct Bench {
  Bench() : vfs(ClientWithLance(client)) {
    ro::sim::Actor caster;
    caster.id = kCasterId;
    caster.kind = ro::sim::ActorKind::kPlayer;
    caster.position = kCasterCell;
    caster.cast = ro::sim::Cast{.skill_id = kBrandishSpear,
                                .target_id = kTargetId,
                                .started_ms = kNowMs,
                                .duration_ms = kCastMs};
    world.Spawn(caster);
    ro::sim::Actor target;
    target.id = kTargetId;
    target.kind = ro::sim::ActorKind::kMonster;
    target.position = kTargetCell;
    world.Spawn(target);
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

// Combien de lances un message de dégâts de cette sorte pose sur le lanceur,
// par la même règle que le chemin des impacts.
[[nodiscard]] int LancesOfHit(std::uint8_t damage_type) {
  constexpr std::int16_t kLevelFive = 5;
  return ro::render::SkillHitUseEffect(kBrandishSpear, damage_type, kLevelFive,
                                       /*target_known=*/true) == kLance
             ? 1
             : 0;
}

}  // namespace

TEST_CASE("Brandish Spear : l'incantation ne pose pas la lance", "[app][effets][brandish]") {
  Bench bench;
  bench.effects.NoticeSkills(bench.world, bench.stage, bench.planner, kNowMs);
  CHECK(bench.effects.playing() == 0);
  // Pas davantage pendant que l'incantation court.
  bench.effects.NoticeSkills(bench.world, bench.stage, bench.planner, kNowMs + kCastMs / 2);
  CHECK(bench.effects.playing() == 0);
}

TEST_CASE("Brandish Spear et Bowling Bash refusent l'effet d'incantation, pas les autres",
          "[app][effets][brandish]") {
  CHECK_FALSE(ro::render::SkillImplFor(kBrandishSpear).CastSpawnsEffect());
  CHECK_FALSE(ro::render::SkillImplFor(kBowlingBash).CastSpawnsEffect());
  CHECK(ro::render::SkillImplFor(kColdBolt).CastSpawnsEffect());
}

TEST_CASE("Brandish Spear : une lance pour un emploi à quatre victimes",
          "[app][effets][brandish]") {
  // L'incantation, puis un message par victime.
  int lances = ro::render::SkillImplFor(kBrandishSpear).CastSpawnsEffect() ? 1 : 0;
  lances += LancesOfHit(kMainTargetDamageType);
  for (int splash = 0; splash < kSplashVictims; ++splash) {
    lances += LancesOfHit(kSplashDamageType);
  }
  CHECK(lances == 1);
}
