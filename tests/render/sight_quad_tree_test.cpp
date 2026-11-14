#include "ro/render/sight_quad_tree.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "ro/formats/gat.h"
#include "ro/formats/gnd.h"
#include "ro/formats/rsw.h"

#include "support/map_builder.h"

using ro::formats::GatMap;
using ro::formats::GndMap;
using ro::formats::RswScene;
using ro::render::BoxMeetsNode;
using ro::render::RayMeetsNode;
using ro::render::SightBox;
using ro::render::SightQuadTree;
using ro::test_support::BuildGat;
using ro::test_support::BuildGnd;
using ro::test_support::BuildRsw;
using ro::test_support::GatCellSpec;
using ro::test_support::GndBuildOptions;
using ro::test_support::GndCubeSpec;
using ro::test_support::RswBuildOptions;

namespace {

constexpr float kUnbounded = 999999.0F;
// Les nœuds sous un enfant de la racine : 1 + 4 + 16 + 64 + 256.
constexpr std::uint32_t kSubtree = 341;
// Une carte de 64 cubes : des feuilles de deux cubes de côté.
constexpr std::int32_t kWideMap = 64;

[[nodiscard]] GndMap Ground(const GndBuildOptions& options) {
  auto gnd = GndMap::Parse(BuildGnd(options));
  REQUIRE(gnd.has_value());
  return std::move(*gnd);
}

[[nodiscard]] GndMap FlatGround(std::int32_t width, std::int32_t height) {
  GndBuildOptions options;
  options.width = width;
  options.height = height;
  return Ground(options);
}

[[nodiscard]] RswScene Scene(const RswBuildOptions& options = RswBuildOptions()) {
  auto rsw = RswScene::Parse(BuildRsw(options));
  REQUIRE(rsw.has_value());
  return std::move(*rsw);
}

[[nodiscard]] SightBox Cube(const glm::vec3& center, float half) {
  SightBox box;
  box.center = center;
  box.half_extents = {half, half, half};
  box.axes = {glm::vec3(1.0F, 0.0F, 0.0F), glm::vec3(0.0F, 1.0F, 0.0F),
              glm::vec3(0.0F, 0.0F, 1.0F)};
  box.lowest = center - glm::vec3(half);
  box.highest = center + glm::vec3(half);
  return box;
}

[[nodiscard]] std::size_t LeavesHolding(const SightQuadTree& tree, std::uint32_t item) {
  return static_cast<std::size_t>(std::ranges::count_if(tree.nodes(), [item](const auto& node) {
    return node.leaf && std::ranges::find(node.items, item) != node.items.end();
  }));
}

// Une droite qui descend en biais vers ce point : ni X ni Z ne lui sont
// parallèles, si bien que la part des sept dixièmes n'intervient pas.
[[nodiscard]] std::vector<std::uint32_t> AlongSlantTo(const SightQuadTree& tree,
                                                      const glm::vec3& target) {
  const glm::vec3 direction = glm::normalize(glm::vec3(0.3F, 1.0F, 0.2F));
  std::vector<std::uint32_t> found;
  tree.CollectAlong(target - direction * 500.0F, direction, found);
  return found;
}

}  // namespace

TEST_CASE("Quadtree : 1 365 nœuds en profondeur d'abord, la racine sur le sol", "[render][veil]") {
  const SightQuadTree tree = SightQuadTree::Build(Scene(), FlatGround(4, 6), nullptr, {});
  REQUIRE(tree.nodes().size() == ro::formats::kRswQuadTreeNodeCount);
  const SightQuadTree::Node& root = tree.nodes().front();
  // Le client compte la moitié par division entière, et s'arrête une unité
  // avant le bord haut.
  CHECK(root.lowest == glm::vec3(-20.0F, -kUnbounded, -30.0F));
  CHECK(root.highest == glm::vec3(19.0F, kUnbounded, 29.0F));
  CHECK(root.children ==
        std::array<std::uint32_t, 4>{1, 1 + kSubtree, 1 + 2 * kSubtree, 1 + 3 * kSubtree});
  // Le premier enfant prend la moitié basse en X et en Z ; le deuxième la
  // moitié haute en X.
  CHECK(tree.nodes()[1].lowest.x == -20.0F);
  CHECK(tree.nodes()[1].highest.x == -0.5F);
  CHECK(tree.nodes()[1].highest.z == -0.5F);
  CHECK(tree.nodes()[1 + kSubtree].lowest.x == -0.5F);
  CHECK(tree.nodes()[1 + kSubtree].highest.x == 19.0F);
  CHECK(tree.nodes()[1 + 2 * kSubtree].lowest.z == -0.5F);
  CHECK_FALSE(tree.nodes()[4].leaf);
  CHECK(tree.nodes()[5].leaf);
}

TEST_CASE("Quadtree : parallèle, la droite ne compte que sept dixièmes du nœud", "[render][veil]") {
  const glm::vec3 lowest(-1.0F);
  const glm::vec3 highest(1.0F);
  const glm::vec3 down{0.0F, 1.0F, 0.0F};
  CHECK(RayMeetsNode(glm::vec3(0.6F, -5.0F, 0.0F), down, lowest, highest));
  // Dans le nœud, mais hors de sa part centrale : manqué.
  CHECK_FALSE(RayMeetsNode(glm::vec3(0.8F, -5.0F, 0.0F), down, lowest, highest));
  // À peine penchée, la même droite n'est plus parallèle, et le trouve.
  CHECK(RayMeetsNode(glm::vec3(0.8F, -5.0F, 0.0F), glm::normalize(glm::vec3(0.0001F, 1.0F, 0.0F)),
                     lowest, highest));
}

TEST_CASE("Quadtree : une boîte orientée contre un nœud", "[render][veil]") {
  const glm::vec3 lowest(0.0F);
  const glm::vec3 highest(1.0F);
  const SightBox square = Cube(glm::vec3(1.6F, 0.5F, 0.5F), 0.5F);
  CHECK_FALSE(BoxMeetsNode(square, lowest, highest));
  // Tournée d'un huitième de tour, la même boîte y plonge un coin.
  SightBox turned = square;
  const glm::mat4 turn = glm::rotate(glm::mat4(1.0F), glm::radians(45.0F), glm::vec3(0, 1, 0));
  for (glm::vec3& axis : turned.axes) {
    axis = glm::vec3(turn * glm::vec4(axis, 0.0F));
  }
  CHECK(BoxMeetsNode(turned, lowest, highest));
  CHECK(BoxMeetsNode(Cube(glm::vec3(0.5F), 0.1F), lowest, highest));
}

TEST_CASE("Quadtree : une pose se range dans sa feuille, et la droite l'y trouve",
          "[render][veil]") {
  const glm::vec3 center{-150.2F, -10.0F, 70.0F};
  const std::array<SightBox, 1> boxes = {Cube(center, 2.0F)};
  const SightQuadTree tree =
      SightQuadTree::Build(Scene(), FlatGround(kWideMap, kWideMap), nullptr, boxes);
  CHECK(LeavesHolding(tree, 0) == 1);

  const std::vector<std::uint32_t> found = AlongSlantTo(tree, center);
  REQUIRE_FALSE(found.empty());
  CHECK(std::ranges::all_of(found, [](std::uint32_t item) { return item == 0; }));
  CHECK(AlongSlantTo(tree, center + glm::vec3(200.0F, 0.0F, 0.0F)).empty());
}

TEST_CASE("Quadtree : les bornes du fichier remplacent celles qui ont rangé", "[render][veil]") {
  // Le fichier porte des bornes loin de tout : la pose se range quand même,
  // d'après le découpage calculé, mais la droite ne trouve plus sa feuille.
  RswBuildOptions options;
  options.quad_tree.assign(ro::formats::kRswQuadTreeNodeCount,
                           {1001.0F, 1001.0F, 1001.0F, 1000.0F, 1000.0F, 1000.0F, 0.5F, 0.5F, 0.5F,
                            1000.5F, 1000.5F, 1000.5F});
  const glm::vec3 center{-150.2F, -10.0F, 70.0F};
  const std::array<SightBox, 1> boxes = {Cube(center, 2.0F)};
  const SightQuadTree tree =
      SightQuadTree::Build(Scene(options), FlatGround(kWideMap, kWideMap), nullptr, boxes);

  CHECK(tree.nodes()[7].lowest == glm::vec3(1000.0F));
  CHECK(tree.nodes()[7].highest == glm::vec3(1001.0F));
  CHECK(LeavesHolding(tree, 0) == 1);
  CHECK(AlongSlantTo(tree, center).empty());
}

TEST_CASE("Quadtree : sans bornes au fichier, une feuille prend la hauteur de son sol",
          "[render][veil]") {
  // La première feuille couvre les cubes 0 et 1 dans les deux sens. L'un d'eux
  // monte à -30 — l'ordonnée du client descend.
  GndBuildOptions options;
  options.width = kWideMap;
  options.height = kWideMap;
  options.tile_count = 1;
  options.cubes.assign(static_cast<std::size_t>(kWideMap * kWideMap),
                       GndCubeSpec{.corner_height = {0.0F, 0.0F, 0.0F, 0.0F}, .tile_up = 0});
  options.cubes[static_cast<std::size_t>(kWideMap + 1)].corner_height = {-30.0F, -30.0F, -30.0F,
                                                                         -30.0F};
  const std::array<SightBox, 1> boxes = {Cube(glm::vec3(-315.0F, -80.0F, -315.0F), 2.0F)};
  const SightQuadTree tree = SightQuadTree::Build(Scene(), Ground(options), nullptr, boxes);

  const SightQuadTree::Node& first_leaf = tree.nodes()[5];
  REQUIRE(first_leaf.leaf);
  // Le sol de la feuille, puis la pose qui s'y tient, jusqu'à -82.
  CHECK(first_leaf.lowest.y == -82.0F);
  CHECK(first_leaf.highest.y == 0.0F);
  CHECK(tree.nodes().back().lowest.y == 0.0F);
  CHECK(tree.nodes().back().highest.y == 0.0F);
  // Les nœuds au-dessus ne se relèvent jamais.
  CHECK(tree.nodes()[4].lowest.y == -kUnbounded);
  CHECK(tree.nodes()[4].highest.y == kUnbounded);
}

TEST_CASE("Quadtree : une feuille plus petite qu'un cube n'a pas de sol, sauf la marche",
          "[render][veil]") {
  // La plage de cubes ne s'étend pas : le client rend zéro et zéro. Le relief
  // de marche, lui, se lit case par case.
  const SightQuadTree bare = SightQuadTree::Build(Scene(), FlatGround(4, 6), nullptr, {});
  CHECK(bare.nodes()[5].lowest.y == 0.0F);
  CHECK(bare.nodes()[5].highest.y == 0.0F);

  std::vector<GatCellSpec> cells(static_cast<std::size_t>(8 * 12));
  cells.front().corner_height = {-50.0F, -40.0F, -40.0F, -40.0F};
  auto gat = GatMap::Parse(BuildGat(8, 12, cells));
  REQUIRE(gat.has_value());
  const SightQuadTree walked = SightQuadTree::Build(Scene(), FlatGround(4, 6), &*gat, {});
  CHECK(walked.nodes()[5].lowest.y == -50.0F);
  CHECK(walked.nodes()[5].highest.y == 0.0F);
}
