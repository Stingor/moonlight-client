// LA TEINTURE D'UN EFFET DE CHAPEAU, DE BOUT EN BOUT, sans fenêtre : un acteur
// porte un effet de chapeau joué par un `.str` ; sa teinture se pose, le relevé
// des chapeaux sème l'effet, la porte de dessin range la teinture sous sa
// naissance, et l'effet dessiné sort teint. C'est l'ordre que suivent la partie
// et le pantin de l'éditeur.

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/formats/gnd.h"
#include "ro/formats/rsw.h"
#include "ro/render/camera.h"
#include "ro/render/effect_catalog.h"
#include "ro/render/effect_dye.h"
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

constexpr std::uint32_t kBearerId = 150'501;
constexpr std::uint32_t kSkillCasterId = 150'502;
constexpr std::uint16_t kHaloNumber = 1;
constexpr std::uint32_t kStartMs = 10'000;
constexpr std::int32_t kTextureSide = 4;
constexpr std::int32_t kHaloLastFrame = 625;
constexpr int kGreenTurnDegrees = 120;
// Le joueur rééquipe son item deux secondes après l'avoir ôté.
constexpr std::uint32_t kReequipAfterMs = 2000;

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

// Un client qui porte la table des effets de chapeau, un `.str` d'une couche et
// sa texture.
ro::vfs::Vfs HaloClient(const TempDirectory& client) {
  ro::test_support::StrLayerSpec layer;
  layer.texture_names = {"halo.tga"};
  layer.key_frames.push_back(ro::test_support::StrKeyFrameSpec{});
  ro::test_support::StrBuildOptions options;
  // Une aura longue, comme bien des costumes : dix secondes avant de se rejouer.
  options.last_frame = kHaloLastFrame;
  options.layers = {layer};
  const std::string table =
      "version: 1\n"
      "hat_effects:\n"
      "  1: {name: 'HAT_EF_ESSAI', file: 'essai\\halo.str'}\n";
  const std::vector<TgaPixel> pixels(static_cast<std::size_t>(kTextureSide * kTextureSide),
                                     TgaPixel{255, 0, 0, 255});
  client.WriteFile("essai.grf",
                   BuildGrf({GrfFileSpec{"data\\yaml_files\\hat_effects.yml",
                                         std::vector<std::uint8_t>(table.begin(), table.end())},
                             GrfFileSpec{"data\\texture\\effect\\essai\\halo.str",
                                         ro::test_support::BuildStr(options)},
                             GrfFileSpec{"data\\texture\\effect\\essai\\halo.tga",
                                         BuildTga(kTextureSide, kTextureSide, pixels)}}));
  auto mounted = ro::vfs::Vfs::Mount({.root = client.path(), .archives = {"essai.grf"}});
  REQUIRE(mounted.has_value());
  return std::move(*mounted);
}

struct Bench {
  TempDirectory client;
  ro::vfs::Vfs vfs = HaloClient(client);
  ro::render::EffectCatalog catalog{vfs};
  moonclient::EffectNodesInPlay nodes{vfs};
  moonclient::EffectsInPlay effects{catalog, nodes, vfs};
  ro::sim::World world;
  moonclient::MapStage stage = FlatStage();
  ro::sim::WalkPlanner planner{moonclient::PlannedMapOf(stage)};

  explicit Bench(std::uint32_t bearer_id) {
    ro::sim::Actor bearer;
    bearer.id = bearer_id;
    bearer.kind = ro::sim::ActorKind::kPlayer;
    bearer.hat_effects = {kHaloNumber};
    world.Spawn(bearer);
    effects.SeeScene(ro::render::Camera{}, moonclient::EffectsInPlay::NamedActorView{});
  }

  // Une image, dans l'ordre de la partie : teinture, relevé, porte, dessin.
  std::vector<ro::render::DrawnEffect> Frame(std::uint32_t now_ms) {
    effects.NoticeHatEffects(world, stage, planner, now_ms);
    effects.NoticeDrawGate(world, nullptr);
    const std::span<const ro::render::DrawnEffect> drawn =
        effects.Drawn(now_ms, stage, world, planner);
    return {drawn.begin(), drawn.end()};
  }
};

}  // namespace

TEST_CASE("Un effet de chapeau teint sort teint du dessin", "[hat_effect_dye_chain]") {
  Bench bench(kBearerId);
  const ro::render::EffectDye green = ro::render::MakeEffectDye(kGreenTurnDegrees, 0, 0, false);
  bench.effects.SetHatEffectDye(kBearerId, kHaloNumber, green);
  // La première image sème et relève ; la porte de la suivante connaît la
  // naissance.
  static_cast<void>(bench.Frame(kStartMs));
  const std::vector<ro::render::DrawnEffect> drawn = bench.Frame(kStartMs + 1);
  REQUIRE_FALSE(drawn.empty());
  for (const ro::render::DrawnEffect& effect : drawn) {
    REQUIRE(effect.dye.has_value());
    CHECK(*effect.dye == green);
  }
}

TEST_CASE("Sans teinture, ou ôtée, l'effet de chapeau reste d'origine", "[hat_effect_dye_chain]") {
  Bench bench(kBearerId);
  static_cast<void>(bench.Frame(kStartMs));
  std::vector<ro::render::DrawnEffect> drawn = bench.Frame(kStartMs + 1);
  REQUIRE_FALSE(drawn.empty());
  for (const ro::render::DrawnEffect& effect : drawn) {
    CHECK_FALSE(effect.dye.has_value());
  }
  // Posée puis ôtée.
  bench.effects.SetHatEffectDye(kBearerId, kHaloNumber,
                                ro::render::MakeEffectDye(kGreenTurnDegrees, 0, 0, false));
  static_cast<void>(bench.Frame(kStartMs + 2));
  bench.effects.SetHatEffectDye(kBearerId, kHaloNumber, std::nullopt);
  static_cast<void>(bench.Frame(kStartMs + 3));
  drawn = bench.Frame(kStartMs + 4);
  REQUIRE_FALSE(drawn.empty());
  for (const ro::render::DrawnEffect& effect : drawn) {
    CHECK_FALSE(effect.dye.has_value());
  }
}

TEST_CASE("La teinture d'un porteur ne passe pas à un autre", "[hat_effect_dye_chain]") {
  // Le même effet, porté par un autre acteur : la teinture rangée sous le
  // premier ne le touche pas.
  Bench bench(kSkillCasterId);
  bench.effects.SetHatEffectDye(kBearerId, kHaloNumber,
                                ro::render::MakeEffectDye(kGreenTurnDegrees, 0, 0, false));
  static_cast<void>(bench.Frame(kStartMs));
  const std::vector<ro::render::DrawnEffect> drawn = bench.Frame(kStartMs + 1);
  REQUIRE_FALSE(drawn.empty());
  for (const ro::render::DrawnEffect& effect : drawn) {
    CHECK_FALSE(effect.dye.has_value());
  }
}

TEST_CASE("Un effet de chapeau teint, ôté puis remis, revient", "[hat_effect_dye_chain]") {
  // Le joueur teint l'aura de son item, le déséquipe, le rééquipe : l'aura
  // revient, et sa teinture avec.
  Bench bench(kBearerId);
  const ro::render::EffectDye green = ro::render::MakeEffectDye(kGreenTurnDegrees, 0, 0, false);
  bench.effects.SetHatEffectDye(kBearerId, kHaloNumber, green);
  static_cast<void>(bench.Frame(kStartMs));
  REQUIRE_FALSE(bench.Frame(kStartMs + 1).empty());

  // Ôté : l'effet s'éteint.
  REQUIRE(bench.world.SetHatEffect(kBearerId, kHaloNumber, false));
  static_cast<void>(bench.Frame(kStartMs + 2));
  CHECK(bench.Frame(kStartMs + 3).empty());

  // Remis : il revient, teint.
  REQUIRE(bench.world.SetHatEffect(kBearerId, kHaloNumber, true));
  static_cast<void>(bench.Frame(kStartMs + kReequipAfterMs));
  const std::vector<ro::render::DrawnEffect> drawn = bench.Frame(kStartMs + kReequipAfterMs + 1);
  REQUIRE_FALSE(drawn.empty());
  for (const ro::render::DrawnEffect& effect : drawn) {
    REQUIRE(effect.dye.has_value());
    CHECK(*effect.dye == green);
  }
}
