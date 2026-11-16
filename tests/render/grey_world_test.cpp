#include "ro/render/grey_world.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/formats/gat.h"
#include "ro/formats/gnd.h"
#include "ro/formats/rsw.h"
#include "ro/mapedit/blocked_cells.h"
#include "ro/mapedit/map_edit.h"
#include "ro/render/ground_quad.h"
#include "ro/render/skill_area.h"
#include "ro/render/terrain_mesh.h"

#include "support/map_builder.h"

using ro::formats::GatMap;
using ro::formats::GndMap;
using ro::formats::RswScene;
using ro::mapedit::CellBlock;
using ro::mapedit::MapEdit;
using ro::render::CellFamily;
using ro::render::CellFamilyOf;
using ro::render::ComposeGreyWorldCells;
using ro::render::ComposeGreyWorldTile;
using ro::render::FlatReliefLevel;
using ro::render::GreyWorldCellFill;
using ro::render::GreyWorldFill;
using ro::render::GreyWorldFlattens;
using ro::render::GreyWorldGroundColor;
using ro::render::GreyWorldHidesLightmap;
using ro::render::GreyWorldHidesMapEffects;
using ro::render::GreyWorldHidesModels;
using ro::render::GreyWorldIsPainted;
using ro::render::GreyWorldLook;
using ro::render::GreyWorldPaintsGround;
using ro::render::GreyWorldShowsGrid;
using ro::render::GreyWorldTexturePath;
using ro::render::GroundHasSurface;
using ro::render::GroundQuad;
using ro::render::kWorldUnitsPerCell;
using ro::render::ReseatMapEffects;
using ro::render::SkillAreaPattern;
using ro::render::SkillAreaTexturePath;
using ro::test_support::BuildGat;
using ro::test_support::BuildGnd;
using ro::test_support::BuildRsw;
using ro::test_support::GatCellSpec;
using ro::test_support::GndBuildOptions;
using ro::test_support::GndCubeSpec;
using ro::test_support::RswBuildOptions;

namespace {

// Quatre cubes sur quatre, donc huit cellules sur huit : c'est le rapport du
// jeu. Le cube (3, 3) n'a PAS de sol — un coin de vide, comme au bord d'une île.
constexpr std::int32_t kCubesPerSide = 4;
constexpr std::int32_t kCellsPerSide = 8;
constexpr float kGroundHeight = 4.0F;
// Les hauteurs du vide plongent loin sous le sol : elles ne doivent pas tirer
// le niveau vers elles.
constexpr float kVoidHeight = 400.0F;
constexpr std::int32_t kWallType = 1;
constexpr std::int32_t kSnipeType = 5;

GndMap FourByFourGnd() {
  GndBuildOptions options;
  options.width = kCubesPerSide;
  options.height = kCubesPerSide;
  options.texture_paths = {"sol.bmp"};
  options.tile_count = 1;
  options.cubes.assign(
      kCubesPerSide * kCubesPerSide,
      GndCubeSpec{.corner_height = {kGroundHeight, kGroundHeight, kGroundHeight, kGroundHeight},
                  .tile_up = 0});
  options.cubes.back() = GndCubeSpec{
      .corner_height = {kVoidHeight, kVoidHeight, kVoidHeight, kVoidHeight}, .tile_up = -1};
  auto gnd = GndMap::Parse(BuildGnd(options));
  REQUIRE(gnd.has_value());
  return std::move(*gnd);
}

// Tout marchable, sauf un mur en (2, 2) et une falaise tirable en (5, 2).
GatMap FlatGat() {
  std::vector<GatCellSpec> cells(
      kCellsPerSide * kCellsPerSide,
      GatCellSpec{.corner_height = {kGroundHeight, kGroundHeight, kGroundHeight, kGroundHeight},
                  .type = 0});
  cells[2 * kCellsPerSide + 2].type = kWallType;
  cells[2 * kCellsPerSide + 5].type = kSnipeType;
  auto gat = GatMap::Parse(BuildGat(kCellsPerSide, kCellsPerSide, cells));
  REQUIRE(gat.has_value());
  return std::move(*gat);
}

// Un sol qui MONTE vers l'est, cube par cube, sans changer d'une rangée à
// l'autre : une confusion des axes du repère s'y lit sur la hauteur rendue.
// `surface` dit si les cubes ont une face du dessus.
GndMap EastwardSlopeGnd(bool surface) {
  GndBuildOptions options;
  options.width = kCubesPerSide;
  options.height = kCubesPerSide;
  options.texture_paths = {"sol.bmp"};
  options.tile_count = 1;
  for (std::int32_t row = 0; row < kCubesPerSide; ++row) {
    for (std::int32_t column = 0; column < kCubesPerSide; ++column) {
      const auto west = static_cast<float>(column);
      options.cubes.push_back(GndCubeSpec{.corner_height = {west, west + 1.0F, west, west + 1.0F},
                                          .tile_up = surface ? 0 : -1});
    }
  }
  auto gnd = GndMap::Parse(BuildGnd(options));
  REQUIRE(gnd.has_value());
  return std::move(*gnd);
}

RswScene OneEffectAtOrigin() {
  RswBuildOptions options;
  options.effect_object_count = 1;
  auto rsw = RswScene::Parse(BuildRsw(options));
  REQUIRE(rsw.has_value());
  REQUIRE(rsw->effects().size() == 1);
  return std::move(*rsw);
}

GreyWorldLook EnabledLook() {
  GreyWorldLook look;
  look.enabled = true;
  return look;
}

// Le centre d'un quad, pour savoir de quelle cellule il vient.
[[nodiscard]] std::array<float, 2> CenterOf(const GroundQuad& quad) {
  float x = 0.0F;
  float z = 0.0F;
  for (const glm::vec3& corner : quad.corners) {
    x += corner.x;
    z += corner.z;
  }
  constexpr float kCorners = 4.0F;
  return {x / kCorners, z / kCorners};
}

[[nodiscard]] bool Paints(const GndMap& gnd, const std::vector<GroundQuad>& quads,
                          std::int32_t cell_x, std::int32_t cell_y) {
  const std::array<float, 3> center =
      ro::render::CellPositionAt(gnd, static_cast<float>(cell_x), static_cast<float>(cell_y));
  return std::ranges::any_of(quads, [&center](const GroundQuad& quad) {
    const std::array<float, 2> drawn = CenterOf(quad);
    return drawn[0] == Catch::Approx(center[0]) && drawn[1] == Catch::Approx(center[2]);
  });
}

}  // namespace

TEST_CASE("GreyWorld : trois familles, le drapeau d'eau n'y change rien", "[render][greyworld]") {
  CHECK(CellFamilyOf(0) == CellFamily::kWalkable);
  CHECK(CellFamilyOf(1) == CellFamily::kWall);
  CHECK(CellFamilyOf(5) == CellFamily::kSnipeable);
  CHECK(CellFamilyOf(3) == CellFamily::kWalkable);
  // Le drapeau sur un mur et sur une falaise : ils le restent.
  CHECK(CellFamilyOf(static_cast<std::int32_t>(0x80000001U)) == CellFamily::kWall);
  CHECK(CellFamilyOf(static_cast<std::int32_t>(0x80000005U)) == CellFamily::kSnipeable);
  CHECK(CellFamilyOf(static_cast<std::int32_t>(0x80000000U)) == CellFamily::kWalkable);
}

TEST_CASE("GreyWorld : l'interrupteur commun tient les quatre leviers", "[render][greyworld]") {
  GreyWorldLook look;
  look.flatten = true;
  CHECK_FALSE(GreyWorldHidesModels(look));
  CHECK_FALSE(GreyWorldShowsGrid(look));
  CHECK_FALSE(GreyWorldPaintsGround(look));
  CHECK_FALSE(GreyWorldFlattens(look));

  look.enabled = true;
  CHECK(GreyWorldShowsGrid(look));
  CHECK(GreyWorldPaintsGround(look));
  CHECK(GreyWorldFlattens(look));
  // APLATIR EMPORTE LES DÉCORS, même quand le joueur les a laissés visibles.
  look.hide_models = false;
  CHECK(GreyWorldHidesModels(look));
  look.flatten = false;
  CHECK_FALSE(GreyWorldHidesModels(look));
}

TEST_CASE("GreyWorld : la lightmap ne part qu'avec les décors", "[render][greyworld]") {
  GreyWorldLook look;
  CHECK_FALSE(GreyWorldHidesLightmap(look));
  look.enabled = true;
  CHECK(GreyWorldHidesLightmap(look));
  // Décors visibles : la case esclave ne vaut plus rien.
  look.hide_models = false;
  CHECK_FALSE(GreyWorldHidesLightmap(look));
  // Aplatir emporte les décors, donc la lightmap avec eux.
  look.flatten = true;
  CHECK(GreyWorldHidesLightmap(look));
  look.hide_lightmap = false;
  CHECK_FALSE(GreyWorldHidesLightmap(look));
}

TEST_CASE("GreyWorld : les effets de la carte ont leur propre levier", "[render][greyworld]") {
  GreyWorldLook look;
  CHECK_FALSE(GreyWorldHidesMapEffects(look));
  look.enabled = true;
  CHECK(GreyWorldHidesMapEffects(look));
  // Indépendant des décors : on peut garder les uns sans les autres.
  look.hide_models = false;
  look.flatten = false;
  CHECK(GreyWorldHidesMapEffects(look));
  look.hide_map_effects = false;
  look.hide_models = true;
  CHECK_FALSE(GreyWorldHidesMapEffects(look));
  CHECK(GreyWorldHidesModels(look));
}

TEST_CASE("GreyWorld : le sol uni par défaut est une ardoise", "[render][greyworld]") {
  const std::array<float, 3> color = GreyWorldGroundColor(GreyWorldLook());
  CHECK(color[0] == Catch::Approx(42.0F / 255.0F));
  CHECK(color[1] == Catch::Approx(42.0F / 255.0F));
  CHECK(color[2] == Catch::Approx(46.0F / 255.0F));
}

TEST_CASE("GreyWorld : le carreau plein est un texel blanc opaque", "[render][greyworld]") {
  const ro::formats::Image tile = ComposeGreyWorldTile();
  CHECK(tile.width == 1);
  CHECK(tile.height == 1);
  CHECK(tile.pixels == std::vector<std::uint8_t>{255, 255, 255, 255});
  CHECK(GreyWorldIsPainted(GreyWorldTexturePath(SkillAreaPattern::kSolid)));
  // Les deux autres sont ceux de la zone des sorts, lus dans l'archive.
  CHECK(GreyWorldTexturePath(SkillAreaPattern::kFrame) ==
        SkillAreaTexturePath(SkillAreaPattern::kFrame));
  CHECK_FALSE(GreyWorldIsPainted(GreyWorldTexturePath(SkillAreaPattern::kCorners)));
  // Un rang inconnu, écrit à la main dans le fichier : le carreau plein.
  CHECK(GreyWorldIsPainted(GreyWorldTexturePath(static_cast<SkillAreaPattern>(7))));
}

TEST_CASE("GreyWorld : le joint ne creuse que le carreau plein", "[render][greyworld]") {
  GreyWorldLook look;
  look.gap_percent = 12.0F;
  CHECK(GreyWorldCellFill(look) == Catch::Approx(0.88F));
  look.gap_percent = 90.0F;
  CHECK(GreyWorldCellFill(look) == Catch::Approx(0.60F));
  look.pattern = SkillAreaPattern::kFrame;
  CHECK(GreyWorldCellFill(look) == Catch::Approx(1.0F));
}

TEST_CASE("GreyWorld : le sol existe là où le cube a une face du dessus", "[render][greyworld]") {
  const GndMap gnd = FourByFourGnd();
  CHECK(GroundHasSurface(gnd, 0, 0));
  CHECK(GroundHasSurface(gnd, 5, 5));
  // Le cube (3, 3) couvre les cellules 6 et 7 sur chaque axe.
  CHECK_FALSE(GroundHasSurface(gnd, 6, 6));
  CHECK_FALSE(GroundHasSurface(gnd, 7, 6));
  CHECK_FALSE(GroundHasSurface(gnd, -1, 0));
  CHECK_FALSE(GroundHasSurface(gnd, kCellsPerSide, 0));
}

TEST_CASE("GreyWorld : toutes les cases avec du sol, une par cellule", "[render][greyworld]") {
  const GndMap gnd = FourByFourGnd();
  const GatMap gat = FlatGat();
  std::vector<GroundQuad> quads;
  ComposeGreyWorldCells(gnd, gat, {}, 3, 3, EnabledLook(), quads);
  // Soixante-quatre cellules, moins les quatre du cube sans sol.
  CHECK(quads.size() == 60);
  CHECK_FALSE(Paints(gnd, quads, 6, 6));
  CHECK(Paints(gnd, quads, 0, 0));

  // Chaque famille prend sa couleur.
  const GreyWorldLook look = EnabledLook();
  const auto wall = ro::render::SkillAreaColorBytes(look.wall_color);
  const auto snipe = ro::render::SkillAreaColorBytes(look.snipe_color);
  const auto colored = [&](const std::array<std::uint8_t, 4>& color) {
    return std::ranges::count_if(quads,
                                 [&color](const GroundQuad& quad) { return quad.color == color; });
  };
  CHECK(colored(wall) == 1);
  CHECK(colored(snipe) == 1);
}

TEST_CASE("GreyWorld : une famille transparente ne pose aucune case", "[render][greyworld]") {
  const GndMap gnd = FourByFourGnd();
  const GatMap gat = FlatGat();
  GreyWorldLook look = EnabledLook();
  look.walk_color = 0x00FFFFFFU;
  std::vector<GroundQuad> quads;
  ComposeGreyWorldCells(gnd, gat, {}, 3, 3, look, quads);
  CHECK(quads.size() == 2);
  CHECK(Paints(gnd, quads, 2, 2));
  CHECK(Paints(gnd, quads, 5, 2));
}

TEST_CASE("GreyWorld : le contour garde les bords, jamais l'intérieur", "[render][greyworld]") {
  const GndMap gnd = FourByFourGnd();
  const GatMap gat = FlatGat();
  GreyWorldLook look = EnabledLook();
  look.fill = GreyWorldFill::kOutline;
  std::vector<GroundQuad> quads;
  ComposeGreyWorldCells(gnd, gat, {}, 3, 3, look, quads);
  // Le mur et ses quatre voisins.
  CHECK(Paints(gnd, quads, 2, 2));
  CHECK(Paints(gnd, quads, 1, 2));
  CHECK(Paints(gnd, quads, 3, 2));
  CHECK(Paints(gnd, quads, 2, 1));
  CHECK(Paints(gnd, quads, 2, 3));
  // Le bord de la carte est un changement de terrain.
  CHECK(Paints(gnd, quads, 0, 4));
  // Une case marchable entourée de marchable n'est pas un bord.
  CHECK_FALSE(Paints(gnd, quads, 4, 5));
}

TEST_CASE("GreyWorld : la portée borne le carré autour du joueur", "[render][greyworld]") {
  const GndMap gnd = FourByFourGnd();
  const GatMap gat = FlatGat();
  GreyWorldLook look = EnabledLook();
  // En deçà de la borne basse : elle s'applique, et huit cellules couvrent ici
  // toute la carte.
  look.radius_cells = 1;
  std::vector<GroundQuad> quads;
  ComposeGreyWorldCells(gnd, gat, {}, 0, 0, look, quads);
  CHECK(quads.size() == 60);
  // Le joint rétrécit chaque carreau depuis son centre.
  const GroundQuad& first = quads.front();
  CHECK(first.corners[1].x - first.corners[0].x == Catch::Approx(0.88F * kWorldUnitsPerCell));
}

TEST_CASE("GreyWorld : le niveau est la moyenne du seul sol", "[render][greyworld]") {
  const GndMap gnd = FourByFourGnd();
  const std::optional<float> level = FlatReliefLevel(gnd);
  REQUIRE(level.has_value());
  CHECK(*level == Catch::Approx(kGroundHeight));

  GndBuildOptions options;
  options.width = 1;
  options.height = 1;
  options.cubes = {GndCubeSpec{.corner_height = {1.0F, 2.0F, 3.0F, 4.0F}, .tile_up = -1}};
  auto voids = GndMap::Parse(BuildGnd(options));
  REQUIRE(voids.has_value());
  CHECK_FALSE(FlatReliefLevel(*voids).has_value());
}

TEST_CASE("GreyWorld : aplatir pose chaque coin au niveau", "[render][greyworld]") {
  GndMap gnd = FourByFourGnd();
  GatMap gat = FlatGat();
  constexpr float kLevel = -7.5F;
  gnd.LevelCornerHeights(kLevel);
  gat.LevelCornerHeights(kLevel);
  for (const ro::formats::GndCube& cube : gnd.cubes()) {
    CHECK(cube.corner_height == std::array<float, 4>{kLevel, kLevel, kLevel, kLevel});
  }
  for (const ro::formats::GatCell& cell : gat.cells()) {
    CHECK(cell.corner_height == std::array<float, 4>{kLevel, kLevel, kLevel, kLevel});
  }
  // Les types ne bougent pas.
  CHECK(gat.at(2, 2).type == kWallType);
  CHECK(gnd.at(3, 3).tile_up == -1);
}

TEST_CASE("GreyWorld : un objet d'effet garde sa hauteur au-dessus du sol", "[render][greyworld]") {
  // L'objet est posé à l'origine du fichier, au milieu de la carte : sur le
  // bord ouest du cube de la troisième colonne, où le sol est à 2 dans l'unité
  // du fichier. Lire la rangée au lieu de la colonne rendrait une autre valeur.
  RswScene rsw = OneEffectAtOrigin();
  const GndMap gnd = EastwardSlopeGnd(true);
  constexpr float kGroundUnderEffect = 2.0F;
  constexpr float kLevel = 1.0F;
  ReseatMapEffects(rsw, gnd, kLevel);
  CHECK(rsw.effects().front().position[1] == Catch::Approx(kLevel - kGroundUnderEffect));
  CHECK(rsw.effects().front().position[0] == Catch::Approx(0.0F));
  CHECK(rsw.effects().front().position[2] == Catch::Approx(0.0F));
}

TEST_CASE("GreyWorld : un objet d'effet au-dessus du vide ne bouge pas", "[render][greyworld]") {
  RswScene rsw = OneEffectAtOrigin();
  ReseatMapEffects(rsw, EastwardSlopeGnd(false), 1.0F);
  CHECK(rsw.effects().front().position[1] == Catch::Approx(0.0F));
}

TEST_CASE("GreyWorld : sans aucune face de sol, le quadrillage couvre tout",
          "[render][greyworld]") {
  std::vector<GroundQuad> quads;
  ComposeGreyWorldCells(EastwardSlopeGnd(false), FlatGat(), {}, 3, 3, EnabledLook(), quads);
  CHECK(quads.size() == kCellsPerSide * kCellsPerSide);
}

// ── LES CASES BLOQUÉES DU DOCUMENT D'ÉDITION ────────────────────────────────

namespace {

// Ce que le document bloque sur la grille d'essai : un mur et une barrière en
// terrain marchable, une barrière sur le mur du fichier, un mur sur sa falaise,
// un mur au-dessus du vide et un autre hors de la grille.
constexpr std::int32_t kAddedWallX = 4;
constexpr std::int32_t kAddedWallY = 4;
constexpr std::int32_t kAddedFenceX = 1;
constexpr std::int32_t kAddedFenceY = 5;

MapEdit BlockingDocument() {
  MapEdit edit;
  edit.BlockCell({.cell_x = kAddedWallX, .cell_y = kAddedWallY, .block = CellBlock::kWall});
  edit.BlockCell({.cell_x = kAddedFenceX, .cell_y = kAddedFenceY, .block = CellBlock::kFence});
  edit.BlockCell({.cell_x = 2, .cell_y = 2, .block = CellBlock::kFence});
  edit.BlockCell({.cell_x = 5, .cell_y = 2, .block = CellBlock::kWall});
  edit.BlockCell({.cell_x = 6, .cell_y = 6, .block = CellBlock::kWall});
  edit.BlockCell({.cell_x = 3, .cell_y = 40, .block = CellBlock::kWall});
  return edit;
}

// La couleur du quad posé sur cette cellule, ou rien quand aucun ne s'y pose.
[[nodiscard]] std::optional<std::array<std::uint8_t, 4>> ColorAt(
    const GndMap& gnd, const std::vector<GroundQuad>& quads, std::int32_t cell_x,
    std::int32_t cell_y) {
  const std::array<float, 3> center =
      ro::render::CellPositionAt(gnd, static_cast<float>(cell_x), static_cast<float>(cell_y));
  for (const GroundQuad& quad : quads) {
    const std::array<float, 2> drawn = CenterOf(quad);
    if (drawn[0] == Catch::Approx(center[0]) && drawn[1] == Catch::Approx(center[2])) {
      return quad.color;
    }
  }
  return std::nullopt;
}

[[nodiscard]] bool SameQuads(const std::vector<GroundQuad>& left,
                             const std::vector<GroundQuad>& right) {
  return std::ranges::equal(left, right, [](const GroundQuad& one, const GroundQuad& other) {
    return one.corners == other.corners && one.texture_uv == other.texture_uv &&
           one.color == other.color;
  });
}

}  // namespace

TEST_CASE("GreyWorld : une case que le document bloque prend la famille de sa sorte",
          "[render][greyworld]") {
  const GndMap gnd = FourByFourGnd();
  const GatMap gat = FlatGat();
  const MapEdit document = BlockingDocument();
  const GreyWorldLook look = EnabledLook();
  const auto walk = ro::render::SkillAreaColorBytes(look.walk_color);
  const auto wall = ro::render::SkillAreaColorBytes(look.wall_color);
  const auto snipe = ro::render::SkillAreaColorBytes(look.snipe_color);
  std::vector<GroundQuad> quads;
  ComposeGreyWorldCells(gnd, gat, document.blocked_cells(), 3, 3, look, quads);
  // Autant de cases qu'avant : bloquer change une couleur, pas le sol.
  CHECK(quads.size() == 60);
  CHECK(ColorAt(gnd, quads, kAddedWallX, kAddedWallY) == wall);
  CHECK(ColorAt(gnd, quads, kAddedFenceX, kAddedFenceY) == snipe);
  // On ne fait que bloquer : le mur du fichier le reste sous une barrière, et
  // sa falaise devient un mur.
  CHECK(ColorAt(gnd, quads, 2, 2) == wall);
  CHECK(ColorAt(gnd, quads, 5, 2) == wall);
  // Les voisines gardent leur famille.
  CHECK(ColorAt(gnd, quads, kAddedWallX + 1, kAddedWallY) == walk);
  CHECK(ColorAt(gnd, quads, kAddedFenceY, kAddedFenceX) == walk);
  // Au-dessus du vide, bloquée ou non, il n'y a pas de sol où poser la case.
  CHECK_FALSE(ColorAt(gnd, quads, 6, 6).has_value());

  // SANS LE DOCUMENT, les mêmes cases sont celles du fichier.
  ComposeGreyWorldCells(gnd, gat, {}, 3, 3, look, quads);
  CHECK(ColorAt(gnd, quads, kAddedWallX, kAddedWallY) == walk);
  CHECK(ColorAt(gnd, quads, kAddedFenceX, kAddedFenceY) == walk);
  CHECK(ColorAt(gnd, quads, 5, 2) == snipe);
}

TEST_CASE("GreyWorld : le quadrillage peint ce que la grille de marche bloque",
          "[render][greyworld]") {
  const GndMap gnd = FourByFourGnd();
  const GatMap gat = FlatGat();
  const MapEdit document = BlockingDocument();
  // LA GRILLE TELLE QUE LA MARCHE LA TIENT : celle du fichier, où les cases du
  // document sont posées pour de bon.
  GatMap walked = FlatGat();
  static_cast<void>(ro::mapedit::BlockGatCells(document.blocked_cells(), walked));

  for (const GreyWorldFill fill : {GreyWorldFill::kAll, GreyWorldFill::kOutline}) {
    GreyWorldLook look = EnabledLook();
    look.fill = fill;
    std::vector<GroundQuad> read;
    std::vector<GroundQuad> written;
    ComposeGreyWorldCells(gnd, gat, document.blocked_cells(), 3, 3, look, read);
    ComposeGreyWorldCells(gnd, walked, {}, 3, 3, look, written);
    CHECK_FALSE(read.empty());
    CHECK(SameQuads(read, written));
  }
}

TEST_CASE("GreyWorld : le contour suit une case que le document bloque", "[render][greyworld]") {
  const GndMap gnd = FourByFourGnd();
  const GatMap gat = FlatGat();
  GreyWorldLook look = EnabledLook();
  look.fill = GreyWorldFill::kOutline;
  std::vector<GroundQuad> quads;
  // Dans le fichier, la case et ses voisines sont de l'intérieur marchable.
  ComposeGreyWorldCells(gnd, gat, {}, 3, 3, look, quads);
  CHECK_FALSE(Paints(gnd, quads, kAddedWallX, kAddedWallY));
  CHECK_FALSE(Paints(gnd, quads, kAddedWallX, kAddedWallY + 1));

  // Bloquée, elle devient un bord, et ses quatre voisines avec elle.
  MapEdit document;
  document.BlockCell({.cell_x = kAddedWallX, .cell_y = kAddedWallY, .block = CellBlock::kWall});
  ComposeGreyWorldCells(gnd, gat, document.blocked_cells(), 3, 3, look, quads);
  CHECK(Paints(gnd, quads, kAddedWallX, kAddedWallY));
  CHECK(Paints(gnd, quads, kAddedWallX - 1, kAddedWallY));
  CHECK(Paints(gnd, quads, kAddedWallX + 1, kAddedWallY));
  CHECK(Paints(gnd, quads, kAddedWallX, kAddedWallY - 1));
  CHECK(Paints(gnd, quads, kAddedWallX, kAddedWallY + 1));
  // Une case plus loin, l'intérieur reste de l'intérieur.
  CHECK_FALSE(Paints(gnd, quads, kAddedWallX - 1, kAddedWallY + 1));
}

TEST_CASE("GreyWorld : marchable éteint, les cases du document restent des obstacles",
          "[render][greyworld]") {
  const GndMap gnd = FourByFourGnd();
  const GatMap gat = FlatGat();
  const MapEdit document = BlockingDocument();
  GreyWorldLook look = EnabledLook();
  look.walk_color = 0x00FFFFFFU;
  std::vector<GroundQuad> quads;
  ComposeGreyWorldCells(gnd, gat, document.blocked_cells(), 3, 3, look, quads);
  // Les deux obstacles du fichier, et les deux que le document ajoute sur du
  // sol marchable.
  CHECK(quads.size() == 4);
  CHECK(Paints(gnd, quads, kAddedWallX, kAddedWallY));
  CHECK(Paints(gnd, quads, kAddedFenceX, kAddedFenceY));
}
