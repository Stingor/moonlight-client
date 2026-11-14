// LA TEINTURE D'UN EFFET DE CHAPEAU SUR SES NŒUDS : un sprite semé par un
// effet de chapeau, chez son porteur, la prend jusque dans ses poses, halo
// compris ; le même nœud semé hors chapeau reste d'origine.

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/camera.h"
#include "ro/render/effect_dye.h"
#include "ro/render/effect_frame.h"
#include "ro/render/effect_glow_sprite_seed.h"
#include "ro/render/effect_node_pattern_table.h"
#include "ro/vfs/vfs.h"

#include "effects/effect_draw_gate.h"
#include "effects/effect_node_view.h"
#include "support/client_vfs.h"
#include "support/grf_builder.h"
#include "support/image_builder.h"
#include "support/sprite_builder.h"
#include "support/temp_directory.h"

using ro::test_support::ActActionSpec;
using ro::test_support::ActBuildOptions;
using ro::test_support::ActFrameSpec;
using ro::test_support::ActLayerSpec;
using ro::test_support::BuildAct;
using ro::test_support::BuildGrf;
using ro::test_support::BuildSpr;
using ro::test_support::BuildTga;
using ro::test_support::Cp949;
using ro::test_support::GrfFileSpec;
using ro::test_support::SprBuildOptions;
using ro::test_support::SprIndexedFrameSpec;
using ro::test_support::SpriteColor;
using ro::test_support::TempDirectory;
using ro::test_support::TgaPixel;

namespace {

constexpr std::int32_t kSpritePixels = 8;
constexpr std::uint32_t kFirstFrameMs = 100;
constexpr std::uint32_t kBearerId = 150'401;
constexpr int kGreenTurnDegrees = 120;
// Quelques tics donnent son alpha au sprite, né éteint.
constexpr std::int32_t kTicks = 10;

// Un `.act` de soixante-quatre actions d'une image : le style tire sa direction.
std::vector<std::uint8_t> ManyActionsAct() {
  ActBuildOptions options;
  constexpr std::size_t kActions = 64;
  for (std::size_t action = 0; action < kActions; ++action) {
    options.actions.push_back(ActActionSpec{{ActFrameSpec{{ActLayerSpec{}}, {}, -1}}});
    options.speeds.push_back(1.0F);
  }
  return BuildAct(options);
}

std::vector<std::uint8_t> OneFrameSpr() {
  SprBuildOptions options;
  options.indexed.push_back(SprIndexedFrameSpec{
      kSpritePixels, kSpritePixels,
      std::vector<std::uint8_t>(static_cast<std::size_t>(kSpritePixels * kSpritePixels), 1)});
  options.palette = {SpriteColor{}, SpriteColor{.red = 200, .green = 60, .blue = 200}};
  return BuildSpr(options);
}

// Sème le sprite éclatant chez le porteur, marqué ou non « chapeau », sous
// une porte qui teint cet effet chez lui, et rend les poses dessinées.
std::vector<ro::render::DrawnActor> SownPoses(bool from_hat) {
  const ro::render::EffectGlowSpriteNodePattern& pattern =
      ro::render::EffectGlowSpriteNodePatternTable().front();
  const ro::render::EffectGlowSpriteStyleSeed* seed =
      ro::render::EffectGlowSpriteStyleSeedFor(pattern.style);
  REQUIRE(seed != nullptr);

  const TempDirectory client;
  const std::string base(seed->sprite_base);
  client.WriteFile("monstres.grf", BuildGrf({GrfFileSpec{Cp949(base + ".spr"), OneFrameSpr()},
                                             GrfFileSpec{Cp949(base + ".act"), ManyActionsAct()}}));
  auto vfs = ro::vfs::Vfs::Mount({.root = client.path(), .archives = {"monstres.grf"}});
  REQUIRE(vfs.has_value());
  moonclient::EffectNodesInPlay nodes(*vfs);
  moonclient::EffectDrawGate gate;
  gate.hat_dyes.emplace(std::pair{kBearerId, pattern.effect_id},
                        ro::render::MakeEffectDye(kGreenTurnDegrees, 0, 0, false));
  nodes.NoticeDrawGate(gate);
  nodes.SowAsHat(from_hat);
  REQUIRE(nodes.PlayProcedural(pattern.effect_id, glm::vec3(0.0F, 0.0F, 0.0F), std::nullopt, 0.0F,
                               kBearerId));
  nodes.SowAsHat(false);

  const ro::render::Camera camera;
  static_cast<void>(nodes.Drawn(kFirstFrameMs, camera));
  static_cast<void>(nodes.Drawn(
      kFirstFrameMs + static_cast<std::uint32_t>(kTicks * ro::render::kEffectTickMs), camera));
  std::vector<ro::render::DrawnActor> poses;
  for (const moonclient::EffectNodesInPlay::SpriteNodeBatch& lot : nodes.SpriteBatches()) {
    poses.insert(poses.end(), lot.actors.begin(), lot.actors.end());
  }
  return poses;
}

// LES QUADS DU MONDE d'un nœud : les plans de Blitz Beat, semés chez
// le porteur, marquées ou non « chapeau ». Rend leurs lots.
std::vector<ro::render::DrawnWorldQuads> SownWorldLots(bool from_hat) {
  constexpr std::uint32_t kBlitzBeat = 115;
  // Le paramètre sous lequel l'effet suit son porteur.
  constexpr std::int32_t kFollowsBearer = 1;
  constexpr std::int32_t kTexturePixels = 4;
  const std::vector<TgaPixel> texture(static_cast<std::size_t>(kTexturePixels * kTexturePixels),
                                      TgaPixel{.red = 200, .green = 200, .blue = 60});
  std::vector<GrfFileSpec> files;
  for (const ro::render::EffectNodePattern& pattern :
       ro::render::EffectNodePatternsFor(kBlitzBeat)) {
    for (const std::string_view name : ro::render::EffectNodePatternTextures(pattern)) {
      files.push_back(GrfFileSpec{Cp949(name), BuildTga(kTexturePixels, kTexturePixels, texture)});
    }
  }
  const TempDirectory client;
  client.WriteFile("effets.grf", BuildGrf(files));
  auto vfs = ro::vfs::Vfs::Mount({.root = client.path(), .archives = {"effets.grf"}});
  REQUIRE(vfs.has_value());
  moonclient::EffectNodesInPlay nodes(*vfs);
  moonclient::EffectDrawGate gate;
  gate.hat_dyes.emplace(std::pair{kBearerId, kBlitzBeat},
                        ro::render::MakeEffectDye(kGreenTurnDegrees, 0, 0, false));
  nodes.NoticeDrawGate(gate);
  nodes.SowAsHat(from_hat);
  REQUIRE(nodes.PlayProcedural(kBlitzBeat, glm::vec3(0.0F, 0.0F, 0.0F), std::nullopt, 0.0F,
                               kBearerId, kFollowsBearer));
  nodes.SowAsHat(false);
  // Un passage entier : avant lui, rien de ce qui naît ne se dessine.
  const ro::render::Camera camera;
  static_cast<void>(nodes.Drawn(kFirstFrameMs, camera));
  static_cast<void>(
      nodes.Drawn(kFirstFrameMs + static_cast<std::uint32_t>(ro::render::kEffectTickMs), camera));
  return {nodes.WorldQuads().begin(), nodes.WorldQuads().end()};
}

}  // namespace

TEST_CASE("les quads du monde qu'un chapeau sème prennent sa teinture", "[effect_node_hat_dye]") {
  const std::vector<ro::render::DrawnWorldQuads> hat = SownWorldLots(true);
  REQUIRE_FALSE(hat.empty());
  for (const ro::render::DrawnWorldQuads& lot : hat) {
    REQUIRE(lot.dye.has_value());
    CHECK(*lot.dye == ro::render::MakeEffectDye(kGreenTurnDegrees, 0, 0, false));
  }
  const std::vector<ro::render::DrawnWorldQuads> skill = SownWorldLots(false);
  REQUIRE_FALSE(skill.empty());
  for (const ro::render::DrawnWorldQuads& lot : skill) {
    CHECK_FALSE(lot.dye.has_value());
  }
}

TEST_CASE("le sprite qu'un chapeau sème prend sa teinture, halo compris", "[effect_node_hat_dye]") {
  const std::vector<ro::render::DrawnActor> poses = SownPoses(true);
  REQUIRE_FALSE(poses.empty());
  for (const ro::render::DrawnActor& pose : poses) {
    REQUIRE(pose.dye.has_value());
    CHECK(*pose.dye == ro::render::MakeEffectDye(kGreenTurnDegrees, 0, 0, false));
  }
}

TEST_CASE("le même sprite semé hors chapeau reste d'origine", "[effect_node_hat_dye]") {
  const std::vector<ro::render::DrawnActor> poses = SownPoses(false);
  REQUIRE_FALSE(poses.empty());
  for (const ro::render::DrawnActor& pose : poses) {
    CHECK_FALSE(pose.dye.has_value());
  }
}
