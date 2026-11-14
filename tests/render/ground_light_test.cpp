#include "ro/render/ground_light.h"

#include <array>
#include <cstdint>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/formats/gat.h"
#include "ro/formats/gnd.h"

#include "support/map_builder.h"

using Catch::Approx;
using ro::formats::GatMap;
using ro::formats::GndMap;
using ro::render::BuildGroundLight;
using ro::render::GroundLight;
using ro::render::GroundLightTerms;
using ro::render::LitTint;
using ro::test_support::BuildGat;
using ro::test_support::BuildGnd;
using ro::test_support::GatCellSpec;
using ro::test_support::GndBuildOptions;
using ro::test_support::GndCubeSpec;

namespace {

// Trois carreaux sur trois, tous couverts par UNE lightmap uniforme : la case
// du milieu lit ses cinq texels dans la carte.
constexpr std::int32_t kCubes = 3;
constexpr std::int32_t kCells = kCubes * 2;
constexpr std::int32_t kMiddleCell = 2;

GroundLight LightOf(std::uint8_t shadow, std::array<std::uint8_t, 3> rgb, float walk_height = 0.0F,
                    std::int32_t tile_up = 0,
                    GroundLightTerms terms = GroundLightTerms::kShadowAndLaidLight) {
  GndBuildOptions options;
  options.width = kCubes;
  options.height = kCubes;
  options.lightmap_count = 1;
  options.tile_count = 1;
  options.lightmap_shadow = shadow;
  options.lightmap_rgb = rgb;
  options.cubes.assign(static_cast<std::size_t>(kCubes * kCubes), GndCubeSpec{.tile_up = tile_up});
  auto gnd = GndMap::Parse(BuildGnd(options));
  REQUIRE(gnd.has_value());

  GatCellSpec cell;
  cell.corner_height = {walk_height, walk_height, walk_height, walk_height};
  auto gat = GatMap::Parse(BuildGat(
      kCells, kCells, std::vector<GatCellSpec>(static_cast<std::size_t>(kCells * kCells), cell)));
  REQUIRE(gat.has_value());
  return BuildGroundLight(*gnd, *gat, terms);
}

}  // namespace

TEST_CASE("Lumière au sol : sans l'ombre cuite, une case noire d'ombre reste en pleine lumière",
          "[render][lightmap]") {
  constexpr std::uint8_t kFullShadow = 0;
  const GroundLight laid =
      LightOf(kFullShadow, {0, 0, 0}, 0.0F, 0, GroundLightTerms::kLaidLightOnly);
  const std::array<float, 3> factors = laid.At(kMiddleCell, kMiddleCell);
  CHECK(factors[0] == Approx(1.0F));
  CHECK(factors[1] == Approx(1.0F));
  CHECK(factors[2] == Approx(1.0F));

  // Le même sol, ombre cuite comprise : noir.
  CHECK(LightOf(kFullShadow, {0, 0, 0}).At(kMiddleCell, kMiddleCell)[0] == Approx(0.0F));
}

TEST_CASE("Lumière au sol : sans l'ombre cuite, la couleur posée teinte encore",
          "[render][lightmap]") {
  // Une lanterne jaune : le bleu reste en deçà du rouge et du vert.
  constexpr std::uint8_t kFullShadow = 0;
  constexpr std::uint8_t kLantern = 64;
  const GroundLight laid =
      LightOf(kFullShadow, {kLantern, kLantern, 0}, 0.0F, 0, GroundLightTerms::kLaidLightOnly);
  const std::array<float, 3> factors = laid.At(kMiddleCell, kMiddleCell);
  CHECK(factors[0] == Approx(1.0F));
  CHECK(factors[1] == Approx(1.0F));
  CHECK(factors[2] < factors[0]);
}

TEST_CASE("Lumière au sol : une case par case de marche", "[render][lightmap]") {
  const GroundLight light = LightOf(0x40, {0x80, 0x40, 0x00});
  CHECK(light.width == kCells);
  CHECK(light.height == kCells);
  CHECK(light.factors.size() == static_cast<std::size_t>(kCells * kCells));
}

TEST_CASE("Lumière au sol : la couleur posée s'ajoute à l'ombre", "[render][lightmap]") {
  // L'ombre compte sur 255, la couleur sur 128 : 64/255 + 128/128 ne se lirait
  // pas comme 64/255 × 128/255.
  const GroundLight light = LightOf(0x40, {0x40, 0x20, 0x00});
  const std::array<float, 3> factors = light.At(kMiddleCell, kMiddleCell);
  const float shadow = 64.0F / 255.0F;
  CHECK(factors[0] == Approx(64.0F / 128.0F + shadow));
  CHECK(factors[1] == Approx(32.0F / 128.0F + shadow));
  CHECK(factors[2] == Approx(shadow));
}

TEST_CASE("Lumière au sol : un canal trop fort ramène les trois d'autant", "[render][lightmap]") {
  // Ombre pleine et rouge à 128 : le rouge vaut deux, le vert et le bleu un.
  // La teinte se garde — le rouge reste deux fois plus fort.
  const GroundLight light = LightOf(0xFF, {0x80, 0x00, 0x00});
  const std::array<float, 3> factors = light.At(kMiddleCell, kMiddleCell);
  CHECK(factors[0] == Approx(1.0F));
  CHECK(factors[1] == Approx(0.5F));
  CHECK(factors[2] == Approx(0.5F));
}

TEST_CASE("Lumière au sol : au bord, le texel absent compte blanc", "[render][lightmap]") {
  // La case d'angle a deux voisines hors carte, qui apportent chacune une ombre
  // et une couleur pleines : elle est PLUS claire que celle du milieu.
  const GroundLight light = LightOf(0x00, {0x00, 0x00, 0x00});
  CHECK(light.At(kMiddleCell, kMiddleCell)[0] == Approx(0.0F));
  const float corner = light.At(0, 0)[0];
  CHECK(corner == Approx(1.0F));
}

TEST_CASE("Lumière au sol : une case perchée au-dessus du sol reste blanche",
          "[render][lightmap]") {
  // Les hauteurs croissent vers le bas : dix unités de moins, c'est un pont.
  const GroundLight perched = LightOf(0x00, {0x00, 0x00, 0x00}, -10.0F);
  CHECK(perched.At(kMiddleCell, kMiddleCell) == std::array<float, 3>{1.0F, 1.0F, 1.0F});
  // Deux unités seulement : encore sous la marge, la case prend l'ombre.
  const GroundLight close = LightOf(0x00, {0x00, 0x00, 0x00}, -2.0F);
  CHECK(close.At(kMiddleCell, kMiddleCell)[0] == Approx(0.0F));
}

TEST_CASE("Lumière au sol : sans face du dessus, pas de lumière à prendre", "[render][lightmap]") {
  const GroundLight light = LightOf(0x00, {0x00, 0x00, 0x00}, 0.0F, -1);
  CHECK(light.At(kMiddleCell, kMiddleCell) == std::array<float, 3>{1.0F, 1.0F, 1.0F});
}

TEST_CASE("Lumière au sol : hors de la carte, blanc", "[render][lightmap]") {
  const GroundLight light = LightOf(0x00, {0x00, 0x00, 0x00});
  CHECK(light.At(-1, 0) == std::array<float, 3>{1.0F, 1.0F, 1.0F});
  CHECK(light.At(0, kCells) == std::array<float, 3>{1.0F, 1.0F, 1.0F});
  CHECK(GroundLight{}.At(0, 0) == std::array<float, 3>{1.0F, 1.0F, 1.0F});
}

TEST_CASE("Lumière au sol : une cellule entière désigne son centre", "[render][lightmap]") {
  GroundLight light;
  light.width = 2;
  light.height = 1;
  light.factors = {{0.25F, 0.25F, 0.25F}, {0.75F, 0.75F, 0.75F}};
  CHECK(light.Under(0.4F, 0.0F)[0] == Approx(0.25F));
  CHECK(light.Under(0.6F, 0.0F)[0] == Approx(0.75F));
  CHECK(light.Under(-0.4F, 0.0F)[0] == Approx(0.25F));
}

TEST_CASE("Lumière au sol : la teinte d'un corps, alpha intact", "[render][lightmap]") {
  const std::array<std::uint8_t, 4> tint = {200, 100, 50, 128};
  const std::array<float, 3> factors = {0.5F, 1.0F, 0.25F};
  CHECK(LitTint(tint, factors, false) == std::array<std::uint8_t, 4>{100, 100, 12, 128});
  // La lumière froide échange le rouge et le bleu de la LUMIÈRE, pas du corps.
  CHECK(LitTint(tint, factors, true) == std::array<std::uint8_t, 4>{50, 100, 25, 128});
}
