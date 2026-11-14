#include <cmath>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/formats/gat.h"
#include "ro/formats/gnd.h"
#include "ro/render/lightmap_atlas.h"
#include "ro/render/terrain_mesh.h"

#include "support/map_builder.h"

using ro::formats::GatMap;
using ro::formats::GndMap;
using ro::render::BuildTerrainMesh;
using ro::render::CellCenterPosition;
using ro::render::GroundHeightAt;
using ro::render::kWorldUnitsPerCube;
using ro::render::WalkableHeightAt;
using ro::test_support::BuildGat;
using ro::test_support::BuildGnd;
using ro::test_support::GatCellSpec;
using ro::test_support::GndBuildOptions;
using ro::test_support::GndCubeSpec;

namespace {

bool NearlyEqual(float actual, float expected) { return std::abs(actual - expected) < 1e-3F; }

// Un GND de deux cubes sur deux, dont chaque cube porte une hauteur uniforme
// mais DIFFÉRENTE de ses voisins : toute confusion de rangée ou de colonne se
// voit sur la valeur rendue.
GndMap DistinctCubes() {
  GndBuildOptions options;
  options.width = 2;
  options.height = 2;
  options.texture_paths = {"sol.bmp"};
  options.tile_count = 1;
  // Rangée y = 0 puis y = 1, colonnes x = 0 puis x = 1.
  options.cubes = {GndCubeSpec{.corner_height = {10.0F, 10.0F, 10.0F, 10.0F}, .tile_up = 0},
                   GndCubeSpec{.corner_height = {20.0F, 20.0F, 20.0F, 20.0F}, .tile_up = 0},
                   GndCubeSpec{.corner_height = {30.0F, 30.0F, 30.0F, 30.0F}, .tile_up = 0},
                   GndCubeSpec{.corner_height = {40.0F, 40.0F, 40.0F, 40.0F}, .tile_up = 0}};

  auto gnd = GndMap::Parse(BuildGnd(options));
  REQUIRE(gnd.has_value());
  return std::move(*gnd);
}

}  // namespace

TEST_CASE("Sol : la hauteur est l'opposée de celle du fichier", "[render][ground]") {
  // L'axe vertical du monde monte, celui du fichier descend. Sans l'inversion,
  // un personnage s'enfoncerait d'autant qu'il devrait s'élever.
  const GndMap gnd = DistinctCubes();
  // Centre du cube (0, 0) : sa hauteur de fichier vaut 10.
  const float depth = 2.0F * kWorldUnitsPerCube;
  const float height = GroundHeightAt(gnd, 5.0F, depth + kWorldUnitsPerCube * 0.5F);
  CHECK(NearlyEqual(height, -10.0F));
}

TEST_CASE("Sol : chaque cube rend SA hauteur", "[render][ground]") {
  // Le test qui distingue une inversion de rangée d'une inversion de colonne :
  // les quatre cubes portent quatre hauteurs différentes.
  const GndMap gnd = DistinctCubes();
  const float depth = 2.0F * kWorldUnitsPerCube;
  const float near_row = depth + kWorldUnitsPerCube * 0.5F;
  const float far_row = depth - kWorldUnitsPerCube * 0.5F;

  CHECK(NearlyEqual(GroundHeightAt(gnd, 5.0F, near_row), -10.0F));
  CHECK(NearlyEqual(GroundHeightAt(gnd, 15.0F, near_row), -20.0F));
  CHECK(NearlyEqual(GroundHeightAt(gnd, 5.0F, far_row), -30.0F));
  CHECK(NearlyEqual(GroundHeightAt(gnd, 15.0F, far_row), -40.0F));
}

TEST_CASE("Sol : la hauteur est INTERPOLÉE dans un cube en pente", "[render][ground]") {
  // Retenir une seule hauteur de coin ferait sursauter un personnage d'un cube
  // à l'autre au lieu de le faire glisser sur la pente.
  GndBuildOptions options;
  options.width = 1;
  options.height = 1;
  options.texture_paths = {"sol.bmp"};
  options.tile_count = 1;
  // Coins 0 et 2 à gauche valent 0, coins 1 et 3 à droite valent 100.
  options.cubes = {GndCubeSpec{.corner_height = {0.0F, 100.0F, 0.0F, 100.0F}, .tile_up = 0}};
  auto gnd = GndMap::Parse(BuildGnd(options));
  REQUIRE(gnd.has_value());

  const float z = kWorldUnitsPerCube + kWorldUnitsPerCube * 0.5F;
  CHECK(NearlyEqual(GroundHeightAt(*gnd, 0.0F, z), 0.0F));
  CHECK(NearlyEqual(GroundHeightAt(*gnd, kWorldUnitsPerCube * 0.5F, z), -50.0F));
  CHECK(NearlyEqual(GroundHeightAt(*gnd, kWorldUnitsPerCube, z), -100.0F));
}

TEST_CASE("Sol : un point hors carte retombe sur le bord", "[render][ground]") {
  // Un acteur mal placé doit se poser sur le terrain le plus proche plutôt que
  // de lire hors des bornes.
  const GndMap gnd = DistinctCubes();
  CHECK(std::isfinite(GroundHeightAt(gnd, -1000.0F, -1000.0F)));
  CHECK(std::isfinite(GroundHeightAt(gnd, 1000.0F, 1000.0F)));
}

TEST_CASE("Sol : la hauteur coïncide avec celle du MAILLAGE", "[render][ground]") {
  // Les deux fonctions partagent le placement des cubes et l'inversion de
  // l'axe vertical ; si elles divergent, le personnage flotte ou s'enfonce.
  //
  // Le terrain d'essai est CONTINU — les cubes voisins s'accordent sur leurs
  // coins communs. Sur un terrain en marches, un sommet posé sur une arête
  // partagée appartient à deux cubes de hauteurs différentes, et l'écart
  // mesuré n'apprendrait rien sur l'accord des deux fonctions.
  //
  // Les quatre coins d'un cube sont les nœuds (x, y), (x+1, y), (x, y+1) et
  // (x+1, y+1) : cette correspondance-là est aussi ce que le test verrouille.
  // Une pente DOUCE : à dix unités de dénivelé par cube de dix de côté, les
  // faces du dessus pointeraient presque à l'horizontale et le filtre par
  // normale ne trierait plus rien.
  const auto node_height = [](std::int32_t i, std::int32_t j) {
    return static_cast<float>(i) * 1.0F + static_cast<float>(j) * 2.0F;
  };

  GndBuildOptions options;
  options.width = 2;
  options.height = 2;
  options.texture_paths = {"sol.bmp"};
  options.tile_count = 1;
  for (std::int32_t y = 0; y < options.height; ++y) {
    for (std::int32_t x = 0; x < options.width; ++x) {
      options.cubes.push_back(
          GndCubeSpec{.corner_height = {node_height(x, y), node_height(x + 1, y),
                                        node_height(x, y + 1), node_height(x + 1, y + 1)},
                      .tile_up = 0});
    }
  }
  auto gnd = GndMap::Parse(BuildGnd(options));
  REQUIRE(gnd.has_value());

  const ro::render::TerrainMesh mesh = BuildTerrainMesh(*gnd, ro::render::LightmapAtlas{});
  REQUIRE_FALSE(mesh.vertices.empty());

  std::size_t checked = 0;
  for (const ro::render::TerrainVertex& vertex : mesh.vertices) {
    // Seules les faces du DESSUS sont à l'altitude du sol en leur point ; les
    // faces latérales, elles, relient deux altitudes et ont une normale
    // horizontale.
    if (vertex.normal[1] > 0.5F) {
      const float sampled = GroundHeightAt(*gnd, vertex.position[0], vertex.position[2]);
      CHECK(NearlyEqual(sampled, vertex.position[1]));
      ++checked;
    }
  }
  CHECK(checked > 0);
}

TEST_CASE("Cellule : le point rendu est le CENTRE de la cellule", "[render][ground]") {
  // Le serveur ne parle qu'en cellules ; c'est cette conversion que le réseau
  // empruntera. Une cellule vaut la moitié d'un cube.
  const GndMap gnd = DistinctCubes();
  const std::array<float, 3> position = CellCenterPosition(gnd, 0, 0);
  // Cellule 0 : de 0 à 5 unités, donc centre à 2,5.
  CHECK(NearlyEqual(position[0], 2.5F));
  // Et l'altitude est celle du sol en ce point.
  CHECK(NearlyEqual(position[1], GroundHeightAt(gnd, position[0], position[2])));
}

TEST_CASE("Praticable : l'altitude d'un acteur vient de la GRILLE, pas du décor",
          "[render][ground]") {
  // Le GND porte le sol que l'on VOIT, le GAT celui sur lequel on MARCHE, et
  // les deux diffèrent là où la carte fait marcher AU-DESSUS du terrain : un
  // pont, une arche, une plateforme. Relevé sur gon_fild01, à la cellule
  // (189, 260), le GAT donne −172 là où le GND donne −100 : un acteur posé sur
  // le second flotte soixante-douze unités sous ses pieds.
  const std::vector<GatCellSpec> cells = {
      GatCellSpec{.corner_height = {10.0F, 10.0F, 10.0F, 10.0F}},
      GatCellSpec{.corner_height = {20.0F, 20.0F, 20.0F, 20.0F}},
      GatCellSpec{.corner_height = {30.0F, 30.0F, 30.0F, 30.0F}},
      GatCellSpec{.corner_height = {40.0F, 40.0F, 40.0F, 40.0F}}};
  auto gat = GatMap::Parse(BuildGat(2, 2, cells));
  REQUIRE(gat.has_value());

  // Chaque cellule rend SA hauteur, opposée à celle du fichier. Un numéro
  // entier désigne le CENTRE de sa cellule.
  CHECK(NearlyEqual(WalkableHeightAt(*gat, 0.0F, 0.0F), -10.0F));
  CHECK(NearlyEqual(WalkableHeightAt(*gat, 1.0F, 0.0F), -20.0F));
  CHECK(NearlyEqual(WalkableHeightAt(*gat, 0.0F, 1.0F), -30.0F));
  CHECK(NearlyEqual(WalkableHeightAt(*gat, 1.0F, 1.0F), -40.0F));
}

TEST_CASE("Praticable : la hauteur est INTERPOLÉE dans une cellule en pente", "[render][ground]") {
  // Sans interpolation, un acteur qui traverse une cellule sauterait d'un coup
  // à son bord.
  const std::vector<GatCellSpec> cells = {
      GatCellSpec{.corner_height = {0.0F, 100.0F, 0.0F, 100.0F}}};
  auto gat = GatMap::Parse(BuildGat(1, 1, cells));
  REQUIRE(gat.has_value());

  // Le bord ouest de la cellule est à un demi AVANT son numéro, le bord est à
  // un demi APRÈS ; son centre est sur son numéro.
  CHECK(NearlyEqual(WalkableHeightAt(*gat, -0.5F, 0.0F), 0.0F));
  CHECK(NearlyEqual(WalkableHeightAt(*gat, 0.0F, 0.0F), -50.0F));
  CHECK(NearlyEqual(WalkableHeightAt(*gat, 0.5F, 0.0F), -100.0F));

  // Hors de la grille, on retombe sur le bord plutôt que de lire ailleurs.
  CHECK(NearlyEqual(WalkableHeightAt(*gat, -5.0F, 0.0F), 0.0F));
  CHECK(NearlyEqual(WalkableHeightAt(*gat, 9.0F, 0.0F), -100.0F));
}

TEST_CASE("Praticable : les coins montent vers le NORD comme dans le fichier", "[render][ground]") {
  // Les coins 0 et 1 bordent la cellule au sud, les coins 2 et 3 au nord.
  // Les intervertir ferait descendre les pentes à l'envers.
  const std::vector<GatCellSpec> cells = {GatCellSpec{.corner_height = {0.0F, 0.0F, 60.0F, 60.0F}}};
  auto gat = GatMap::Parse(BuildGat(1, 1, cells));
  REQUIRE(gat.has_value());

  CHECK(NearlyEqual(WalkableHeightAt(*gat, 0.0F, -0.5F), 0.0F));
  CHECK(NearlyEqual(WalkableHeightAt(*gat, 0.0F, 0.5F), -60.0F));
}

TEST_CASE("Cellule : la cellule d'un point du monde est l'inverse de sa position",
          "[render][ground]") {
  // Les deux conversions vivent côte à côte et doivent se défaire l'une
  // l'autre : sinon le clic vise une case et le personnage se dessine dans une
  // autre, sans que rien ne le dise.
  const GndMap gnd = DistinctCubes();
  for (const std::array<float, 2> cell :
       {std::array<float, 2>{0.0F, 0.0F}, std::array<float, 2>{3.0F, 2.0F},
        std::array<float, 2>{1.5F, 0.25F}}) {
    const std::array<float, 3> world = ro::render::CellPositionAt(gnd, cell[0], cell[1]);
    const std::array<float, 2> back = ro::render::FractionalCellAt(gnd, world[0], world[2]);
    CHECK(NearlyEqual(back[0], cell[0]));
    CHECK(NearlyEqual(back[1], cell[1]));
  }
}
