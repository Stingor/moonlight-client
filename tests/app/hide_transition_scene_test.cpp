// HIDING, DE BOUT EN BOUT : le changement d'option que le serveur envoie en
// direct fait paraître une bouffée de fumée aux pieds de qui se cache, et de
// qui reparaît. L'essai compose la scène comme le jeu : la trame traverse le
// traducteur réseau, le monde compte le front, les effets le remarquent, et la
// troupe des nœuds rend le sprite à peindre.

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/formats/gnd.h"
#include "ro/formats/rsw.h"
#include "ro/net/world_update.h"
#include "ro/proto/map_packets.h"
#include "ro/render/camera.h"
#include "ro/render/effect_catalog.h"
#include "ro/render/lightmap_atlas.h"
#include "ro/render/map_scene.h"
#include "ro/render/option_effect_table.h"
#include "ro/render/scene_light.h"
#include "ro/render/skill_suite_table.h"
#include "ro/sim/actor.h"
#include "ro/sim/movement.h"
#include "ro/sim/walk_path.h"
#include "ro/sim/world.h"
#include "ro/vfs/encoding.h"
#include "ro/vfs/vfs.h"

#include "effects/effect_node_view.h"
#include "effects/effect_view.h"
#include "support/grf_builder.h"
#include "support/map_builder.h"
#include "support/net_frames.h"
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
using ro::test_support::FrameOf;
using ro::test_support::GndBuildOptions;
using ro::test_support::GrfFileSpec;
using ro::test_support::IdleUnit;
using ro::test_support::RswBuildOptions;
using ro::test_support::SprBuildOptions;
using ro::test_support::SprIndexedFrameSpec;
using ro::test_support::SpriteColor;
using ro::test_support::TempDirectory;

namespace {

constexpr std::uint32_t kPlayerId = 150'777;
constexpr std::uint32_t kOtherId = 150'778;
constexpr std::uint32_t kStartMs = 10'000;
constexpr std::uint32_t kHideMs = 11'000;
constexpr std::uint32_t kShowMs = 14'000;
// Une image plus tard : la troupe a battu, le nœud a pris son alpha.
constexpr std::uint32_t kFrameMs = 17;
constexpr std::int32_t kPuffPixels = 8;
constexpr std::size_t kPuffFrames = 3;
constexpr std::uint8_t kOpaqueAlpha = 255;
// La bouffée naît aux pieds et s'élève à peine à sa première image.
constexpr float kFeetMarginUnits = 2.0F;

void FillSmokeClient(const TempDirectory& client) {
  const auto cp949 = [](std::string_view utf8) {
    auto converted = ro::vfs::Utf8ToCp949(utf8);
    REQUIRE(converted.has_value());
    return *converted;
  };
  SprBuildOptions sprite;
  sprite.indexed.push_back(SprIndexedFrameSpec{
      kPuffPixels, kPuffPixels,
      std::vector<std::uint8_t>(static_cast<std::size_t>(kPuffPixels * kPuffPixels), 1)});
  sprite.palette = {SpriteColor{}, SpriteColor{.red = 90, .green = 90, .blue = 90}};
  ActBuildOptions animation;
  ActActionSpec action;
  for (std::size_t frame = 0; frame < kPuffFrames; ++frame) {
    action.frames.push_back(ActFrameSpec{{ActLayerSpec{}}, {}, -1});
  }
  animation.actions.push_back(action);
  animation.speeds = {4.0F};
  const std::string base(ro::render::kSmokePuffSprite);
  client.WriteFile("fumee.grf", BuildGrf({GrfFileSpec{cp949(base + ".spr"), BuildSpr(sprite)},
                                          GrfFileSpec{cp949(base + ".act"), BuildAct(animation)}}));
}

ro::vfs::Vfs SmokeClient(const TempDirectory& client) {
  FillSmokeClient(client);
  auto mounted = ro::vfs::Vfs::Mount({.root = client.path(), .archives = {"fumee.grf"}});
  REQUIRE(mounted.has_value());
  return std::move(*mounted);
}

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

// Le changement d'option tel que le serveur l'envoie à la zone.
ro::net::Frame OptionChange(std::uint32_t actor_id, std::uint32_t option) {
  ro::proto::ZcStateChange change;
  change.AID = actor_id;
  change.effectState = static_cast<std::int32_t>(option);
  return FrameOf(Encode(change));
}

// Un sprite de la troupe tel que la peinture du jeu le reçoit, et le mélange
// de son lot.
struct EffectPaint {
  ro::render::DrawnActor drawn;
  bool additive = true;
};

// Le client, la carte, le monde et la troupe, reliés comme dans le jeu.
struct Scene {
  TempDirectory client;
  ro::vfs::Vfs vfs = SmokeClient(client);
  ro::render::EffectCatalog catalog{vfs};
  moonclient::EffectNodesInPlay nodes{vfs};
  moonclient::EffectsInPlay effects{catalog, nodes, vfs};
  ro::sim::World world;
  moonclient::MapStage stage = FlatStage();
  ro::sim::WalkPlanner planner{moonclient::PlannedMapOf(stage)};
  ro::render::Camera camera;

  Scene() {
    for (const std::uint32_t id : {kPlayerId, kOtherId}) {
      REQUIRE(ro::net::ApplyFrame(FrameOf(Encode(IdleUnit(id, ro::sim::CellPosition{5, 5, 0}))),
                                  world, kStartMs) == ro::net::FrameOutcome::kApplied);
    }
    world.set_player_id(kPlayerId);
    Frame(kStartMs);
  }

  // Une image du jeu : ce que les effets remarquent, puis ce que la troupe rend.
  void Frame(std::uint32_t now_ms) {
    effects.NoticeStateChanges(world, stage, planner, now_ms);
    static_cast<void>(nodes.Drawn(now_ms, camera));
    last_frame_ms = now_ms;
  }

  // Les images du jeu jusqu'à cet instant, à soixante par seconde.
  void RunUntil(std::uint32_t now_ms) {
    while (last_frame_ms + kFrameMs <= now_ms) {
      Frame(last_frame_ms + kFrameMs);
    }
  }

  // Une trame reçue, puis l'image qui la suit.
  void Receive(const ro::net::Frame& frame) {
    REQUIRE(ro::net::ApplyFrame(frame, world, last_frame_ms) == ro::net::FrameOutcome::kApplied);
    Frame(last_frame_ms + kFrameMs);
  }

  std::uint32_t last_frame_ms = 0;

  // Les sprites de la troupe que la peinture du jeu recevra, lot par lot.
  [[nodiscard]] std::vector<EffectPaint> Painted() const;

  // Les pieds de l'acteur, pris comme le chemin des effets les prend.
  [[nodiscard]] glm::vec3 FeetOf(std::uint32_t id, std::uint32_t now_ms) {
    const ro::sim::Actor* actor = world.Find(id);
    REQUIRE(actor != nullptr);
    const ro::sim::FractionalCell cell =
        ro::sim::InterpolatedCell(*actor, planner.PathFor(*actor), now_ms);
    const std::array<float, 3> ground = moonclient::GroundPositionAt(stage, cell.x, cell.y);
    return {ground[0], ground[1], ground[2]};
  }
};

std::vector<EffectPaint> Scene::Painted() const {
  std::vector<EffectPaint> painted;
  for (const moonclient::EffectNodesInPlay::SpriteNodeBatch& lot : nodes.SpriteBatches()) {
    for (const ro::render::DrawnActor& drawn : lot.actors) {
      painted.push_back({drawn, lot.additive});
    }
  }
  return painted;
}

// Vrai quand une bouffée visible se peint à ces pieds-là.
bool PuffPaintedAt(const std::vector<EffectPaint>& painted, const glm::vec3& feet) {
  for (const EffectPaint& paint : painted) {
    const glm::vec3 offset = paint.drawn.ground_position - feet;
    if (paint.drawn.actor != nullptr && !paint.drawn.actor->quads.empty() &&
        paint.drawn.tint[3] > 0 && std::abs(offset.x) < kFeetMarginUnits &&
        std::abs(offset.y) < kFeetMarginUnits && std::abs(offset.z) < kFeetMarginUnits) {
      return true;
    }
  }
  return false;
}

}  // namespace

TEST_CASE("Hiding : la fumée paraît aux pieds du joueur qui se cache, puis qui reparaît",
          "[hiding][scene]") {
  Scene scene;
  scene.RunUntil(kHideMs);
  const glm::vec3 feet = scene.FeetOf(kPlayerId, kHideMs);
  CHECK(scene.Painted().empty());

  scene.Receive(OptionChange(kPlayerId, ro::sim::kOptionHide));
  CHECK(PuffPaintedAt(scene.Painted(), feet));

  // La bouffée joue une passe et s'éteint : rien ne la redemande tant que
  // l'option ne change pas.
  scene.RunUntil(kShowMs);
  CHECK(scene.Painted().empty());

  scene.Receive(OptionChange(kPlayerId, 0));
  CHECK(PuffPaintedAt(scene.Painted(), feet));
}

TEST_CASE("Hiding : un autre qui se cache fait sa fumée, lui qui ne se dessine plus",
          "[hiding][scene]") {
  Scene scene;
  scene.RunUntil(kHideMs);
  const glm::vec3 feet = scene.FeetOf(kOtherId, kHideMs);
  scene.Receive(OptionChange(kOtherId, ro::sim::kOptionHide));
  CHECK(PuffPaintedAt(scene.Painted(), feet));
}

TEST_CASE("Hiding : la fumée se peint comme un sprite d'acteur, et non en addition",
          "[hiding][scene]") {
  // Une bouffée grise ajoutée au décor l'éclaircit à peine, et sur un sol
  // clair elle ne se voit plus : le client la peint au mélange ordinaire,
  // comme le corps qu'elle entoure, et pleine dès sa première image.
  Scene scene;
  scene.RunUntil(kHideMs);
  scene.Receive(OptionChange(kPlayerId, ro::sim::kOptionHide));
  const std::vector<EffectPaint> painted = scene.Painted();
  REQUIRE_FALSE(painted.empty());
  for (const EffectPaint& paint : painted) {
    CHECK_FALSE(paint.additive);
    CHECK(paint.drawn.tint[3] == kOpaqueAlpha);
  }
}
