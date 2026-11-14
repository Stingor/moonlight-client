// LA VIGNETTE D'UN MODÈLE 3D, rendue sans GPU.
//
// 🔴 TROIS RÈGLES QUE L'ŒIL NE TRANCHE PAS SEUL :
//
//   · le HAUT du modèle — son Z — est en haut de l'image : le signe inverse
//     montre un monstre vu de dessous, image plausible ;
//   · le cadrage remplit le cadre moins sa marge, sur la pose, et centre ;
//   · une face se peint des deux côtés : le drapeau est un plan.

#include "ro/render/model_vignette.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using ro::render::ModelVignetteMesh;
using ro::render::ModelVignetteView;
using ro::render::PosedVertex;
using ro::render::RasterModelVignette;

namespace {

constexpr std::int32_t kSide = 64;
constexpr std::size_t kChannels = 4;
constexpr std::size_t kAlpha = 3;
// Une vue de face, sans plongée ni suréchantillonnage : la géométrie se lit
// directement sur l'image.
constexpr ModelVignetteView kFront{.width = kSide,
                                   .height = kSide,
                                   .yaw_radians = 0.0F,
                                   .pitch_radians = 0.0F,
                                   .margin = 0.0F,
                                   .supersample = false};

// Un triangle dans le plan vertical X-Z, pointe en HAUT.
[[nodiscard]] std::vector<PosedVertex> UpwardTriangle() {
  return {PosedVertex{.position = {-1.0F, 0.0F, 0.0F}, .normal = {0.0F, -1.0F, 0.0F}},
          PosedVertex{.position = {1.0F, 0.0F, 0.0F}, .normal = {0.0F, -1.0F, 0.0F}},
          PosedVertex{.position = {0.0F, 0.0F, 2.0F}, .normal = {0.0F, -1.0F, 0.0F}}};
}

constexpr std::array<std::uint16_t, 3> kOneTriangle = {0, 1, 2};

[[nodiscard]] std::uint8_t AlphaAt(const ro::formats::Image& image, int x, int y) {
  return image.pixels[(static_cast<std::size_t>(y) * static_cast<std::size_t>(image.width) +
                       static_cast<std::size_t>(x)) *
                          kChannels +
                      kAlpha];
}

}  // namespace

TEST_CASE("Vignette de modèle : la pointe du modèle est en haut de l'image",
          "[render][monster_sheet]") {
  const std::vector<PosedVertex> vertices = UpwardTriangle();
  const std::array<ModelVignetteMesh, 1> meshes = {
      ModelVignetteMesh{.vertices = vertices, .indices = kOneTriangle}};
  const ro::formats::Image image = RasterModelVignette(meshes, kFront);
  REQUIRE(image.width == kSide);
  REQUIRE(image.pixels.size() == static_cast<std::size_t>(kSide * kSide) * kChannels);
  // En bas, le triangle couvre toute la largeur ; en haut, presque rien.
  CHECK(AlphaAt(image, kSide / 2, kSide - 2) != 0);
  CHECK(AlphaAt(image, 2, kSide - 2) != 0);
  CHECK(AlphaAt(image, 2, 2) == 0);
  CHECK(AlphaAt(image, kSide / 2, 1) != 0);
}

TEST_CASE("Vignette de modèle : un plan vu de dos se peint quand même", "[render][monster_sheet]") {
  const std::vector<PosedVertex> vertices = UpwardTriangle();
  const std::array<ModelVignetteMesh, 1> meshes = {
      ModelVignetteMesh{.vertices = vertices, .indices = kOneTriangle}};
  ModelVignetteView behind = kFront;
  behind.yaw_radians = 3.14159265F;
  const ro::formats::Image image = RasterModelVignette(meshes, behind);
  REQUIRE_FALSE(image.empty());
  CHECK(AlphaAt(image, kSide / 2, kSide - 2) != 0);
}

TEST_CASE("Vignette de modèle : la marge laisse un bord vide, le fond est transparent",
          "[render][monster_sheet]") {
  const std::vector<PosedVertex> vertices = UpwardTriangle();
  const std::array<ModelVignetteMesh, 1> meshes = {
      ModelVignetteMesh{.vertices = vertices, .indices = kOneTriangle}};
  ModelVignetteView framed = kFront;
  framed.margin = 0.25F;
  const ro::formats::Image image = RasterModelVignette(meshes, framed);
  REQUIRE_FALSE(image.empty());
  CHECK(AlphaAt(image, kSide / 2, kSide - 2) == 0);
  CHECK(AlphaAt(image, kSide / 2, kSide / 2) != 0);
}

TEST_CASE("Vignette de modèle : sans rien à peindre, pas d'image", "[render][monster_sheet]") {
  CHECK(RasterModelVignette({}, kFront).empty());
  const std::vector<PosedVertex> flat = {PosedVertex{}, PosedVertex{}, PosedVertex{}};
  const std::array<ModelVignetteMesh, 1> meshes = {
      ModelVignetteMesh{.vertices = flat, .indices = kOneTriangle}};
  CHECK(RasterModelVignette(meshes, kFront).empty());
}
