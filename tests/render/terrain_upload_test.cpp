// LA COPIE PAR RANGÉES DES GRANDES TEXTURES DU SOL : `BeginUpload` réserve
// toute texture de plus de kSlicedTextureBytes que le téléversement ne repeint
// pas, et `UploadSlice` la copie rangée par rangée, tranche après tranche, en
// passant d'une texture à la suivante sans perdre ni décaler une rangée. Une
// petite texture entre deux grandes part entière au début. Relues, toutes
// sont octet pour octet leur image source, quel que soit le budget.
//
// Sans contexte OpenGL (un poste sans carte graphique), l'essai se saute.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/formats/image.h"
#include "ro/render/gl_context.h"
#include "ro/render/terrain_mesh.h"
#include "ro/render/terrain_renderer.h"

namespace {

using ro::formats::Image;
using ro::render::TerrainRenderer;

constexpr std::size_t kRgbaBytes = 4;
constexpr std::uint8_t kOpaque = 255;
// Le contexte n'a rien à peindre : la plus petite surface suffit.
constexpr int kContextSide = 16;
// DEUX GRANDES TEXTURES de largeurs différentes (288 000 et 270 400 octets,
// au-delà des 262 144 de la découpe), et une petite entre elles.
constexpr int kFirstWidth = 300;
constexpr int kFirstHeight = 240;
constexpr int kSmallSide = 8;
constexpr int kSecondSide = 260;
// Les graines des motifs : chaque texture le sien.
constexpr std::uint32_t kFirstPattern = 3;
constexpr std::uint32_t kSmallPattern = 5;
constexpr std::uint32_t kSecondPattern = 7;
// Le générateur des motifs : un pas d'un congruentiel 32 bits.
constexpr std::uint32_t kLcgMultiplier = 1'664'525U;
constexpr std::uint32_t kLcgIncrement = 1'013'904'223U;
constexpr unsigned kLcgByteShift = 24;
// Une rangée et demie de la première texture : chaque tranche finit au milieu
// d'une rangée, et le reste du budget passe à la texture suivante.
constexpr std::size_t kRowAndHalfBytes = static_cast<std::size_t>(kFirstWidth) * kRgbaBytes * 3 / 2;
// Le budget le plus maigre : la copie avance quand même d'une rangée.
constexpr std::size_t kLeanestBudget = 1;
// Assez de tranches pour finir au budget le plus maigre, pas davantage.
constexpr int kMostSlices = kFirstHeight + kSecondSide + 1;

// Un motif opaque, de texel en texel différent, lu comme une BMP du sol.
Image Pattern(int width, int height, std::uint32_t seed) {
  Image image{.width = width, .height = height, .file = ro::formats::ImageFile::kBmp};
  image.pixels.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) *
                      kRgbaBytes);
  std::uint32_t state = seed;
  for (std::size_t byte = 0; byte < image.pixels.size(); ++byte) {
    state = state * kLcgMultiplier + kLcgIncrement;
    image.pixels[byte] = byte % kRgbaBytes == kRgbaBytes - 1
                             ? kOpaque
                             : static_cast<std::uint8_t>(state >> kLcgByteShift);
  }
  return image;
}

}  // namespace

TEST_CASE("Sol : une grande texture se copie par rangées, tranche après tranche, sans rien perdre",
          "[render][snapshot]") {
  auto context = ro::render::GlContext::CreateHeadless(kContextSide, kContextSide);
  if (!context) SKIP("pas de contexte OpenGL : " + ro::core::Describe(context.error()));
  auto renderer = TerrainRenderer::Create();
  REQUIRE(renderer.has_value());

  const std::vector<Image> textures{Pattern(kFirstWidth, kFirstHeight, kFirstPattern),
                                    Pattern(kSmallSide, kSmallSide, kSmallPattern),
                                    Pattern(kSecondSide, kSecondSide, kSecondPattern)};
  REQUIRE(textures[0].pixels.size() > TerrainRenderer::kSlicedTextureBytes);
  REQUIRE(textures[1].pixels.size() <= TerrainRenderer::kSlicedTextureBytes);
  REQUIRE(textures[2].pixels.size() > TerrainRenderer::kSlicedTextureBytes);
  const ro::render::TerrainMesh mesh;

  for (const std::size_t budget : {kRowAndHalfBytes, kLeanestBudget}) {
    INFO("budget de " << budget << " octets par tranche");
    REQUIRE(renderer->BeginUpload(mesh, textures, Image{}, 0).has_value());
    int slices = 1;
    while (!renderer->UploadSlice(budget)) {
      ++slices;
      REQUIRE(slices <= kMostSlices);
    }
    // Plusieurs tranches pour chaque grande texture : la reprise a servi.
    CHECK(slices > 2);
    for (std::size_t rank = 0; rank < textures.size(); ++rank) {
      INFO("texture " << rank);
      const ro::render::GlTexture* uploaded = renderer->texture(rank);
      REQUIRE(uploaded != nullptr);
      auto read = uploaded->ReadPixels();
      REQUIRE(read.has_value());
      REQUIRE(read->width == textures[rank].width);
      REQUIRE(read->height == textures[rank].height);
      REQUIRE(read->pixels.size() == textures[rank].pixels.size());
      // La première rangée qui diffère, ou la hauteur si aucune.
      const auto differing = std::ranges::mismatch(read->pixels, textures[rank].pixels);
      const auto row_bytes = static_cast<std::ptrdiff_t>(textures[rank].width) *
                             static_cast<std::ptrdiff_t>(kRgbaBytes);
      CHECK((differing.in2 - textures[rank].pixels.begin()) / row_bytes == textures[rank].height);
    }
  }
}
