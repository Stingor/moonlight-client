#include "ro/render/navi_trail.h"

#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/formats/gat.h"
#include "ro/formats/gnd.h"

#include "support/map_builder.h"

using ro::formats::GatMap;
using ro::formats::GndMap;
using ro::render::ComposeGroundCellQuad;
using ro::render::ComposeNaviTrail;
using ro::render::ComposeNaviTrailQuad;
using ro::render::GroundQuad;
using ro::render::kNaviTrailAlpha;
using ro::render::kNaviTrailReachCells;
using ro::render::kNaviTrailWaveAlpha;
using ro::render::kNaviTrailWaveRanks;
using ro::render::kNaviTrailWaveStepMs;
using ro::render::NaviTrailDirectionOf;
using ro::render::NaviTrailMark;
using ro::render::NaviTrailTexturePath;
using ro::test_support::BuildGat;
using ro::test_support::BuildGnd;
using ro::test_support::GatCellSpec;
using ro::test_support::GndBuildOptions;

namespace {

// Un chemin droit vers l'est, de `length` cases, départ en (10, 20).
std::vector<glm::ivec2> EastwardPath(int length) {
  std::vector<glm::ivec2> path;
  for (int step = 0; step < length; ++step) {
    path.emplace_back(10 + step, 20);
  }
  return path;
}

// L'instant où la vague éclaire la flèche de ce rang, compté depuis la cible.
constexpr std::uint32_t PhaseOfRank(std::uint32_t from_goal) {
  return ((kNaviTrailWaveRanks - 1 - from_goal % kNaviTrailWaveRanks) % kNaviTrailWaveRanks) *
         kNaviTrailWaveStepMs;
}

}  // namespace

TEST_CASE("La direction d'un pas suit l'ordre des images de flèches", "[render][navi_trail]") {
  CHECK(NaviTrailDirectionOf(0, 1) == 4);
  CHECK(NaviTrailDirectionOf(-1, 1) == 3);
  CHECK(NaviTrailDirectionOf(-1, 0) == 2);
  CHECK(NaviTrailDirectionOf(-1, -1) == 1);
  CHECK(NaviTrailDirectionOf(0, -1) == 0);
  CHECK(NaviTrailDirectionOf(1, -1) == 7);
  CHECK(NaviTrailDirectionOf(1, 0) == 6);
  CHECK(NaviTrailDirectionOf(1, 1) == 5);
  CHECK_FALSE(NaviTrailDirectionOf(0, 0).has_value());
  // Un long segment prend l'octant qui le contient, pas le signe de ses deux
  // composantes.
  CHECK(NaviTrailDirectionOf(10, 1) == 6);
  CHECK(NaviTrailDirectionOf(-3, 12) == 4);
}

TEST_CASE("Une flèche toutes les deux cases, la cible n'en porte pas", "[render][navi_trail]") {
  // Cinq cases : la cible est au rang 4, les rangs 3 et 1 sont à distance impaire.
  const std::vector<NaviTrailMark> odd = ComposeNaviTrail(EastwardPath(5), 0);
  REQUIRE(odd.size() == 2);
  CHECK(odd[0].cell_x == 11);
  CHECK(odd[1].cell_x == 13);

  // Six cases : les rangs 0, 2 et 4.
  const std::vector<NaviTrailMark> even = ComposeNaviTrail(EastwardPath(6), 0);
  REQUIRE(even.size() == 3);
  CHECK(even[0].cell_x == 10);
  CHECK(even[1].cell_x == 12);
  CHECK(even[2].cell_x == 14);

  CHECK(ComposeNaviTrail(EastwardPath(1), 0).empty());
  CHECK(ComposeNaviTrail({}, 0).empty());
}

TEST_CASE("Une flèche reste sur sa case quand le joueur avance", "[render][navi_trail]") {
  const std::vector<glm::ivec2> path = EastwardPath(9);
  const std::vector<NaviTrailMark> before = ComposeNaviTrail(path, 0);
  const std::vector<NaviTrailMark> after = ComposeNaviTrail(std::span(path).subspan(1), 0);
  REQUIRE(before.size() == after.size());
  for (std::size_t mark = 0; mark < before.size(); ++mark) {
    CHECK(before[mark].cell_x == after[mark].cell_x);
  }
}

TEST_CASE("La flèche d'une case pointe vers la suivante", "[render][navi_trail]") {
  // Deux pas vers le nord, puis deux vers l'ouest : (0,0) (0,1) (0,2) (-1,2) (-2,2).
  const std::vector<glm::ivec2> path = {{0, 0}, {0, 1}, {0, 2}, {-1, 2}, {-2, 2}};
  const std::vector<NaviTrailMark> marks = ComposeNaviTrail(path, 0);
  REQUIRE(marks.size() == 2);
  CHECK(marks[0].cell_y == 1);
  CHECK(marks[0].direction == 4);
  // La case d'après le tournant regarde vers l'ouest.
  CHECK(marks[1].cell_x == -1);
  CHECK(marks[1].direction == 2);
}

TEST_CASE("La trace s'arrête à sa portée devant le joueur", "[render][navi_trail]") {
  const std::vector<glm::ivec2> path = EastwardPath(static_cast<int>(kNaviTrailReachCells) * 3);
  const std::vector<NaviTrailMark> marks = ComposeNaviTrail(path, 0);
  REQUIRE_FALSE(marks.empty());
  for (const NaviTrailMark& mark : marks) {
    CHECK(mark.cell_x - path.front().x <= static_cast<int>(kNaviTrailReachCells));
  }
  CHECK(marks.size() == kNaviTrailReachCells / 2);

  CHECK(ComposeNaviTrail(path, 0, 4).size() == 2);
}

TEST_CASE("La vague éclaire une flèche qui court vers la cible", "[render][navi_trail]") {
  // Onze cases, cinq flèches : rangs 1, 3, 5, 7, 9 — cases 11 à 19 ; comptées
  // depuis la cible, 4 à 0.
  const std::vector<glm::ivec2> path = EastwardPath(11);
  const auto lit_cell = [&path](std::uint32_t now_ms) -> std::optional<int> {
    std::optional<int> lit;
    for (const NaviTrailMark& mark : ComposeNaviTrail(path, now_ms)) {
      if (mark.alpha == kNaviTrailWaveAlpha) {
        CHECK_FALSE(lit.has_value());
        lit = mark.cell_x;
      } else {
        CHECK(mark.alpha == kNaviTrailAlpha);
      }
    }
    return lit;
  };
  CHECK(lit_cell(PhaseOfRank(4)) == 11);
  CHECK(lit_cell(PhaseOfRank(3)) == 13);
  CHECK(lit_cell(PhaseOfRank(0)) == 19);
  // Une phase qui vise un rang au-delà du chemin n'éclaire rien.
  CHECK_FALSE(lit_cell(PhaseOfRank(kNaviTrailWaveRanks - 1)).has_value());
  // La période : une seconde plus tard, la même flèche.
  CHECK(lit_cell(PhaseOfRank(3) + kNaviTrailWaveRanks * kNaviTrailWaveStepMs) == 13);
}

TEST_CASE("Le quad d'une flèche lit son image nord en haut", "[render][navi_trail]") {
  GndBuildOptions options;
  options.width = 2;
  options.height = 2;
  options.texture_paths = {"sol.bmp"};
  options.tile_count = 1;
  auto gnd = GndMap::Parse(BuildGnd(options));
  REQUIRE(gnd.has_value());
  const std::vector<GatCellSpec> cells(16, GatCellSpec{});
  auto gat = GatMap::Parse(BuildGat(4, 4, cells));
  REQUIRE(gat.has_value());

  const NaviTrailMark mark{.cell_x = 1, .cell_y = 2, .direction = 0, .alpha = kNaviTrailAlpha};
  const std::optional<GroundQuad> quad = ComposeNaviTrailQuad(*gnd, *gat, mark);
  const std::optional<GroundQuad> cell =
      ComposeGroundCellQuad(*gnd, *gat, 1, 2, {0xFF, 0xFF, 0xFF, kNaviTrailAlpha});
  REQUIRE(quad.has_value());
  REQUIRE(cell.has_value());
  CHECK(quad->corners == cell->corners);
  CHECK(quad->color == cell->color);
  // Coin 0 : cellule y, donc le SUD — le bas de l'image, v = 1.
  CHECK(quad->texture_uv[0] == glm::vec2(0.0F, 1.0F));
  CHECK(quad->texture_uv[3] == glm::vec2(1.0F, 0.0F));

  const NaviTrailMark outside{.cell_x = 9, .cell_y = 9};
  CHECK_FALSE(ComposeNaviTrailQuad(*gnd, *gat, outside).has_value());
}

TEST_CASE("Le chemin d'une image de flèche", "[render][navi_trail]") {
  CHECK(NaviTrailTexturePath(3, 5) ==
        "data\\texture\\유저인터페이스\\navigation_interface3\\navi_grid3_5.tga");
  CHECK(NaviTrailTexturePath(0, 0).ends_with("navi_grid1_0.tga"));
  CHECK(NaviTrailTexturePath(42, 7).ends_with("navi_grid8_7.tga"));
}
