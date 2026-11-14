// UN FICHIER `.str` ET DES NŒUDS : les deux se jouent, et le fichier se peint
// par-dessus les nœuds ordinaires.
//
// Signum Crucis déroule sa croix en fichier ET fait jaillir, au trentième tic,
// la couronne et les éclats d'un coup de Bash. Fire Bolt pose sa flèche et son
// anneau en nœuds, et l'éclat de feu de chaque frappe en fichier : le client
// montre l'éclat PAR-DESSUS la flèche.

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
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
#include "ro/render/effect_render_word.h"
#include "ro/render/effect_screen_burst_seed.h"
#include "ro/render/effect_table.h"
#include "ro/render/lightmap_atlas.h"
#include "ro/render/map_scene.h"
#include "ro/render/scene_light.h"
#include "ro/render/skill_hit_effect.h"
#include "ro/sim/actor.h"
#include "ro/sim/walk_path.h"
#include "ro/sim/world.h"
#include "ro/vfs/vfs.h"

#include "effects/effect_node_view.h"
#include "effects/effect_view.h"
#include "support/client_vfs.h"
#include "support/effect_builder.h"
#include "support/grf_builder.h"
#include "support/image_builder.h"
#include "support/map_builder.h"
#include "support/sprite_builder.h"
#include "support/temp_directory.h"
#include "world/map_stage.h"

using ro::test_support::ActActionSpec;
using ro::test_support::ActBuildOptions;
using ro::test_support::ActFrameSpec;
using ro::test_support::ActLayerSpec;
using ro::test_support::BuildAct;
using ro::test_support::BuildGnd;
using ro::test_support::BuildGrf;
using ro::test_support::BuildRsw;
using ro::test_support::BuildSpr;
using ro::test_support::BuildStr;
using ro::test_support::BuildTga;
using ro::test_support::Cp949;
using ro::test_support::GndBuildOptions;
using ro::test_support::GrfFileSpec;
using ro::test_support::RswBuildOptions;
using ro::test_support::SprBuildOptions;
using ro::test_support::SprIndexedFrameSpec;
using ro::test_support::SpriteColor;
using ro::test_support::StrBuildOptions;
using ro::test_support::StrKeyFrameSpec;
using ro::test_support::StrLayerSpec;
using ro::test_support::TempDirectory;
using ro::test_support::TgaPixel;

namespace {

constexpr std::uint32_t kCasterId = 150'021;
constexpr std::uint32_t kNowMs = 10'000;
// AL_CRUCIS : son annonce pose l'effet 40 sur le lanceur, qui a un fichier et
// une gerbe d'écran.
constexpr std::uint16_t kSignumCrucisSkill = 32;
constexpr std::uint32_t kSignumCrucisEffect = 40;
constexpr const char* kSignumCrucisFile = "data\\texture\\effect\\Cross.str";
constexpr std::int32_t kSkillLevel = 1;
// Assez d'images pour que la croix vive encore quand la gerbe jaillit.
constexpr std::int32_t kLongStrLastFrame = 60;
// EF_FIREARROW : la flèche, ses flammes et son anneau d'impact.
constexpr std::uint32_t kFireArrowEffect = 31;
constexpr std::int32_t kPlainParam = 1;
// De quoi voir la flèche naître (douzième tic) et les flammes la suivre.
constexpr std::int32_t kFireArrowTicks = 24;
constexpr float kFireArrowReach = 50.0F;
// MG_FIREBOLT : chaque frappe pose son éclat de feu sur la cible.
constexpr std::uint16_t kFireBoltSkill = 19;
constexpr ro::sim::CellPosition kCasterCell{.x = 1, .y = 1};
constexpr std::int32_t kGroundCells = 4;
constexpr std::int32_t kImagePixels = 8;
constexpr float kViewportWidth = 640.0F;
constexpr float kViewportHeight = 480.0F;

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

std::vector<std::uint8_t> PlainTga() {
  return BuildTga(kImagePixels, kImagePixels,
                  std::vector<TgaPixel>(static_cast<std::size_t>(kImagePixels * kImagePixels),
                                        TgaPixel{.red = 200, .green = 200, .blue = 60}));
}

std::vector<std::uint8_t> OneFrameSpr() {
  SprBuildOptions options;
  options.indexed.push_back(SprIndexedFrameSpec{
      kImagePixels, kImagePixels,
      std::vector<std::uint8_t>(static_cast<std::size_t>(kImagePixels * kImagePixels), 1)});
  options.palette = {SpriteColor{}, SpriteColor{.red = 0, .green = 200, .blue = 60}};
  return BuildSpr(options);
}

std::vector<std::uint8_t> OneFrameAct() {
  ActBuildOptions options;
  options.actions.push_back(ActActionSpec{{ActFrameSpec{{ActLayerSpec{}}, {}, -1}}});
  options.speeds = {1.0F};
  return BuildAct(options);
}

std::vector<std::uint8_t> LongStr() {
  StrKeyFrameSpec image;
  image.offset_x = ro::formats::kStrOriginX;
  image.offset_y = ro::formats::kStrOriginY;
  StrLayerSpec layer;
  layer.key_frames = {image};
  StrBuildOptions file;
  file.last_frame = kLongStrLastFrame;
  file.layers = {layer};
  return BuildStr(file);
}

// La croix de Signum Crucis et les deux images de sa gerbe d'écran.
std::vector<GrfFileSpec> SignumCrucisFiles() {
  std::vector<GrfFileSpec> files{{.name = kSignumCrucisFile, .content = LongStr()}};
  for (const ro::render::EffectScreenBurstPattern& pattern :
       ro::render::EffectScreenBurstPatternsFor(kSignumCrucisEffect)) {
    files.push_back(GrfFileSpec{Cp949(pattern.texture), PlainTga()});
  }
  return files;
}

// Ce que les patrons de la flèche de feu nomment : des images et un sprite.
std::vector<GrfFileSpec> FireArrowFiles() {
  std::vector<GrfFileSpec> files;
  for (const ro::render::EffectNodePattern& pattern :
       ro::render::EffectNodePatternsFor(kFireArrowEffect)) {
    for (const std::string_view name : ro::render::EffectNodePatternTextures(pattern)) {
      if (pattern.shape == ro::render::EffectNodeShape::kSprite) {
        files.push_back(GrfFileSpec{Cp949(std::string(name) + ".spr"), OneFrameSpr()});
        files.push_back(GrfFileSpec{Cp949(std::string(name) + ".act"), OneFrameAct()});
      } else {
        files.push_back(GrfFileSpec{Cp949(name), PlainTga()});
      }
    }
  }
  return files;
}

// Toutes les variantes du fichier d'un effet de compétence tiré au sort.
std::vector<GrfFileSpec> EffectFileVariants(std::uint32_t effect_id) {
  const std::span<const ro::render::EffectEntry> table = ro::render::EffectPrimitiveTable();
  const auto entry = std::ranges::find(table, effect_id, &ro::render::EffectEntry::id);
  REQUIRE(entry != table.end());
  const std::uint32_t draws = std::max<std::uint32_t>(entry->variants, 1);
  std::vector<GrfFileSpec> files;
  for (std::uint32_t draw = 0; draw < draws; ++draw) {
    files.push_back(GrfFileSpec{
        .name = std::string(ro::render::kEffectFolder) + ro::render::EffectFile(effect_id, draw),
        .content = LongStr()});
  }
  return files;
}

ro::vfs::Vfs MountWith(const TempDirectory& client, const std::vector<GrfFileSpec>& files) {
  client.WriteFile("effet.grf", BuildGrf(files));
  auto mounted = ro::vfs::Vfs::Mount({.root = client.path(), .archives = {"effet.grf"}});
  REQUIRE(mounted.has_value());
  return std::move(*mounted);
}

// Un lanceur seul sur une carte plate.
struct Bench {
  explicit Bench(const std::vector<GrfFileSpec>& files) : vfs(MountWith(client, files)) {
    ro::sim::Actor caster;
    caster.id = kCasterId;
    caster.kind = ro::sim::ActorKind::kPlayer;
    caster.position = kCasterCell;
    world.Spawn(caster);
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

ro::render::Camera CameraOn(const std::array<float, 3>& place) {
  ro::render::Camera camera;
  camera.look_at = {place[0], place[1], place[2]};
  camera.distance = 100.0F;
  camera.pitch_degrees = 45.0F;
  camera.yaw_degrees = 0.0F;
  camera.fov_degrees = 45.0F;
  return camera;
}

std::uint32_t AfterTicks(std::uint32_t from_ms, std::int32_t ticks) {
  return from_ms + static_cast<std::uint32_t>(ticks * ro::render::kEffectTickMs);
}

}  // namespace

TEST_CASE("Signum Crucis joue sa croix ET fait jaillir sa gerbe au trentième tic",
          "[effect_str_and_nodes]") {
  Bench bench(SignumCrucisFiles());
  bench.effects.PlaySkillOn(bench.world, bench.stage, bench.planner, *bench.world.Find(kCasterId),
                            kSignumCrucisSkill, kCasterId, kNowMs, kSkillLevel,
                            moonclient::SkillMoment::kAnnounced, kSkillLevel);
  // Le fichier se joue…
  CHECK(bench.effects.playing() == 1);
  // … et le patron aussi : son horloge attend le trentième tic sur le lanceur.
  std::vector<std::uint32_t> bearer_ids;
  bench.nodes.CollectBearerIds(bearer_ids);
  REQUIRE(std::ranges::find(bearer_ids, kCasterId) != bearer_ids.end());

  const std::array<float, 3> place = moonclient::GroundPositionAt(
      bench.stage, static_cast<float>(kCasterCell.x), static_cast<float>(kCasterCell.y));
  const ro::render::Camera camera = CameraOn(place);
  std::int32_t sow_tick = 0;
  for (const ro::render::EffectScreenBurstPattern& pattern :
       ro::render::EffectScreenBurstPatternsFor(kSignumCrucisEffect)) {
    sow_tick = std::max(sow_tick, pattern.sow_tick);
  }
  REQUIRE(sow_tick > 0);
  // Le premier passage ne rattrape rien : il pose la référence.
  static_cast<void>(bench.nodes.Drawn(kNowMs, camera));
  static_cast<void>(bench.nodes.Drawn(AfterTicks(kNowMs, sow_tick - 1), camera));
  CHECK(bench.nodes.ScreenQuads(camera, kViewportWidth, kViewportHeight).empty());
  static_cast<void>(bench.nodes.Drawn(AfterTicks(kNowMs, sow_tick + 1), camera));
  CHECK_FALSE(bench.nodes.ScreenQuads(camera, kViewportWidth, kViewportHeight).empty());
}

TEST_CASE("la flèche de Fire Bolt se range sous les fichiers .str", "[effect_str_and_nodes]") {
  Bench bench(FireArrowFiles());
  const glm::vec3 origin{0.0F, 0.0F, 0.0F};
  REQUIRE(bench.nodes.PlayProcedural(kFireArrowEffect, origin,
                                     origin + glm::vec3{kFireArrowReach, 0.0F, 0.0F}, 0.0F,
                                     kCasterId, kPlainParam));
  const ro::render::Camera camera = CameraOn({0.0F, 0.0F, 0.0F});
  static_cast<void>(bench.nodes.Drawn(kNowMs, camera));
  static_cast<void>(bench.nodes.Drawn(AfterTicks(kNowMs, kFireArrowTicks), camera));

  const std::span<const moonclient::EffectNodesInPlay::SpriteNodeBatch> batches =
      bench.nodes.SpriteBatches();
  const std::span<const ro::render::DrawnWorldQuads> lots = bench.nodes.WorldQuads();
  REQUIRE(batches.size() + lots.size() > 0);
  for (const moonclient::EffectNodesInPlay::SpriteNodeBatch& batch : batches) {
    CHECK(batch.paints_before_str);
  }
  for (const ro::render::DrawnWorldQuads& lot : lots) {
    CHECK(ro::render::EffectLotPaintsBeforeStr(lot.render_word));
  }
}

TEST_CASE("l'éclat de feu de Fire Bolt se peint avec les fichiers .str, par-dessus la flèche",
          "[effect_str_and_nodes]") {
  // L'impact que chaque frappe pose est un FICHIER, et non des nœuds : c'est ce
  // qui le range dans le seau des `.str`, vidé après celui de la flèche.
  const std::int32_t fire_hit = ro::render::SkillHitEffect(kFireBoltSkill, true, false);
  REQUIRE(fire_hit >= 0);
  const auto fire_hit_id = static_cast<std::uint32_t>(fire_hit);
  REQUIRE_FALSE(ro::render::EffectFile(fire_hit_id).empty());

  Bench bench(EffectFileVariants(fire_hit_id));
  const std::array<float, 3> place = moonclient::GroundPositionAt(
      bench.stage, static_cast<float>(kCasterCell.x), static_cast<float>(kCasterCell.y));
  REQUIRE(bench.effects.PlayById(fire_hit_id, glm::vec3{place[0], place[1], place[2]}, kNowMs,
                                 kCasterId));
  static_cast<void>(bench.effects.Drawn(kNowMs, bench.stage, bench.world, bench.planner));
  // Ni avant les corps, où la flèche peinte ensuite le recouvrirait, ni à part,
  // par-dessus le monde : il passe au rang des fichiers, après tout ce qui se
  // range sous eux.
  CHECK(bench.effects.DrawnBehindActors().empty());
  CHECK(bench.effects.DrawnAmongActors().size() == 1);
  CHECK(bench.effects.DrawnOverEverything().empty());
}
