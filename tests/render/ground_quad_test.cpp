#include "ro/render/ground_quad.h"

#include <array>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/formats/gat.h"
#include "ro/formats/gnd.h"
#include "ro/render/ground_cursor.h"
#include "ro/render/terrain_mesh.h"

#include "support/map_builder.h"

using ro::formats::GatMap;
using ro::formats::GndMap;
using ro::render::CellPositionAt;
using ro::render::ComposeGroundCellQuad;
using ro::render::ComposeGroundCursor;
using ro::render::GroundQuad;
using ro::render::kGroundCursorColor;
using ro::render::kWorldUnitsPerCell;
using ro::test_support::BuildGat;
using ro::test_support::BuildGnd;
using ro::test_support::GatCellSpec;
using ro::test_support::GndBuildOptions;

namespace {

constexpr std::array<std::uint8_t, 4> kTestColor = {10, 20, 30, 40};

// Deux cubes sur deux, donc quatre cellules sur quatre : c'est le rapport du
// jeu, et le prendre pour un contre un masquerait toute confusion entre les
// deux grilles.
GndMap TwoByTwoGnd() {
  GndBuildOptions options;
  options.width = 2;
  options.height = 2;
  options.texture_paths = {"sol.bmp"};
  options.tile_count = 1;
  auto gnd = GndMap::Parse(BuildGnd(options));
  REQUIRE(gnd.has_value());
  return std::move(*gnd);
}

// Une grille dont la cellule (1, 2) porte QUATRE hauteurs différentes : toute
// permutation des coins se lit sur la valeur rendue.
GatMap SlopedGat(std::int32_t type_at_one_two = 0) {
  std::vector<GatCellSpec> cells(16,
                                 GatCellSpec{.corner_height = {0.0F, 0.0F, 0.0F, 0.0F}, .type = 0});
  cells[2 * 4 + 1] =
      GatCellSpec{.corner_height = {1.0F, 2.0F, 3.0F, 4.0F}, .type = type_at_one_two};
  auto gat = GatMap::Parse(BuildGat(4, 4, cells));
  REQUIRE(gat.has_value());
  return std::move(*gat);
}

}  // namespace

TEST_CASE("Quad de sol : l'emprise est celle de la CELLULE", "[render][groundquad]") {
  const GndMap gnd = TwoByTwoGnd();
  const GatMap gat = SlopedGat();
  const std::optional<GroundQuad> quad = ComposeGroundCellQuad(gnd, gat, 1, 2, kTestColor);
  REQUIRE(quad.has_value());

  // Le centre du quad est celui que la conversion des acteurs donne : les deux
  // doivent désigner la même case, sans quoi la marque annoncerait un pas
  // ailleurs que là où il a lieu.
  const std::array<float, 3> center = CellPositionAt(gnd, 1.0F, 2.0F);
  constexpr float kHalf = kWorldUnitsPerCell / 2.0F;
  float sum_x = 0.0F;
  float sum_z = 0.0F;
  for (const glm::vec3& corner : quad->corners) {
    sum_x += corner.x;
    sum_z += corner.z;
  }
  CHECK(sum_x / 4.0F == Catch::Approx(center[0]));
  CHECK(sum_z / 4.0F == Catch::Approx(center[2]));

  // Une cellule fait CINQ unités de côté : la moitié d'un cube.
  CHECK(quad->corners[0].x == Catch::Approx(center[0] - kHalf));
  CHECK(quad->corners[1].x == Catch::Approx(center[0] + kHalf));
  CHECK(quad->corners[2].x == Catch::Approx(center[0] - kHalf));
  CHECK(quad->corners[3].x == Catch::Approx(center[0] + kHalf));

  // ⚠ UNE CELLULE DE PLUS, C'EST UN Z DE MOINS. Les coins 2 et 3 sont ceux de
  // la plus grande ordonnée de cellule, donc les plus PROCHES en Z.
  CHECK(quad->corners[0].z == Catch::Approx(center[2] + kHalf));
  CHECK(quad->corners[1].z == Catch::Approx(center[2] + kHalf));
  CHECK(quad->corners[2].z == Catch::Approx(center[2] - kHalf));
  CHECK(quad->corners[3].z == Catch::Approx(center[2] - kHalf));
}

TEST_CASE("Quad de sol : les hauteurs sont celles de la GRILLE, inversées",
          "[render][groundquad]") {
  // Le sol dessiné est plat ; la cellule, elle, est en pente. C'est la pente
  // qui doit sortir : on marque le sol sur lequel on MARCHE, pas celui qu'on
  // voit — et les deux diffèrent sur un pont.
  const GndMap gnd = TwoByTwoGnd();
  const GatMap gat = SlopedGat();
  const std::optional<GroundQuad> quad = ComposeGroundCellQuad(gnd, gat, 1, 2, kTestColor);
  REQUIRE(quad.has_value());

  CHECK(quad->corners[0].y == Catch::Approx(-1.0F));
  CHECK(quad->corners[1].y == Catch::Approx(-2.0F));
  CHECK(quad->corners[2].y == Catch::Approx(-3.0F));
  CHECK(quad->corners[3].y == Catch::Approx(-4.0F));
}

TEST_CASE("Quad de sol : la texture se pose d'un coin à l'autre", "[render][groundquad]") {
  const GndMap gnd = TwoByTwoGnd();
  const GatMap gat = SlopedGat();
  const std::optional<GroundQuad> quad = ComposeGroundCellQuad(gnd, gat, 1, 2, kTestColor);
  REQUIRE(quad.has_value());

  // L'ordre du client : (0,0) (1,0) (0,1) (1,1).
  CHECK(quad->texture_uv[0] == glm::vec2{0.0F, 0.0F});
  CHECK(quad->texture_uv[1] == glm::vec2{1.0F, 0.0F});
  CHECK(quad->texture_uv[2] == glm::vec2{0.0F, 1.0F});
  CHECK(quad->texture_uv[3] == glm::vec2{1.0F, 1.0F});
  CHECK(quad->color == kTestColor);
}

TEST_CASE("Quad de sol : hors de la grille, aucun quad", "[render][groundquad]") {
  const GndMap gnd = TwoByTwoGnd();
  const GatMap gat = SlopedGat();
  CHECK_FALSE(ComposeGroundCellQuad(gnd, gat, -1, 0, kTestColor).has_value());
  CHECK_FALSE(ComposeGroundCellQuad(gnd, gat, 0, -1, kTestColor).has_value());
  CHECK_FALSE(ComposeGroundCellQuad(gnd, gat, 4, 0, kTestColor).has_value());
  CHECK_FALSE(ComposeGroundCellQuad(gnd, gat, 0, 4, kTestColor).has_value());
}

TEST_CASE("Curseur au sol : il ne se pose que sur une case OUVERTE", "[render][groundcursor]") {
  const GndMap gnd = TwoByTwoGnd();

  // Marchable : le cadre paraît, dans la couleur relevée chez le client.
  const std::optional<GroundQuad> open = ComposeGroundCursor(gnd, SlopedGat(0), 1, 2);
  REQUIRE(open.has_value());
  CHECK(open->color == kGroundCursorColor);

  // Mur, et case seulement tirable : le client cache le cadre dans les deux
  // cas. Le montrer promettrait un pas qui n'aura pas lieu.
  CHECK_FALSE(ComposeGroundCursor(gnd, SlopedGat(1), 1, 2).has_value());
  CHECK_FALSE(ComposeGroundCursor(gnd, SlopedGat(5), 1, 2).has_value());

  // Le DRAPEAU D'EAU ne change pas la réponse : il fait d'une case marchable
  // une case d'eau marchable, et laisse un mur être un mur. Le garder dans la
  // comparaison ferait paraître le cadre sur un demi-million de murs par carte.
  CHECK(ComposeGroundCursor(gnd, SlopedGat(static_cast<std::int32_t>(0x80000000U)), 1, 2)
            .has_value());
  CHECK_FALSE(ComposeGroundCursor(gnd, SlopedGat(static_cast<std::int32_t>(0x80000001U)), 1, 2)
                  .has_value());
}
