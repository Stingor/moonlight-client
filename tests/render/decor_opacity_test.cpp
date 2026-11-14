// L'OPACITÉ DU DÉCOR DANS UNE IMAGE : le document d'édition mêlé au voile, et
// les poses qui portent encore ombre. Sans GL.

#include "ro/render/decor_opacity.h"

#include <cstdint>
#include <optional>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using ro::mapedit::ModelOpacity;
using ro::render::CombinedOpacity;
using ro::render::DecorOpacity;
using ro::render::kVeilOpaque;
using ro::render::ModelInstances;
using ro::render::ModelOpacities;
using ro::render::OpacityByte;
using ro::render::VeiledPose;

namespace {

constexpr std::uint8_t kHalf = 128;
constexpr std::uint8_t kQuarter = 64;

// Deux modèles : le premier pose les rangs 0 et 2 figés, puis le rang 3 animé ;
// le second le rang 1.
std::vector<ModelInstances> TwoModels() {
  std::vector<ModelInstances> models(2);
  models[0].transforms = {glm::mat4(1.0F), glm::mat4(1.0F)};
  models[0].rsw_ranks = {0, 2};
  models[0].animated.push_back(
      ro::render::ModelPlaybackGroup{.transforms = {glm::mat4(1.0F)}, .rsw_ranks = {3}});
  models[1].transforms = {glm::mat4(1.0F)};
  models[1].rsw_ranks = {1};
  return models;
}

}  // namespace

TEST_CASE("Opacité du décor : le produit, arrondi, le plein étant neutre", "[render][veil]") {
  CHECK(OpacityByte(1.0F) == kVeilOpaque);
  CHECK(OpacityByte(0.0F) == 0);
  CHECK(OpacityByte(0.5F) == kHalf);
  CHECK(OpacityByte(2.0F) == kVeilOpaque);
  CHECK(CombinedOpacity(kVeilOpaque, kHalf) == kHalf);
  CHECK(CombinedOpacity(kHalf, kVeilOpaque) == kHalf);
  CHECK(CombinedOpacity(kHalf, kHalf) == kQuarter);
  CHECK(CombinedOpacity(0, kVeilOpaque) == 0);
}

TEST_CASE("Opacité du décor : le document se range pose par pose, comme le voile",
          "[render][veil]") {
  const std::vector<ModelInstances> models = TwoModels();
  ModelOpacities document;
  ro::render::DocumentOpacities(models, {}, document);
  CHECK(document.empty());

  const std::vector<ModelOpacity> by_rank{{.model_index = 2, .opacity = 0.5F},
                                          {.model_index = 3, .opacity = 0.0F}};
  ro::render::DocumentOpacities(models, by_rank, document);
  REQUIRE(document.size() == 2);
  CHECK(document[0] == std::vector<std::uint8_t>{kVeilOpaque, kHalf, 0});
  CHECK(document[1] == std::vector<std::uint8_t>{kVeilOpaque});
  CHECK(ro::render::OpacitySlot(models[0], 0, 0) == 2);

  // Un rang translucide qu'aucune pose ne dessine : rien à composer.
  const std::vector<ModelOpacity> off_map{{.model_index = 9, .opacity = 0.5F}};
  ro::render::DocumentOpacities(models, off_map, document);
  CHECK(document.empty());
}

TEST_CASE("Opacité du décor : sans voile, le document seul nourrit les deux passes",
          "[render][veil]") {
  const std::vector<ModelInstances> models = TwoModels();
  const std::vector<ModelOpacity> by_rank{{.model_index = 2, .opacity = 0.5F},
                                          {.model_index = 3, .opacity = 0.0F}};
  ModelOpacities document;
  ro::render::DocumentOpacities(models, by_rank, document);
  DecorOpacity decor;
  ro::render::ComposeDecorOpacity(models, document, nullptr, {}, decor);
  CHECK(decor.opacities == document);
  // La pose de rang 2 se peint translucide ; celle d'opacité nulle, pas du tout.
  REQUIRE(decor.translucent.size() == 1);
  CHECK(decor.translucent[0].model_index == 0);
  CHECK_FALSE(decor.translucent[0].group.has_value());
  CHECK(decor.translucent[0].index == 1);
  CHECK(decor.translucent[0].opacity == kHalf);
  CHECK(decor.translucent[0].across);
}

TEST_CASE("Opacité du décor : le voile se multiplie au document", "[render][veil]") {
  const std::vector<ModelInstances> models = TwoModels();
  const std::vector<ModelOpacity> by_rank{{.model_index = 2, .opacity = 0.5F}};
  ModelOpacities document;
  ro::render::DocumentOpacities(models, by_rank, document);
  // Le voile efface la pose de rang 2, que la vue montre, et celle du second
  // modèle, qu'elle ne montre pas.
  const ModelOpacities veil{{kVeilOpaque, kHalf, kVeilOpaque}, {kHalf}};
  const std::vector<VeiledPose> veiled{
      VeiledPose{.model_index = 0, .index = 1, .opacity = kHalf, .across = false}};
  DecorOpacity decor;
  ro::render::ComposeDecorOpacity(models, document, &veil, veiled, decor);
  CHECK(decor.opacities[0] == std::vector<std::uint8_t>{kVeilOpaque, kQuarter, kVeilOpaque});
  CHECK(decor.opacities[1] == std::vector<std::uint8_t>{kHalf});
  REQUIRE(decor.translucent.size() == 1);
  CHECK(decor.translucent[0].opacity == kQuarter);
  CHECK_FALSE(decor.translucent[0].across);
}

TEST_CASE("Opacité du décor : sous la moitié, une pose ne porte plus d'ombre", "[render][veil]") {
  ModelOpacities casting;
  ro::render::ShadowCastingOpacities({{kVeilOpaque, kHalf}}, casting);
  CHECK(casting.empty());
  ro::render::ShadowCastingOpacities({{kVeilOpaque, kHalf - 1, 0}}, casting);
  CHECK(casting == ModelOpacities{{kVeilOpaque, 0, 0}});
}
