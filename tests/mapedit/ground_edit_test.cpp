// LE RELIEF RETOUCHÉ : appliqué au sol et à la grille de marche, écrit, relu,
// fusionné ; et la brosse de l'éditeur.

#include "ro/mapedit/ground_edit.h"

#include <array>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/formats/gat.h"
#include "ro/formats/gnd.h"
#include "ro/mapedit/map_edit.h"

#include "support/map_builder.h"

using Catch::Approx;
using ro::mapedit::GroundEdit;
using ro::mapedit::MapEdit;

namespace {

constexpr std::int32_t kSide = 2;

// Un sol de 2 × 2 cubes, chacun à la hauteur de fichier `heights[rang]`, rangé
// y × largeur + x.
ro::formats::GndMap Ground(const std::array<float, 4>& heights = {}) {
  ro::test_support::GndBuildOptions options;
  options.width = kSide;
  options.height = kSide;
  for (const float height : heights) {
    options.cubes.push_back(
        ro::test_support::GndCubeSpec{.corner_height = {height, height, height, height}});
  }
  auto gnd = ro::formats::GndMap::Parse(ro::test_support::BuildGnd(options));
  REQUIRE(gnd.has_value());
  return std::move(*gnd);
}

// La grille de marche qui va avec : 4 × 4 cellules plates, toutes marchables
// sauf la première.
ro::formats::GatMap Walk() {
  constexpr std::int32_t kCells = kSide * 2;
  std::vector<ro::test_support::GatCellSpec> cells(static_cast<std::size_t>(kCells * kCells));
  cells[0].type = 1;
  auto gat = ro::formats::GatMap::Parse(ro::test_support::BuildGat(kCells, kCells, cells));
  REQUIRE(gat.has_value());
  return std::move(*gat);
}

}  // namespace

TEST_CASE("Relief : une levée monte tous les coins qui touchent son point", "[mapedit]") {
  ro::formats::GndMap gnd = Ground();
  const std::vector<GroundEdit> edits = {{.corner_x = 1, .corner_y = 1, .lift = 3.0F}};
  const ro::mapedit::MapEditReport report = ro::mapedit::ApplyGroundEdits(edits, gnd);
  CHECK(report.applied == 1);
  CHECK(report.refused.empty());
  // Le point (1, 1) est le coin haut-droit du cube (0, 0), haut-gauche de
  // (1, 0), bas-droit de (0, 1), bas-gauche de (1, 1). Le fichier compte vers
  // le bas.
  CHECK(gnd.at(0, 0).corner_height == std::array<float, 4>{0.0F, 0.0F, 0.0F, -3.0F});
  CHECK(gnd.at(1, 0).corner_height == std::array<float, 4>{0.0F, 0.0F, -3.0F, 0.0F});
  CHECK(gnd.at(0, 1).corner_height == std::array<float, 4>{0.0F, -3.0F, 0.0F, 0.0F});
  CHECK(gnd.at(1, 1).corner_height == std::array<float, 4>{-3.0F, 0.0F, 0.0F, 0.0F});
  CHECK(ro::mapedit::CornerPointHeight(gnd, 1, 1) == Approx(3.0F));
}

TEST_CASE("Relief : une falaise monte d'un bloc et garde sa marche", "[mapedit]") {
  // Le cube (1, 0) domine son voisin de dix unités : une falaise au point (1, 0).
  ro::formats::GndMap gnd = Ground({0.0F, -10.0F, 0.0F, 0.0F});
  const std::vector<GroundEdit> edits = {{.corner_x = 1, .corner_y = 0, .lift = 2.0F}};
  static_cast<void>(ro::mapedit::ApplyGroundEdits(edits, gnd));
  CHECK(gnd.at(0, 0).corner_height[1] == Approx(-2.0F));
  CHECK(gnd.at(1, 0).corner_height[0] == Approx(-12.0F));
  CHECK(gnd.at(1, 0).corner_height[0] - gnd.at(0, 0).corner_height[1] == Approx(-10.0F));
}

TEST_CASE("Relief : un point hors de la carte ne se pose pas, et se dit", "[mapedit]") {
  ro::formats::GndMap gnd = Ground();
  const std::vector<GroundEdit> edits = {{.corner_x = kSide + 1, .corner_y = 0, .lift = 1.0F},
                                         {.corner_x = 0, .corner_y = -1, .lift = 1.0F}};
  const ro::mapedit::MapEditReport report = ro::mapedit::ApplyGroundEdits(edits, gnd);
  CHECK(report.applied == 0);
  CHECK(report.refused.size() == 2);
  for (const ro::formats::GndCube& cube : gnd.cubes()) {
    CHECK(cube.corner_height == std::array<float, 4>{});
  }
  // Le dernier point de la grille, lui, est sur la carte.
  const std::vector<GroundEdit> corner = {{.corner_x = kSide, .corner_y = kSide, .lift = 1.0F}};
  CHECK(ro::mapedit::ApplyGroundEdits(corner, gnd).applied == 1);
  CHECK(gnd.at(1, 1).corner_height[3] == Approx(-1.0F));
}

TEST_CASE("Relief : la grille de marche suit, interpolée, sans changer de type", "[mapedit]") {
  ro::formats::GatMap gat = Walk();
  const std::vector<GroundEdit> edits = {{.corner_x = 1, .corner_y = 1, .lift = 4.0F}};
  ro::mapedit::LiftWalkHeights(edits, kSide, kSide, gat);
  // Le coin de cellule (2, 2) tombe sur le point (1, 1) : la levée entière.
  CHECK(gat.at(1, 1).corner_height[3] == Approx(-4.0F));
  CHECK(gat.at(2, 2).corner_height[0] == Approx(-4.0F));
  // Le coin (1, 2) tombe au milieu des points (0, 1) et (1, 1) : la moitié.
  CHECK(gat.at(0, 1).corner_height[3] == Approx(-2.0F));
  // Le coin (1, 1), au milieu de quatre points dont un seul est levé : le quart.
  CHECK(gat.at(0, 0).corner_height[3] == Approx(-1.0F));
  // Le coin (0, 0) ne voit aucun point levé.
  CHECK(gat.at(0, 0).corner_height[0] == 0.0F);
  // La marchabilité ne bouge pas.
  CHECK(gat.at(0, 0).type == 1);
  CHECK(gat.at(1, 1).type == 0);
}

TEST_CASE("Relief : une entrée par point, les levées s'additionnent", "[mapedit]") {
  MapEdit edit;
  edit.LiftGround({.corner_x = 3, .corner_y = 2, .lift = 1.5F});
  edit.LiftGround({.corner_x = 1, .corner_y = 2, .lift = 1.0F});
  edit.LiftGround({.corner_x = 3, .corner_y = 0, .lift = -2.0F});
  edit.LiftGround({.corner_x = 3, .corner_y = 2, .lift = 0.5F});
  REQUIRE(edit.ground().size() == 3);
  // Rangées par rangée, puis par colonne.
  CHECK(edit.ground()[0] == GroundEdit{.corner_x = 3, .corner_y = 0, .lift = -2.0F});
  CHECK(edit.ground()[1] == GroundEdit{.corner_x = 1, .corner_y = 2, .lift = 1.0F});
  CHECK(edit.ground()[2] == GroundEdit{.corner_x = 3, .corner_y = 2, .lift = 2.0F});
  // Une levée qui retombe à zéro quitte le document.
  edit.LiftGround({.corner_x = 3, .corner_y = 0, .lift = 2.0F});
  CHECK(edit.ground().size() == 2);
  edit.ClearGround();
  CHECK(edit.empty());
}

TEST_CASE("Relief : le document l'écrit et le relit à l'identique", "[mapedit]") {
  MapEdit edit;
  edit.LiftGround({.corner_x = 10, .corner_y = 4, .lift = 2.75F});
  edit.LiftGround({.corner_x = 0, .corner_y = 0, .lift = -0.1F});
  edit.SetGroundWhy("la dune qui bouchait le chemin");
  const std::string text = ro::mapedit::MapEditToYaml(edit, "moc_fild01");
  auto reread = ro::mapedit::ParseMapEdit(text, "moc_fild01");
  REQUIRE(reread.has_value());
  CHECK(*reread == edit);

  // Deux points au même endroit, écrits à la main, s'additionnent.
  constexpr const char* kTwice =
      "version: 1\nmap: 'moc_fild01'\nmodels: []\nground:\n  why: \"x\"\n  points:\n"
      "    - [1, 2, 1.5]\n    - [1, 2, 0.5]\n";
  auto summed = ro::mapedit::ParseMapEdit(kTwice, "moc_fild01");
  REQUIRE(summed.has_value());
  REQUIRE(summed->ground().size() == 1);
  CHECK(summed->ground()[0].lift == 2.0F);
}

TEST_CASE("Relief : un relief sans raison, ou un point mal formé, est refusé", "[mapedit]") {
  constexpr const char* kNoWhy =
      "version: 1\nmap: 'm'\nmodels: []\nground:\n  why: \"\"\n  points:\n    - [1, 2, 1]\n";
  CHECK_FALSE(ro::mapedit::ParseMapEdit(kNoWhy, "m").has_value());
  constexpr const char* kTwoFields =
      "version: 1\nmap: 'm'\nmodels: []\nground:\n  why: \"x\"\n  points:\n    - [1, 2]\n";
  CHECK_FALSE(ro::mapedit::ParseMapEdit(kTwoFields, "m").has_value());
}

TEST_CASE("Brosse : une retombée en cosinus, de 1 au centre à 0 au rayon", "[mapedit]") {
  CHECK(ro::mapedit::BrushFalloff(0.0F, 4.0F) == Approx(1.0F));
  CHECK(ro::mapedit::BrushFalloff(2.0F, 4.0F) == Approx(0.5F));
  CHECK(ro::mapedit::BrushFalloff(4.0F, 4.0F) == 0.0F);
  CHECK(ro::mapedit::BrushFalloff(5.0F, 4.0F) == 0.0F);
  CHECK(ro::mapedit::BrushFalloff(1.0F, 4.0F) > ro::mapedit::BrushFalloff(3.0F, 4.0F));
}

TEST_CASE("Brosse : lever, puis abaisser, de la force pendant la durée", "[mapedit]") {
  const ro::formats::GndMap gnd = Ground();
  const ro::mapedit::GroundBrush brush{.radius_cubes = 1.0F, .units_per_second = 2.0F};
  // Un rayon d'un cube autour du point (1, 1) : ses voisins tombent au rayon,
  // où la brosse ne pèse plus.
  const std::vector<GroundEdit> up =
      ro::mapedit::BrushStroke(gnd, brush, 1.0F, 1.0F, 0.5F, false, 0.0F);
  REQUIRE(up.size() == 1);
  CHECK(up[0] == GroundEdit{.corner_x = 1, .corner_y = 1, .lift = 1.0F});
  const std::vector<GroundEdit> down =
      ro::mapedit::BrushStroke(gnd, brush, 1.0F, 1.0F, 0.5F, true, 0.0F);
  REQUIRE(down.size() == 1);
  CHECK(down[0].lift == -1.0F);
}

TEST_CASE("Brosse : aplanir tire vers la hauteur visée, sans la dépasser", "[mapedit]") {
  // Le cube (0, 0) est cinq unités plus haut que les autres : son coin haut-
  // droit, au point (1, 1), en moyenne 1,25 au-dessus.
  const ro::formats::GndMap gnd = Ground({-5.0F, 0.0F, 0.0F, 0.0F});
  const ro::mapedit::GroundBrush flatten{.radius_cubes = 0.5F,
                                         .units_per_second = 100.0F,
                                         .mode = ro::mapedit::GroundBrushMode::kFlatten};
  const std::vector<GroundEdit> pulled =
      ro::mapedit::BrushStroke(gnd, flatten, 1.0F, 1.0F, 1.0F, false, 0.0F);
  REQUIRE(pulled.size() == 1);
  CHECK(pulled[0].lift == Approx(-1.25F));
  // Une brosse lente n'en fait qu'une part.
  const ro::mapedit::GroundBrush slow{.radius_cubes = 0.5F,
                                      .units_per_second = 1.0F,
                                      .mode = ro::mapedit::GroundBrushMode::kFlatten};
  const std::vector<GroundEdit> step =
      ro::mapedit::BrushStroke(gnd, slow, 1.0F, 1.0F, 0.5F, false, 0.0F);
  REQUIRE(step.size() == 1);
  CHECK(step[0].lift == Approx(-0.5F));
}

TEST_CASE("Brosse : adoucir tire un point vers la moyenne de ses voisins", "[mapedit]") {
  ro::formats::GndMap gnd = Ground();
  const std::vector<GroundEdit> spike = {{.corner_x = 1, .corner_y = 1, .lift = 4.0F}};
  static_cast<void>(ro::mapedit::ApplyGroundEdits(spike, gnd));
  const ro::mapedit::GroundBrush smooth{.radius_cubes = 0.5F,
                                        .units_per_second = 100.0F,
                                        .mode = ro::mapedit::GroundBrushMode::kSmooth};
  const std::vector<GroundEdit> eased =
      ro::mapedit::BrushStroke(gnd, smooth, 1.0F, 1.0F, 1.0F, false, 0.0F);
  REQUIRE(eased.size() == 1);
  // Ses quatre voisins sont à plat : la pointe redescend au sol.
  CHECK(eased[0].lift == Approx(-4.0F));
}
