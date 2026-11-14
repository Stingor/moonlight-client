#include "ro/render/terrain_relief.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/formats/gnd.h"
#include "ro/render/lightmap_atlas.h"
#include "ro/render/terrain_mesh.h"

#include "support/map_builder.h"

using ro::formats::GndMap;
using ro::render::CubeReliefOf;
using ro::render::GroundHeightAt;
using ro::render::kRoundedGroundSubdivisions;
using ro::render::kWorldUnitsPerCube;
using ro::render::ReliefHeightIn;
using ro::render::ReliefNormalIn;
using ro::render::RoundedGroundHeightAt;
using ro::test_support::BuildGnd;
using ro::test_support::GndBuildOptions;
using ro::test_support::GndCubeSpec;

namespace {

constexpr float kTolerance = 1e-4F;

bool NearlyEqual(float actual, float expected) { return std::abs(actual - expected) < kTolerance; }

// La hauteur du fichier au point (i, j) de la grille des coins.
using LatticeHeight = std::function<float(std::int32_t, std::int32_t)>;
// Le décalage de tout un cube : non nul, il le sépare de ses voisins par une
// falaise.
using CubeLift = std::function<float(std::int32_t, std::int32_t)>;

// Les cubes troués : sans dessus, ce que le mappeur laisse au vide.
using CubeHole = std::function<bool(std::int32_t, std::int32_t)>;

// Un GND dont chaque cube prend ses coins sur la grille `lattice`, plus le
// décalage de son cube. Tout cube a son dessus, sauf ceux que `hole` désigne ;
// `walls` lui donne ses deux parois.
GndMap BuildRelief(
    std::int32_t width, std::int32_t height, const LatticeHeight& lattice,
    const CubeLift& lift = [](std::int32_t, std::int32_t) { return 0.0F; }, bool walls = false,
    const CubeHole& hole = [](std::int32_t, std::int32_t) { return false; }) {
  GndBuildOptions options;
  options.width = width;
  options.height = height;
  options.texture_paths = {"sol.bmp"};
  options.tile_count = 1;
  for (std::int32_t y = 0; y < height; ++y) {
    for (std::int32_t x = 0; x < width; ++x) {
      const float offset = lift(x, y);
      options.cubes.push_back(
          GndCubeSpec{.corner_height = {lattice(x, y) + offset, lattice(x + 1, y) + offset,
                                        lattice(x, y + 1) + offset, lattice(x + 1, y + 1) + offset},
                      .tile_up = hole(x, y) ? -1 : 0,
                      .tile_front = walls ? 0 : -1,
                      .tile_side = walls ? 0 : -1});
    }
  }
  auto gnd = GndMap::Parse(BuildGnd(options));
  REQUIRE(gnd.has_value());
  return std::move(*gnd);
}

// Des dunes : une houle en diagonale, sans plat ni falaise.
float Dunes(std::int32_t i, std::int32_t j) {
  return 12.0F * std::sin(0.9F * static_cast<float>(i)) +
         7.0F * std::cos(1.3F * static_cast<float>(j) + 0.4F * static_cast<float>(i));
}

// Le point du monde d'un cube et de ses fractions.
std::array<float, 2> WorldOf(const GndMap& gnd, std::int32_t cube_x, std::int32_t cube_y,
                             float across, float along) {
  const float depth = static_cast<float>(gnd.height()) * kWorldUnitsPerCube;
  const float z_front =
      depth - static_cast<float>(cube_y) * kWorldUnitsPerCube + kWorldUnitsPerCube;
  return {(static_cast<float>(cube_x) + across) * kWorldUnitsPerCube,
          z_front - along * kWorldUnitsPerCube};
}

}  // namespace

TEST_CASE("Sol arrondi : il passe par les hauteurs de coin du fichier", "[render][relief]") {
  const GndMap gnd = BuildRelief(4, 4, Dunes);
  for (std::int32_t y = 0; y < gnd.height(); ++y) {
    for (std::int32_t x = 0; x < gnd.width(); ++x) {
      const auto relief = CubeReliefOf(gnd, x, y);
      const auto& corners = gnd.at(x, y).corner_height;
      CHECK(ReliefHeightIn(relief, 0.0F, 0.0F) == corners[0]);
      CHECK(ReliefHeightIn(relief, 1.0F, 0.0F) == corners[1]);
      CHECK(ReliefHeightIn(relief, 0.0F, 1.0F) == corners[2]);
      CHECK(ReliefHeightIn(relief, 1.0F, 1.0F) == corners[3]);
      // Dans le repère du monde, comme GroundHeightAt.
      const auto world = WorldOf(gnd, x, y, 0.0F, 0.0F);
      CHECK(NearlyEqual(RoundedGroundHeightAt(gnd, world[0], world[1]), -corners[0]));
    }
  }
}

TEST_CASE("Sol arrondi : un cube et son voisin se rejoignent, pente comprise", "[render][relief]") {
  const GndMap gnd = BuildRelief(4, 4, Dunes);
  constexpr int kSamples = 8;
  for (int sample = 0; sample <= kSamples; ++sample) {
    const float t = static_cast<float>(sample) / static_cast<float>(kSamples);
    // L'arête entre les cubes (1, 1) et (2, 1), puis celle entre (1, 1) et
    // (1, 2) : même hauteur au BIT près, sans quoi le maillage s'ouvrirait.
    const auto left = CubeReliefOf(gnd, 1, 1);
    const auto right = CubeReliefOf(gnd, 2, 1);
    const auto back = CubeReliefOf(gnd, 1, 2);
    CHECK(ReliefHeightIn(left, 1.0F, t) == ReliefHeightIn(right, 0.0F, t));
    CHECK(ReliefHeightIn(left, t, 1.0F) == ReliefHeightIn(back, t, 0.0F));
    // Et sans pli : la normale est la même des deux côtés.
    const auto normal_left = ReliefNormalIn(left, 1.0F, t);
    const auto normal_right = ReliefNormalIn(right, 0.0F, t);
    for (std::size_t axis = 0; axis < 3; ++axis) {
      CHECK(NearlyEqual(normal_left[axis], normal_right[axis]));
    }
  }
}

TEST_CASE("Sol arrondi : une dune s'arrondit entre ses coins", "[render][relief]") {
  // Une crête de dix unités, en colonnes : 0, puis 10, puis 0. L'ancien sol tire
  // une ligne droite jusqu'au sommet ; l'arrondi bombe au-dessus d'elle — vers
  // le haut du monde, donc vers les hauteurs NÉGATIVES du fichier.
  const GndMap gnd =
      BuildRelief(4, 2, [](std::int32_t i, std::int32_t) { return i == 2 ? -10.0F : 0.0F; });
  const auto relief = CubeReliefOf(gnd, 1, 0);
  // Aux trois quarts de la montée, la ligne droite est à 7,5.
  CHECK(ReliefHeightIn(relief, 0.75F, 0.5F) < -7.5F);
  const auto world = WorldOf(gnd, 1, 0, 0.75F, 0.5F);
  CHECK(RoundedGroundHeightAt(gnd, world[0], world[1]) > GroundHeightAt(gnd, world[0], world[1]));
}

TEST_CASE("Sol arrondi : une pente droite reste droite", "[render][relief]") {
  const GndMap gnd = BuildRelief(4, 4, [](std::int32_t i, std::int32_t j) {
    return 3.0F * static_cast<float>(i) - 2.0F * static_cast<float>(j);
  });
  for (const float across : {0.1F, 0.5F, 0.8F}) {
    for (const float along : {0.2F, 0.5F, 0.9F}) {
      const auto world = WorldOf(gnd, 1, 2, across, along);
      CHECK(NearlyEqual(RoundedGroundHeightAt(gnd, world[0], world[1]),
                        GroundHeightAt(gnd, world[0], world[1])));
    }
  }
}

TEST_CASE("Sol arrondi : il ne franchit pas une falaise", "[render][relief]") {
  // Un plateau plat sur les colonnes 0 et 1, une fosse cent unités plus bas à
  // partir de la colonne 2. Lissé à travers la falaise, le plateau plongerait
  // vers la fosse avant son bord, et la fosse remonterait vers lui.
  const GndMap gnd = BuildRelief(
      4, 3, [](std::int32_t, std::int32_t) { return 0.0F; },
      [](std::int32_t x, std::int32_t) { return x >= 2 ? 100.0F : 0.0F; });
  for (const float across : {0.0F, 0.3F, 0.7F, 1.0F}) {
    for (const float along : {0.0F, 0.5F, 1.0F}) {
      CHECK(ReliefHeightIn(CubeReliefOf(gnd, 1, 1), across, along) == 0.0F);
      CHECK(ReliefHeightIn(CubeReliefOf(gnd, 2, 1), across, along) == 100.0F);
    }
  }
}

TEST_CASE("Sol arrondi : une marche en pente ne déborde ni en haut ni en bas", "[render][relief]") {
  // Un socle : plat, puis trente unités en UN cube de pente continue, puis plat.
  // Sans paroi, la marche se lisse ; elle ne doit pas pour autant bomber
  // au-dessus du socle ni se creuser à son pied, ce qui couvrirait ce qui s'y
  // pose — la base d'une statue, les pieds d'un personnage.
  const GndMap gnd =
      BuildRelief(4, 2, [](std::int32_t i, std::int32_t) { return i >= 2 ? -30.0F : 0.0F; });
  for (const float across : {0.0F, 0.25F, 0.5F, 0.75F, 1.0F}) {
    for (const float along : {0.0F, 0.5F, 1.0F}) {
      CHECK(ReliefHeightIn(CubeReliefOf(gnd, 0, 0), across, along) == 0.0F);
      CHECK(ReliefHeightIn(CubeReliefOf(gnd, 2, 0), across, along) == -30.0F);
      const float step = ReliefHeightIn(CubeReliefOf(gnd, 1, 0), across, along);
      CHECK(step <= 0.0F);
      CHECK(step >= -30.0F);
    }
  }
}

TEST_CASE("Sol arrondi : le bord d'une île ne se prolonge pas dans le vide", "[render][relief]") {
  // Une rampe de cinq unités par cube, puis des cubes troués dont les hauteurs
  // plongent de soixante. Le trou touche le bord à la même hauteur : lu comme du
  // sol, il tirerait la pente du bord vers le bas, et le bord de l'île
  // s'enroulerait vers le vide.
  const GndMap gnd = BuildRelief(
      5, 2,
      [](std::int32_t i, std::int32_t) {
        return i <= 2 ? -5.0F * static_cast<float>(i) : -10.0F + 60.0F * static_cast<float>(i - 2);
      },
      [](std::int32_t, std::int32_t) { return 0.0F; }, false,
      [](std::int32_t x, std::int32_t) { return x >= 2; });
  const auto edge = CubeReliefOf(gnd, 1, 0);
  for (const float across : {0.25F, 0.5F, 0.75F}) {
    CHECK(NearlyEqual(ReliefHeightIn(edge, across, 0.5F), -5.0F * (1.0F + across)));
  }
}

TEST_CASE("Sol arrondi : au bord d'une falaise, la pente garde sa ligne", "[render][relief]") {
  // Une rampe régulière que coupe une falaise : le carreau du bord suit encore
  // la rampe, sans se courber vers le pied.
  const GndMap gnd = BuildRelief(
      5, 2, [](std::int32_t i, std::int32_t) { return 4.0F * static_cast<float>(i); },
      [](std::int32_t x, std::int32_t) { return x >= 3 ? 80.0F : 0.0F; });
  const auto edge = CubeReliefOf(gnd, 2, 0);
  for (const float across : {0.25F, 0.5F, 0.75F}) {
    CHECK(NearlyEqual(ReliefHeightIn(edge, across, 0.5F), 4.0F * (2.0F + across)));
  }
}

TEST_CASE("Sol arrondi : un acteur suit le sol dessiné, sauf au-dessus du terrain",
          "[render][relief]") {
  const GndMap gnd = BuildRelief(4, 4, Dunes);
  const auto world = WorldOf(gnd, 1, 2, 0.4F, 0.6F);
  const float flat = GroundHeightAt(gnd, world[0], world[1]);
  const float rounded = RoundedGroundHeightAt(gnd, world[0], world[1]);
  REQUIRE_FALSE(NearlyEqual(flat, rounded));
  // La grille de marche suit le sol d'origine : l'acteur prend l'arrondi.
  CHECK(NearlyEqual(ro::render::RoundedStandingHeight(gnd, flat, world[0], world[1]), rounded));
  // Elle passe vingt unités au-dessus, sur un pont : l'acteur garde le pont.
  const float bridge = flat + 20.0F;
  CHECK(ro::render::RoundedStandingHeight(gnd, bridge, world[0], world[1]) == bridge);
}

TEST_CASE("Sol arrondi : le maillage découpe chaque cube en grille", "[render][relief]") {
  const GndMap gnd = BuildRelief(3, 3, Dunes);
  const auto flat = ro::render::BuildTerrainMesh(gnd, ro::render::LightmapAtlas{});
  CHECK(flat.indices.empty());

  const auto mesh = ro::render::BuildTerrainMesh(gnd, ro::render::LightmapAtlas{}, {},
                                                 kRoundedGroundSubdivisions);
  constexpr auto kSide = static_cast<std::size_t>(kRoundedGroundSubdivisions);
  const std::size_t cubes = 9;
  CHECK(mesh.vertices.size() == cubes * (kSide + 1) * (kSide + 1));
  CHECK(mesh.indices.size() == cubes * kSide * kSide * 6);
  REQUIRE(mesh.surfaces.size() == 1);
  CHECK(mesh.surfaces[0].index_count == mesh.indices.size());
  // Chaque sommet est SUR la surface arrondie, et sa normale de forme aussi.
  for (const auto& vertex : mesh.vertices) {
    CHECK(NearlyEqual(vertex.position[1],
                      RoundedGroundHeightAt(gnd, vertex.position[0], vertex.position[2])));
    CHECK(vertex.normal == std::array<float, 3>{0.0F, 1.0F, 0.0F});
    CHECK(vertex.shape_normal[1] > 0.0F);
  }
}

TEST_CASE("Sol arrondi : une paroi touche le dessus de ses deux cubes", "[render][relief]") {
  // Une marche : les cubes de droite plus bas. La paroi doit suivre l'arête
  // arrondie de chaque côté, sommet pour sommet, sans quoi le jour passe.
  const GndMap gnd = BuildRelief(
      3, 3, Dunes, [](std::int32_t x, std::int32_t) { return x >= 1 ? 30.0F : 0.0F; }, true);
  const auto mesh = ro::render::BuildTerrainMesh(gnd, ro::render::LightmapAtlas{}, {},
                                                 kRoundedGroundSubdivisions);
  std::vector<std::array<float, 3>> tops;
  std::vector<std::array<float, 3>> walls;
  for (const auto& vertex : mesh.vertices) {
    (vertex.normal[1] > 0.0F ? tops : walls).push_back(vertex.position);
  }
  REQUIRE_FALSE(walls.empty());
  for (const auto& wall : walls) {
    bool touches = false;
    for (const auto& top : tops) {
      touches = touches || top == wall;
    }
    CHECK(touches);
  }
}
