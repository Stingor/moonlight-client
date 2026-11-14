// LE TREMBLEMENT QUE L'ÂGE D'UN EFFET DÉCLENCHE, ET LE CHAMP QUI DURE AUTANT QUE
// SA ZONE.
//
// Meteor Storm fait trembler l'écran quand sa boule touche le sol, au
// quarantième tic de l'effet qu'il pose sur la case — quel que soit le chemin
// qui l'a fait naître. Et le champ de Gravitation, que le client d'origine
// laisse s'éteindre avant la fin de la zone, dure ici autant qu'elle. Une unité
// que le serveur renvoie à une autre case emporte ce qu'elle porte, selon la
// table de suivi de chaque effet.

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/formats/gnd.h"
#include "ro/formats/rsw.h"
#include "ro/formats/str.h"
#include "ro/render/camera.h"
#include "ro/render/effect_catalog.h"
#include "ro/render/effect_frame.h"
#include "ro/render/effect_node_pattern_table.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_table.h"
#include "ro/render/effect_tinted_quad_seed.h"
#include "ro/render/lightmap_atlas.h"
#include "ro/render/map_scene.h"
#include "ro/render/scene_light.h"
#include "ro/render/skill_unit_table.h"
#include "ro/render/str_effect_setup.h"
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

using ro::test_support::BmpPixel;
using ro::test_support::BuildBmp;
using ro::test_support::BuildGnd;
using ro::test_support::BuildGrf;
using ro::test_support::BuildRsw;
using ro::test_support::BuildStr;
using ro::test_support::BuildTga;
using ro::test_support::GndBuildOptions;
using ro::test_support::GrfFileSpec;
using ro::test_support::RswBuildOptions;
using ro::test_support::StrBuildOptions;
using ro::test_support::StrKeyFrameSpec;
using ro::test_support::StrLayerSpec;
using ro::test_support::TempDirectory;
using ro::test_support::TgaPixel;

namespace {

constexpr std::uint32_t kCasterId = 150'031;
constexpr std::uint32_t kNowMs = 10'000;
constexpr std::uint16_t kSkillLevel = 10;
constexpr ro::sim::CellPosition kCasterCell{.x = 1, .y = 1};
constexpr ro::sim::CellPosition kAimedCell{.x = 3, .y = 3};
constexpr std::int32_t kGroundCells = 6;
constexpr std::int32_t kTexturePixels = 4;
constexpr std::int32_t kStrLastFrame = 100;
// Sous le plafond de rattrapage de la troupe.
constexpr std::int32_t kSliceTicks = 30;

// Meteor Storm, en effets pleins : 92 sur la case, dont la boule touche au
// quarantième tic.
constexpr std::uint16_t kMeteorStormSkill = 83;
constexpr std::uint32_t kMeteorEffect = 92;
constexpr std::uint32_t kMeteorFileVariants = 3;
constexpr std::uint32_t kMeteorImpactMs = 40 * ro::render::kEffectTickMs;

// Le champ de Gravitation : l'unité 184 entretient 522.
constexpr std::uint32_t kGravitationUnitId = 8'400;
constexpr std::uint32_t kGravitationFieldEffect = 522;

// La chanson Lullaby : l'unité 158 entretient 278, qui suit son porteur à
// chaque tic.
constexpr std::uint32_t kLullabyUnitId = 8'500;
constexpr std::uint16_t kLullabyUnit = 158;
constexpr std::uint32_t kLullabyEffect = 278;

// Earth Strain : son unité fait trembler l'écran tant qu'elle vit.
constexpr std::uint32_t kEarthStrainUnitId = 8'600;
constexpr std::uint16_t kEarthStrainUnit = 203;
constexpr ro::sim::CellPosition kStepCell{.x = 4, .y = 3};

std::uint32_t TickToMs(std::int32_t ticks) {
  return static_cast<std::uint32_t>(ticks * ro::render::kEffectTickMs);
}

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

// Un client qui porte les trois fichiers de Meteor Storm, d'une couche, et une
// image blanche pour chaque texture des nœuds du champ de Gravitation.
ro::vfs::Vfs MeteorAndGravitationClient(const TempDirectory& client) {
  const std::vector<TgaPixel> white(static_cast<std::size_t>(kTexturePixels * kTexturePixels),
                                    TgaPixel{.red = 255, .green = 255, .blue = 255});
  const std::vector<BmpPixel> white_bmp(static_cast<std::size_t>(kTexturePixels * kTexturePixels),
                                        BmpPixel{.red = 255, .green = 255, .blue = 255});
  std::vector<GrfFileSpec> files;
  for (const ro::render::EffectNodePattern& pattern :
       ro::render::EffectNodePatternsFor(kGravitationFieldEffect)) {
    for (const std::string_view path : ro::render::EffectNodePatternTextures(pattern)) {
      const bool bmp = path.ends_with(".bmp");
      files.push_back(
          GrfFileSpec{.name = std::string(path),
                      .content = bmp ? BuildBmp(kTexturePixels, kTexturePixels, white_bmp)
                                     : BuildTga(kTexturePixels, kTexturePixels, white)});
    }
  }
  for (const ro::render::EffectTintedQuadNodePattern& pattern :
       ro::render::EffectTintedQuadNodePatternsFor(kLullabyEffect)) {
    files.push_back(GrfFileSpec{.name = std::string(pattern.texture),
                                .content = BuildBmp(kTexturePixels, kTexturePixels, white_bmp)});
  }
  StrKeyFrameSpec image;
  image.offset_x = ro::formats::kStrOriginX;
  image.offset_y = ro::formats::kStrOriginY;
  StrLayerSpec layer;
  layer.key_frames = {image};
  StrBuildOptions str;
  str.last_frame = kStrLastFrame;
  str.layers = {layer};
  for (std::uint32_t variant = 0; variant < kMeteorFileVariants; ++variant) {
    files.push_back(GrfFileSpec{.name = std::string(ro::render::kEffectFolder) +
                                        ro::render::EffectFile(kMeteorEffect, variant),
                                .content = BuildStr(str)});
  }
  client.WriteFile("white.grf", BuildGrf(files));
  auto mounted = ro::vfs::Vfs::Mount({.root = client.path(), .archives = {"white.grf"}});
  REQUIRE(mounted.has_value());
  return std::move(*mounted);
}

struct Bench {
  Bench() : vfs(MeteorAndGravitationClient(client)) {
    ro::sim::Actor caster;
    caster.id = kCasterId;
    caster.kind = ro::sim::ActorKind::kPlayer;
    caster.position = kCasterCell;
    world.Spawn(caster);
  }

  // Avance la troupe jusqu'à `tick` tics après l'instant de départ, par tranches.
  void AdvanceTo(std::int32_t tick) {
    while (elapsed_ticks < tick) {
      elapsed_ticks = std::min(tick, elapsed_ticks + kSliceTicks);
      static_cast<void>(nodes.Drawn(kNowMs + TickToMs(elapsed_ticks), camera));
    }
  }

  [[nodiscard]] std::size_t LivingFrom(std::uint32_t effect_id) const {
    return static_cast<std::size_t>(std::ranges::count(
        nodes.living_nodes(), effect_id, &ro::render::PooledEffectNode::sown_by_effect_id));
  }

  // Le tremblement demandé à cet instant, après le passage des effets en fichier.
  [[nodiscard]] std::optional<std::uint32_t> QuakeAt(std::uint32_t now_ms) {
    static_cast<void>(effects.Drawn(now_ms, stage, world, planner));
    return effects.TakeScreenQuake();
  }

  TempDirectory client;
  ro::vfs::Vfs vfs;
  ro::render::EffectCatalog catalog{vfs};
  moonclient::EffectNodesInPlay nodes{vfs};
  moonclient::EffectsInPlay effects{catalog, nodes, vfs};
  ro::sim::World world;
  moonclient::MapStage stage = FlatStage();
  ro::sim::WalkPlanner planner{moonclient::PlannedMapOf(stage)};
  const ro::render::Camera camera;
  std::int32_t elapsed_ticks = 0;
};

}  // namespace

TEST_CASE("Meteor Storm : l'écran tremble quand la boule touche le sol", "[effect_update_quake]") {
  Bench bench;
  REQUIRE(bench.world.RecordGroundSkill(kCasterId,
                                        ro::sim::GroundSkillUse{.skill_id = kMeteorStormSkill,
                                                                .level = kSkillLevel,
                                                                .cell = kAimedCell,
                                                                .started_ms = kNowMs}));
  bench.effects.NoticeSkills(bench.world, bench.stage, bench.planner, kNowMs);
  // La boule n'a pas encore touché : rien ne tremble.
  CHECK_FALSE(bench.QuakeAt(kNowMs).has_value());
  CHECK_FALSE(bench.QuakeAt(kNowMs + kMeteorImpactMs - 1).has_value());
  // 🔴 AU QUARANTIÈME TIC, six cent cinquante millisecondes. Rien ne tremblait
  // plus depuis que la case pose 92, et non la variante allégée, en effets pleins.
  const std::optional<std::uint32_t> quake = bench.QuakeAt(kNowMs + kMeteorImpactMs);
  REQUIRE(quake.has_value());
  CHECK(*quake == kNowMs + kMeteorImpactMs + ro::render::kEffectQueuedQuakeMs);
  // Une seule fois.
  CHECK_FALSE(
      bench.QuakeAt(kNowMs + kMeteorImpactMs + ro::render::kEffectQueuedQuakeMs).has_value());
}

TEST_CASE("Gravitation : le champ dure autant que la zone, et s'en va avec elle",
          "[effect_update_quake]") {
  Bench bench;
  bench.world.PlaceSkillUnit(ro::sim::SkillUnit{.id = kGravitationUnitId,
                                                .cell = kAimedCell,
                                                .unit_id = ro::render::kGravitationFieldUnit,
                                                .skill_level = 5,
                                                .placed_ms = kNowMs,
                                                .look_since_ms = kNowMs});
  bench.effects.NoticeSkillUnitVisuals(bench.world, bench.stage, kNowMs);
  static_cast<void>(bench.nodes.Drawn(kNowMs, bench.camera));
  REQUIRE(bench.LivingFrom(kGravitationFieldEffect) > 0);

  // La troupe vit deux cent cinquante tics ; la zone en tient bien plus.
  const std::uint32_t period_ms = ro::render::SustainedUnitVisualPeriodMs(kGravitationFieldEffect);
  REQUIRE(period_ms > 0);
  const auto relaunch_tick =
      static_cast<std::int32_t>(period_ms / static_cast<std::uint32_t>(ro::render::kEffectTickMs));
  const std::int32_t life_ticks =
      moonclient::EffectNodesInPlay::ProceduralLifeTicks(kGravitationFieldEffect);
  REQUIRE(relaunch_tick < life_ticks);
  bench.AdvanceTo(relaunch_tick);
  bench.effects.NoticeSkillUnitVisuals(bench.world, bench.stage, kNowMs + TickToMs(relaunch_tick));
  // 🔴 PASSÉ LA VIE DE LA PREMIÈRE TROUPE, LE CHAMP EST TOUJOURS LÀ : il s'éteignait
  // au deux-cent-cinquantième tic, zone ou pas.
  bench.AdvanceTo(life_ticks + 1);
  CHECK(bench.LivingFrom(kGravitationFieldEffect) > 0);

  // La zone levée par le serveur l'emporte.
  REQUIRE(bench.world.RemoveSkillUnit(kGravitationUnitId));
  const std::uint32_t lifted_ms = kNowMs + TickToMs(life_ticks + 1);
  bench.effects.NoticeSkillUnitVisuals(bench.world, bench.stage, lifted_ms);
  bench.effects.NoticeOptionEffects(bench.world, bench.stage, bench.planner, lifted_ms);
  static_cast<void>(bench.nodes.Drawn(lifted_ms, bench.camera));
  CHECK(bench.LivingFrom(kGravitationFieldEffect) == 0);
}

TEST_CASE("Une unité renvoyée à une autre case emporte ce qui la suit, pas le reste",
          "[effect_update_quake]") {
  Bench bench;
  const auto place = [&bench](std::uint32_t id, ro::sim::CellPosition cell, std::uint16_t unit) {
    bench.world.PlaceSkillUnit(ro::sim::SkillUnit{.id = id,
                                                  .cell = cell,
                                                  .unit_id = unit,
                                                  .skill_level = 5,
                                                  .placed_ms = kNowMs,
                                                  .look_since_ms = kNowMs});
  };
  place(kLullabyUnitId, kAimedCell, kLullabyUnit);
  place(kGravitationUnitId, kAimedCell, ro::render::kGravitationFieldUnit);
  bench.effects.NoticeSkillUnitVisuals(bench.world, bench.stage, kNowMs);
  static_cast<void>(bench.nodes.Drawn(kNowMs, bench.camera));
  const auto positions = [&bench](std::uint32_t effect_id) {
    std::vector<glm::vec3> found;
    for (const ro::render::PooledEffectNode& node : bench.nodes.living_nodes()) {
      if (node.sown_by_effect_id == effect_id) {
        found.push_back(node.ground_position);
      }
    }
    return found;
  };
  const std::vector<glm::vec3> song = positions(kLullabyEffect);
  const std::vector<glm::vec3> field = positions(kGravitationFieldEffect);
  REQUIRE_FALSE(song.empty());
  REQUIRE_FALSE(field.empty());

  // Le serveur renvoie les deux unités, mêmes identifiants, une case plus loin :
  // c'est ce qu'il fait d'une chanson à chaque pas de son barde.
  place(kLullabyUnitId, kStepCell, kLullabyUnit);
  place(kGravitationUnitId, kStepCell, ro::render::kGravitationFieldUnit);
  bench.effects.NoticeSkillUnitVisuals(bench.world, bench.stage, kNowMs + 1);

  // 🔴 LA CHANSON SUIT : son effet recopie la place de son porteur à chaque tic.
  const std::vector<glm::vec3> song_after = positions(kLullabyEffect);
  REQUIRE(song_after.size() == song.size());
  for (std::size_t node = 0; node < song.size(); ++node) {
    CHECK(song_after[node].x > song[node].x);
  }
  // ⚠ LE CHAMP DE GRAVITATION RESTE où il est né : son effet ne suit pas.
  CHECK(positions(kGravitationFieldEffect) == field);
}

TEST_CASE("Earth Strain : l'écran tremble tant que vit l'unité", "[effect_update_quake]") {
  Bench bench;
  bench.world.PlaceSkillUnit(ro::sim::SkillUnit{.id = kEarthStrainUnitId,
                                                .cell = kAimedCell,
                                                .unit_id = kEarthStrainUnit,
                                                .placed_ms = kNowMs,
                                                .look_since_ms = kNowMs});
  bench.effects.NoticeSkills(bench.world, bench.stage, bench.planner, kNowMs);
  const std::optional<std::uint32_t> quake = bench.effects.TakeScreenQuake();
  REQUIRE(quake.has_value());
  CHECK(*quake == kNowMs + ro::render::kEffectQueuedQuakeMs);
  // Redemandé à l'image suivante, puis plus du tout une fois l'unité levée.
  bench.effects.NoticeSkills(bench.world, bench.stage, bench.planner, kNowMs + kSliceTicks);
  CHECK(bench.effects.TakeScreenQuake().has_value());
  REQUIRE(bench.world.RemoveSkillUnit(kEarthStrainUnitId));
  bench.effects.NoticeSkills(bench.world, bench.stage, bench.planner, kNowMs + 2 * kSliceTicks);
  CHECK_FALSE(bench.effects.TakeScreenQuake().has_value());
}
