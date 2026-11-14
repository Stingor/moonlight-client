// LA VIGNETTE D'UN MODÈLE DU DÉCOR, rendue sans GPU : ce que l'éditeur de
// carte montre au survol d'un fichier.
//
// 🔴 CE QUE L'ŒIL NE TRANCHE PAS SEUL : la boîte ENTIÈRE tient dans le cadre,
// sans en toucher le bord, et le remplit — un modèle qui déborde perd un toit,
// un modèle perdu au milieu ne se reconnaît plus.

#include "ro/render/decor_vignette.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using ro::render::ModelMesh;
using ro::render::ModelSurface;
using ro::render::ModelVertex;
using ro::render::RasterDecorVignette;

namespace {

constexpr std::int32_t kSide = 96;
constexpr std::size_t kChannels = 4;
constexpr std::size_t kAlpha = 3;
// Une version de la première famille : le modèle se recentre sur sa boîte.
constexpr std::int32_t kFirstFamily = 0x0104;
constexpr std::int32_t kShaded = 1;
constexpr std::int32_t kUnshaded = 0;
// La marge de la vignette, 6 % du cadre, laisse au moins ces pixels vides
// au bord ; le modèle s'en approche à moins de cette distance sur un axe.
constexpr int kEmptyBorderPx = 3;
constexpr int kNearBorderPx = 12;

// Un pavé de 10 × 20 × 30, faces en triangles, en un seul lot.
[[nodiscard]] ModelMesh Brick() {
  constexpr std::array<std::array<float, 3>, 8> kCorners = {{{0, 0, 0},
                                                             {10, 0, 0},
                                                             {10, 20, 0},
                                                             {0, 20, 0},
                                                             {0, 0, 30},
                                                             {10, 0, 30},
                                                             {10, 20, 30},
                                                             {0, 20, 30}}};
  constexpr std::array<std::array<int, 3>, 12> kFaces = {{{0, 1, 2},
                                                          {0, 2, 3},
                                                          {4, 6, 5},
                                                          {4, 7, 6},
                                                          {0, 4, 5},
                                                          {0, 5, 1},
                                                          {1, 5, 6},
                                                          {1, 6, 2},
                                                          {2, 6, 7},
                                                          {2, 7, 3},
                                                          {3, 7, 4},
                                                          {3, 4, 0}}};
  ModelMesh mesh;
  for (const std::array<int, 3>& face : kFaces) {
    for (const int corner : face) {
      mesh.vertices.push_back(ModelVertex{.position = kCorners[static_cast<std::size_t>(corner)],
                                          .texture_uv = {0.5F, 0.5F},
                                          .normal = {0.0F, 1.0F, 0.0F}});
    }
  }
  mesh.surfaces.push_back(
      ModelSurface{.texture_index = 0,
                   .first_vertex = 0,
                   .vertex_count = static_cast<std::uint32_t>(mesh.vertices.size())});
  mesh.textures.emplace_back("brique.bmp");
  mesh.bounds_min = {0.0F, 0.0F, 0.0F};
  mesh.bounds_max = {10.0F, 20.0F, 30.0F};
  return mesh;
}

[[nodiscard]] const std::uint8_t* PixelAt(const ro::formats::Image& image, int x, int y) {
  return &image.pixels[(static_cast<std::size_t>(y) * static_cast<std::size_t>(image.width) +
                        static_cast<std::size_t>(x)) *
                       kChannels];
}

struct Extent {
  int left = kSide;
  int right = -1;
  int top = kSide;
  int bottom = -1;
};

[[nodiscard]] Extent PaintedExtent(const ro::formats::Image& image) {
  Extent extent;
  for (int y = 0; y < image.height; ++y) {
    for (int x = 0; x < image.width; ++x) {
      if (PixelAt(image, x, y)[kAlpha] != 0) {
        extent.left = std::min(extent.left, x);
        extent.right = std::max(extent.right, x);
        extent.top = std::min(extent.top, y);
        extent.bottom = std::max(extent.bottom, y);
      }
    }
  }
  return extent;
}

}  // namespace

TEST_CASE("Vignette du décor : la boîte entière tient dans le cadre, et le remplit",
          "[render][map_editor]") {
  const ModelMesh mesh = Brick();
  const std::array<std::shared_ptr<const ro::formats::Image>, 1> textures{};
  const ro::formats::Image image =
      RasterDecorVignette(mesh, textures, kFirstFamily, kShaded, kSide);
  REQUIRE(image.width == kSide);
  REQUIRE(image.height == kSide);
  const Extent extent = PaintedExtent(image);
  REQUIRE(extent.right >= extent.left);
  CHECK(extent.left >= kEmptyBorderPx);
  CHECK(extent.top >= kEmptyBorderPx);
  CHECK(extent.right < kSide - kEmptyBorderPx);
  CHECK(extent.bottom < kSide - kEmptyBorderPx);
  const bool fills_width = extent.left < kNearBorderPx && extent.right >= kSide - kNearBorderPx;
  const bool fills_height = extent.top < kNearBorderPx && extent.bottom >= kSide - kNearBorderPx;
  CHECK((fills_width || fills_height));
}

TEST_CASE("Vignette du décor : un modèle sans ombrage se peint à pleine lumière",
          "[render][map_editor]") {
  const ModelMesh mesh = Brick();
  ro::formats::Image red;
  red.width = 1;
  red.height = 1;
  red.pixels = {255, 0, 0, 255};
  const std::array<std::shared_ptr<const ro::formats::Image>, 1> textures = {
      std::make_shared<const ro::formats::Image>(red)};
  const ro::formats::Image image =
      RasterDecorVignette(mesh, textures, kFirstFamily, kUnshaded, kSide);
  const std::uint8_t* middle = PixelAt(image, kSide / 2, kSide / 2);
  CHECK(middle[kAlpha] == 255);
  CHECK(middle[0] == 255);
  CHECK(middle[1] == 0);
  CHECK(middle[2] == 0);
}

TEST_CASE("Vignette du décor : un modèle sans face ne rend rien", "[render][map_editor]") {
  CHECK(RasterDecorVignette(ModelMesh{}, {}, kFirstFamily, kShaded, kSide).empty());
  CHECK(RasterDecorVignette(Brick(), {}, kFirstFamily, kShaded, 0).empty());
}
