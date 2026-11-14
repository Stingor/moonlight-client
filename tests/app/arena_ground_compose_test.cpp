// LA TERRE COMPOSÉE : une tuile ne dépend que de la case du monde, de son
// bloc et de la graine ; elle lit l'herbe et la terre au point du monde, si
// bien qu'elle se raccorde au texel près à ses voisines, composées ou non ; et
// son masque suit le chemin — la terre au cœur d'un chemin, l'herbe loin de
// lui, la lisière sur la limite des cases.

#include "survivors/arena_ground_compose.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <utility>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/formats/gnd.h"
#include "ro/render/map_scene.h"
#include "ro/sim/survivors_terrain.h"
#include "ro/vfs/vfs.h"

#include "arena_ground_synthetic.h"
#include "support/temp_directory.h"
#include "survivors/arena_ground_look.h"
#include "survivors/arena_ground_paths.h"
#include "survivors/arena_ground_window.h"

using moonclient::ComposeCubeTile;
using moonclient::ComposedGround;
using moonclient::ComposedTileRef;
using moonclient::ComposeGround;
using moonclient::GroundBlend;
using moonclient::GroundBlendOf;
using moonclient::GroundComposeSources;
using moonclient::GroundPlan;
using moonclient::GroundPlanLight;
using moonclient::GroundWindowAround;
using moonclient::GroundWindowOrigin;
using moonclient::kComposedBorderTexels;
using moonclient::kGroundCellsPerCube;
using moonclient::kGroundTextureCubes;
using moonclient::kGroundWindowCubes;
using moonclient::kPathBlockFull;
using moonclient::kPathBlockSide;
using moonclient::MakeGroundComposeSources;
using ro::formats::Image;

namespace {

constexpr std::uint64_t kSeed = 20261084;
constexpr std::uint64_t kOtherSeed = 7;
constexpr std::size_t kChannels = 4;
// Des textures d'essai de 32 texels : 8 par cube, 4 par case.
constexpr std::int32_t kTextureSide = 32;
constexpr std::int32_t kCubeTexels = kTextureSide / kGroundTextureCubes;
constexpr std::int32_t kTexelsPerCell = kCubeTexels / kGroundCellsPerCube;
constexpr std::int32_t kSide = kCubeTexels + 2 * kComposedBorderTexels;
constexpr std::size_t kTileBytes = static_cast<std::size_t>(kSide) * kSide * kChannels;
// Un cube du monde loin de tout bord.
constexpr std::int32_t kCubeX = 16'390;
constexpr std::int32_t kCubeY = 16'401;
// Un cube au sud-ouest de l'origine du monde, de coordonnées négatives qui ne
// tombent pas sur un multiple de la période.
constexpr std::int32_t kWestCubeX = -3;
constexpr std::int32_t kSouthCubeY = -6;
// Les graines des motifs d'essai.
constexpr std::uint32_t kGrassPattern = 11;
constexpr std::uint32_t kDirtPattern = 29;
// L'herbe et la terre unies : la part de terre se lit sur le rouge.
constexpr std::array<std::uint8_t, 3> kGreen{0, 200, 0};
constexpr std::array<std::uint8_t, 3> kBrown{200, 100, 0};
constexpr float kBrownRed = 200.0F;
// Le même point du monde, lu de deux tuiles : à l'octet près.
constexpr int kSamePointTolerance = 0;
// Au moins tant de cubes comparés, pour qu'un essai ait éprouvé quelque chose.
constexpr std::size_t kLeastCompared = 50;
// Le centre d'un texel, en part de texel depuis son coin sud-ouest.
constexpr double kTexelHalf = 0.5;
// Un soleil quelconque : la composition ne le lit pas.
const GroundPlanLight kNoonSun{};
constexpr std::uint8_t kOpaque = 255;
// Le générateur des motifs d'essai : un pas d'un congruentiel 32 bits.
constexpr std::uint32_t kLcgMultiplier = 1'664'525U;
constexpr std::uint32_t kLcgIncrement = 1'013'904'223U;
constexpr unsigned kLcgByteShift = 24;

// Un motif pseudo-aléatoire opaque : chaque texel diffère de ses voisins.
Image Pattern(std::uint32_t seed) {
  Image image{.width = kTextureSide, .height = kTextureSide, .file = ro::formats::ImageFile::kBmp};
  image.pixels.resize(static_cast<std::size_t>(kTextureSide) * kTextureSide * kChannels);
  std::uint32_t state = seed;
  for (std::size_t byte = 0; byte < image.pixels.size(); ++byte) {
    state = state * kLcgMultiplier + kLcgIncrement;
    image.pixels[byte] = (byte % kChannels == kChannels - 1)
                             ? kOpaque
                             : static_cast<std::uint8_t>(state >> kLcgByteShift);
  }
  return image;
}

Image Plain(const std::array<std::uint8_t, 3>& colour) {
  Image image{.width = kTextureSide, .height = kTextureSide, .file = ro::formats::ImageFile::kBmp};
  for (std::int32_t texel = 0; texel < kTextureSide * kTextureSide; ++texel) {
    image.pixels.insert(image.pixels.end(), colour.begin(), colour.end());
    image.pixels.push_back(kOpaque);
  }
  return image;
}

GroundComposeSources PatternSources() {
  std::optional<GroundComposeSources> sources =
      MakeGroundComposeSources(Pattern(kGrassPattern), Pattern(kDirtPattern));
  REQUIRE(sources.has_value());
  return *sources;
}

GroundComposeSources PlainSources() {
  std::optional<GroundComposeSources> sources =
      MakeGroundComposeSources(Plain(kGreen), Plain(kBrown));
  REQUIRE(sources.has_value());
  return *sources;
}

std::vector<std::uint8_t> Tile(const GroundComposeSources& sources, std::uint64_t seed,
                               std::int32_t cube_x, std::int32_t cube_y, std::uint16_t block) {
  std::vector<std::uint8_t> tile(kTileBytes);
  ComposeCubeTile(sources, seed, cube_x, cube_y, block, tile);
  return tile;
}

// Le texel (colonne, rangée) d'une tuile, bord compris, comptés depuis le
// texel de bord sud-ouest.
const std::uint8_t* TexelOf(const std::vector<std::uint8_t>& tile, std::int32_t column,
                            std::int32_t row) {
  return &tile[(static_cast<std::size_t>(row) * kSide + static_cast<std::size_t>(column)) *
               kChannels];
}

// Le texel de la texture source qu'un cube non composé montre au texel
// (colonne, rangée) de la tuile du cube (cube_x, cube_y).
const std::uint8_t* SourceTexel(const Image& image, std::int32_t cube_x, std::int32_t cube_y,
                                std::int32_t column, std::int32_t row) {
  const auto wrap = [](std::int64_t value) {
    const std::int64_t rest = value % kTextureSide;
    return static_cast<std::size_t>(rest < 0 ? rest + kTextureSide : rest);
  };
  const std::size_t x =
      wrap(static_cast<std::int64_t>(cube_x) * kCubeTexels + column - kComposedBorderTexels);
  const std::size_t y =
      wrap(static_cast<std::int64_t>(cube_y) * kCubeTexels + row - kComposedBorderTexels);
  return &image.pixels[(y * kTextureSide + x) * kChannels];
}

bool SameTexel(const std::uint8_t* left, const std::uint8_t* right) {
  for (std::size_t channel = 0; channel < kChannels; ++channel) {
    if (std::abs(static_cast<int>(left[channel]) - static_cast<int>(right[channel])) >
        kSamePointTolerance) {
      return false;
    }
  }
  return true;
}

// Le bloc dont les colonnes [0, path_columns) sont de chemin.
std::uint16_t WestColumns(std::int32_t path_columns) {
  std::uint16_t block = 0;
  for (std::int32_t row = 0; row < kPathBlockSide; ++row) {
    for (std::int32_t column = 0; column < path_columns; ++column) {
      block |= static_cast<std::uint16_t>(1U << (row * kPathBlockSide + column));
    }
  }
  return block;
}

// La part de terre d'un texel d'une tuile unie.
float DirtShare(const std::vector<std::uint8_t>& tile, std::int32_t column, std::int32_t row) {
  return static_cast<float>(TexelOf(tile, column, row)[0]) / kBrownRed;
}

// La tuile d'un cube dans les pages d'une fenêtre composée, bord compris.
std::vector<std::uint8_t> TileInPages(const ComposedGround& composed, const ComposedTileRef& ref) {
  const Image& page = composed.pages[ref.page];
  const auto left = static_cast<std::size_t>(
      std::lround(ref.south_west[0] * static_cast<float>(page.width)) - kComposedBorderTexels);
  const auto bottom = static_cast<std::size_t>(
      std::lround(ref.south_west[1] * static_cast<float>(page.height)) - kComposedBorderTexels);
  std::vector<std::uint8_t> tile;
  tile.reserve(kTileBytes);
  for (std::size_t row = 0; row < static_cast<std::size_t>(kSide); ++row) {
    const auto first =
        page.pixels.begin() +
        static_cast<std::ptrdiff_t>(((bottom + row) * static_cast<std::size_t>(page.width) + left) *
                                    kChannels);
    tile.insert(tile.end(), first, first + kSide * static_cast<std::ptrdiff_t>(kChannels));
  }
  return tile;
}

GroundPlan PlanAt(GroundWindowOrigin origin) {
  const ro::sim::TerrainRule& rule = moonclient::test_support::ShippedPlains();
  const moonclient::BiomeLook* look = moonclient::FindBiomeLook(rule.key);
  REQUIRE(look != nullptr);
  return moonclient::PlanGroundWindow(rule, kSeed, *look, origin, kNoonSun);
}

}  // namespace

TEST_CASE("Terre composée : un bloc vide est l'herbe, un bloc plein la terre, le reste se compose",
          "[app][survivors]") {
  CHECK(GroundBlendOf(0) == GroundBlend::kGrass);
  CHECK(GroundBlendOf(kPathBlockFull) == GroundBlend::kDirt);
  CHECK(GroundBlendOf(1) == GroundBlend::kComposed);
  CHECK(GroundBlendOf(WestColumns(2)) == GroundBlend::kComposed);
  // Des sources trop grossières, ou dépareillées, ne se composent pas.
  CHECK_FALSE(MakeGroundComposeSources(Image{}, Plain(kBrown)).has_value());
  Image coarse{.width = kGroundTextureCubes, .height = kGroundTextureCubes};
  coarse.pixels.assign(
      static_cast<std::size_t>(kGroundTextureCubes) * kGroundTextureCubes * kChannels, kOpaque);
  CHECK_FALSE(MakeGroundComposeSources(coarse, coarse).has_value());
}

TEST_CASE("Terre composée : la tuile est pure — mêmes entrées, mêmes octets", "[app][survivors]") {
  const GroundComposeSources sources = PatternSources();
  const std::uint16_t block = WestColumns(2);
  CHECK(Tile(sources, kSeed, kCubeX, kCubeY, block) == Tile(sources, kSeed, kCubeX, kCubeY, block));
  // La graine trouble la lisière : une autre graine, une autre tuile.
  CHECK(Tile(sources, kSeed, kCubeX, kCubeY, block) !=
        Tile(sources, kOtherSeed, kCubeX, kCubeY, block));
}

TEST_CASE("Terre composée : sans chemin l'herbe du monde, tout de chemin la terre du monde",
          "[app][survivors]") {
  const GroundComposeSources sources = PatternSources();
  // Les blocs extrêmes ne se composent pas, mais leur tuile montrerait
  // exactement ce que montre un cube de texture, bord compris — au sud-ouest
  // de l'origine du monde aussi, où le cube se compte en négatif.
  for (const auto& [cube_x, cube_y] :
       {std::pair{kCubeX, kCubeY}, std::pair{kWestCubeX, kSouthCubeY}}) {
    const std::vector<std::uint8_t> grass = Tile(sources, kSeed, cube_x, cube_y, 0);
    const std::vector<std::uint8_t> dirt = Tile(sources, kSeed, cube_x, cube_y, kPathBlockFull);
    for (std::int32_t row = 0; row < kSide; ++row) {
      for (std::int32_t column = 0; column < kSide; ++column) {
        INFO("cube (" << cube_x << ", " << cube_y << "), texel (" << column << ", " << row << ")");
        CHECK(SameTexel(TexelOf(grass, column, row),
                        SourceTexel(sources.grass, cube_x, cube_y, column, row)));
        CHECK(SameTexel(TexelOf(dirt, column, row),
                        SourceTexel(sources.dirt, cube_x, cube_y, column, row)));
      }
    }
  }
}

TEST_CASE("Terre composée : le masque suit le chemin et garde sa largeur", "[app][survivors]") {
  const GroundComposeSources sources = PlainSources();
  // Le centre de la case ouest du cube, et celui de la case est.
  const std::int32_t west_centre = kComposedBorderTexels + kTexelsPerCell / 2;
  const std::int32_t east_centre = west_centre + kTexelsPerCell;
  const std::int32_t middle_row = kSide / 2;
  // Trois colonnes de chemin : la case ouest du cube et ses voisines en sont,
  // son centre est de terre ; la case est longe l'herbe.
  const std::vector<std::uint8_t> wide = Tile(sources, kSeed, kCubeX, kCubeY, WestColumns(3));
  CHECK(DirtShare(wide, west_centre, middle_row) == 1.0F);
  // Une colonne de chemin, hors du cube : le centre de la case est, à plus
  // d'une case de lui, est d'herbe.
  const std::vector<std::uint8_t> narrow = Tile(sources, kSeed, kCubeX, kCubeY, WestColumns(1));
  CHECK(DirtShare(narrow, east_centre, middle_row) == 0.0F);
  // LA LISIÈRE PASSE SUR LA LIMITE des cases : deux colonnes de chemin, la
  // limite au milieu du cube. En moyenne sur la hauteur, la terre domine juste
  // en deçà, l'herbe juste au-delà.
  for (const std::uint64_t seed : {kSeed, kOtherSeed}) {
    const std::vector<std::uint8_t> half = Tile(sources, seed, kCubeX, kCubeY, WestColumns(2));
    const std::int32_t limit = kComposedBorderTexels + kTexelsPerCell;
    float inside = 0.0F;
    float outside = 0.0F;
    for (std::int32_t row = kComposedBorderTexels; row < kSide - kComposedBorderTexels; ++row) {
      inside += DirtShare(half, limit - 1, row);
      outside += DirtShare(half, limit, row);
    }
    const auto rows = static_cast<float>(kCubeTexels);
    CHECK(inside / rows > 0.5F);
    CHECK(outside / rows < 0.5F);
    CHECK(inside / rows + outside / rows == Catch::Approx(1.0F).margin(0.25F));
  }
}

TEST_CASE("Terre composée : deux fenêtres qui se recouvrent composent les mêmes tuiles",
          "[app][survivors]") {
  const GroundComposeSources sources = PatternSources();
  const GroundWindowOrigin first =
      GroundWindowAround(ro::sim::kTerrainStartCell, ro::sim::kTerrainStartCell);
  const GroundWindowOrigin second{.x_cells = first.x_cells + ro::sim::kTerrainChunkCells,
                                  .y_cells = first.y_cells + 2 * ro::sim::kTerrainChunkCells};
  const ComposedGround here = ComposeGround(sources, kSeed, PlanAt(first));
  const ComposedGround there = ComposeGround(sources, kSeed, PlanAt(second));
  const std::int32_t shift_x = (second.x_cells - first.x_cells) / kGroundCellsPerCube;
  const std::int32_t shift_y = (second.y_cells - first.y_cells) / kGroundCellsPerCube;
  std::size_t compared = 0;
  for (std::int32_t j = shift_y; j < kGroundWindowCubes; ++j) {
    for (std::int32_t i = shift_x; i < kGroundWindowCubes; ++i) {
      const auto own =
          static_cast<std::size_t>(j) * kGroundWindowCubes + static_cast<std::size_t>(i);
      const auto other = static_cast<std::size_t>(j - shift_y) * kGroundWindowCubes +
                         static_cast<std::size_t>(i - shift_x);
      REQUIRE(here.tiles[own].has_value() == there.tiles[other].has_value());
      if (!here.tiles[own].has_value()) continue;
      CHECK(TileInPages(here, *here.tiles[own]) == TileInPages(there, *there.tiles[other]));
      ++compared;
    }
  }
  CHECK(here.overflow == 0);
  CHECK(compared >= kLeastCompared);
}

TEST_CASE("Terre composée : de part et d'autre d'une frontière de cubes, le même point du monde",
          "[app][survivors]") {
  const GroundComposeSources sources = PatternSources();
  const GroundWindowOrigin origin =
      GroundWindowAround(ro::sim::kTerrainStartCell, ro::sim::kTerrainStartCell);
  const GroundPlan plan = PlanAt(origin);
  const ComposedGround composed = ComposeGround(sources, kSeed, plan);
  const std::int32_t world_x = origin.x_cells / kGroundCellsPerCube;
  const std::int32_t world_y = origin.y_cells / kGroundCellsPerCube;
  // Le texel de bord d'un cube est le premier texel du voisin, au même point
  // du monde ; un voisin non composé y montre sa texture, d'herbe ou de terre.
  const auto neighbour_texel = [&](std::int32_t i, std::int32_t j, std::int32_t column,
                                   std::int32_t row) -> std::optional<std::vector<std::uint8_t>> {
    const auto rank =
        static_cast<std::size_t>(j) * kGroundWindowCubes + static_cast<std::size_t>(i);
    const std::uint16_t block = plan.path_blocks[rank];
    if (composed.tiles[rank].has_value()) {
      const std::vector<std::uint8_t> tile = TileInPages(composed, *composed.tiles[rank]);
      const std::uint8_t* texel = TexelOf(tile, column, row);
      return std::vector<std::uint8_t>(texel, texel + kChannels);
    }
    const Image& image = GroundBlendOf(block) == GroundBlend::kDirt ? sources.dirt : sources.grass;
    const std::uint8_t* texel = SourceTexel(image, world_x + i, world_y + j, column, row);
    return std::vector<std::uint8_t>(texel, texel + kChannels);
  };
  std::size_t compared = 0;
  for (std::int32_t j = 1; j + 1 < kGroundWindowCubes; ++j) {
    for (std::int32_t i = 1; i + 1 < kGroundWindowCubes; ++i) {
      const auto rank =
          static_cast<std::size_t>(j) * kGroundWindowCubes + static_cast<std::size_t>(i);
      if (!composed.tiles[rank].has_value()) continue;
      const std::vector<std::uint8_t> tile = TileInPages(composed, *composed.tiles[rank]);
      for (std::int32_t along = kComposedBorderTexels; along < kSide - kComposedBorderTexels;
           ++along) {
        // Le bord est de ce cube et la première colonne du voisin de l'est ;
        // le bord nord et la première rangée du voisin du nord.
        const auto east = neighbour_texel(i + 1, j, kComposedBorderTexels, along);
        const auto north = neighbour_texel(i, j + 1, along, kComposedBorderTexels);
        INFO("cube (" << i << ", " << j << "), texel " << along);
        CHECK(SameTexel(TexelOf(tile, kSide - kComposedBorderTexels, along), east->data()));
        CHECK(SameTexel(TexelOf(tile, along, kSide - kComposedBorderTexels), north->data()));
        // Et l'inverse : le dernier texel de ce cube est le bord du voisin.
        const auto east_border = neighbour_texel(i + 1, j, 0, along);
        CHECK(SameTexel(TexelOf(tile, kSide - 2 * kComposedBorderTexels, along),
                        east_border->data()));
      }
      ++compared;
    }
  }
  CHECK(compared >= kLeastCompared);
}

namespace {

// UNE PRAIRIE TEINTÉE : l'herbe verte, la terre rousse, toutes deux en motif ;
// la tache, de l'herbe autour d'un carré de terre au milieu.
constexpr std::uint8_t kTintFloor = 160;
constexpr std::uint8_t kTintSpread = 63;
constexpr std::int32_t kSquareFirst = kTextureSide / 4;
constexpr std::int32_t kSquareLast = 3 * kTextureSide / 4;
constexpr std::uint8_t kQuarterTurns = 4;

Image Tinted(std::uint32_t seed, bool green) {
  Image image = Pattern(seed);
  for (std::size_t texel = 0; texel < image.pixels.size(); texel += kChannels) {
    const std::uint8_t strong =
        static_cast<std::uint8_t>(kTintFloor + (image.pixels[texel] & kTintSpread));
    const std::uint8_t weak = static_cast<std::uint8_t>(image.pixels[texel + 1] & kTintSpread);
    image.pixels[texel] = green ? weak : strong;
    image.pixels[texel + 1] = green ? strong : weak;
  }
  return image;
}

GroundComposeSources TintedSources() {
  std::optional<GroundComposeSources> sources =
      MakeGroundComposeSources(Tinted(kGrassPattern, true), Tinted(kDirtPattern, false));
  REQUIRE(sources.has_value());
  return *sources;
}

Image SquarePatch() {
  Image patch = Tinted(kGrassPattern + 1, true);
  const Image dirt = Tinted(kDirtPattern + 1, false);
  for (std::int32_t y = kSquareFirst; y < kSquareLast; ++y) {
    for (std::int32_t x = kSquareFirst; x < kSquareLast; ++x) {
      const std::size_t at =
          (static_cast<std::size_t>(y) * kTextureSide + static_cast<std::size_t>(x)) * kChannels;
      std::copy_n(dirt.pixels.begin() + static_cast<std::ptrdiff_t>(at), kChannels,
                  patch.pixels.begin() + static_cast<std::ptrdiff_t>(at));
    }
  }
  return patch;
}

// Le texel de l'emplacement que montre le texel (x, y) de la tache posée
// avec `turn` quarts de tour.
std::pair<std::int32_t, std::int32_t> PlaceOf(std::int32_t x, std::int32_t y, std::uint8_t turn) {
  const std::int32_t last = kTextureSide - 1;
  switch (turn % kQuarterTurns) {
    case 1:
      return {last - y, x};
    case 2:
      return {last - x, last - y};
    case 3:
      return {y, last - x};
    default:
      return {x, y};
  }
}

const std::uint8_t* At(const Image& image, std::int32_t x, std::int32_t y) {
  return &image.pixels[(static_cast<std::size_t>(y) * kTextureSide + static_cast<std::size_t>(x)) *
                       kChannels];
}

}  // namespace

TEST_CASE("Tache recomposée : le pourtour est l'herbe de fond de l'emplacement, le cœur sa terre",
          "[app][survivors]") {
  const GroundComposeSources sources = TintedSources();
  const Image patch = SquarePatch();
  for (std::uint8_t turn = 0; turn < kQuarterTurns; ++turn) {
    const std::optional<Image> recomposed = moonclient::ComposePatchTexture(sources, patch, turn);
    REQUIRE(recomposed.has_value());
    std::size_t rim = 0;
    std::size_t core = 0;
    for (std::int32_t y = 0; y < kTextureSide; ++y) {
      for (std::int32_t x = 0; x < kTextureSide; ++x) {
        const auto [place_x, place_y] = PlaceOf(x, y, turn);
        // La place suit la règle de pose des motifs : le cube de texture du
        // texel est celui que montre le cube de sa place.
        const moonclient::TextureCube shown =
            moonclient::TextureCubeOf(static_cast<std::uint8_t>(place_x / kCubeTexels),
                                      static_cast<std::uint8_t>(place_y / kCubeTexels), turn);
        CHECK(shown.u == x / kCubeTexels);
        CHECK(shown.v == y / kCubeTexels);
        const std::int32_t edge = std::min({x, y, kTextureSide - 1 - x, kTextureSide - 1 - y});
        INFO("quart " << int{turn} << ", texel (" << x << ", " << y << ")");
        if (edge < moonclient::kPatchRimTexels) {
          CHECK(SameTexel(At(*recomposed, x, y), At(sources.grass, place_x, place_y)));
          ++rim;
        }
        const bool deep = x >= kSquareFirst + moonclient::kPatchMaskReachTexels &&
                          y >= kSquareFirst + moonclient::kPatchMaskReachTexels &&
                          x < kSquareLast - moonclient::kPatchMaskReachTexels &&
                          y < kSquareLast - moonclient::kPatchMaskReachTexels;
        if (deep) {
          CHECK(SameTexel(At(*recomposed, x, y), At(sources.dirt, place_x, place_y)));
          ++core;
        }
      }
    }
    CHECK(rim > 0);
    CHECK(core > 0);
  }
  // Des sources sans écart du vert au rouge ne lisent pas de masque.
  CHECK_FALSE(moonclient::ComposePatchTexture(PatternSources(), patch, 0).has_value());
}

TEST_CASE("Motif bordé : le bord est la période suivante, à la place que pose le quart de tour",
          "[app][survivors]") {
  const GroundComposeSources sources = TintedSources();
  const Image patch = SquarePatch();
  const std::int32_t border = moonclient::kMotifBorderTexels;
  const std::int32_t bordered_side = kTextureSide + 2 * border;
  const auto wrap = [](std::int32_t value) {
    return (value % kTextureSide + kTextureSide) % kTextureSide;
  };
  const auto texel_of = [&](const Image& image, std::int32_t x, std::int32_t y) {
    return &image.pixels[(static_cast<std::size_t>(y + border) *
                              static_cast<std::size_t>(bordered_side) +
                          static_cast<std::size_t>(x + border)) *
                         kChannels];
  };
  for (std::uint8_t turn = 0; turn < kQuarterTurns; ++turn) {
    const std::optional<Image> recomposed = moonclient::ComposePatchTexture(sources, patch, turn);
    REQUIRE(recomposed.has_value());
    const Image bordered = moonclient::BorderMotif(*recomposed, sources.grass, turn);
    REQUIRE(bordered.width == bordered_side);
    REQUIRE(bordered.height == bordered_side);
    for (std::int32_t y = -border; y < kTextureSide + border; ++y) {
      for (std::int32_t x = -border; x < kTextureSide + border; ++x) {
        INFO("quart " << int{turn} << ", texel (" << x << ", " << y << ")");
        if (x >= 0 && y >= 0 && x < kTextureSide && y < kTextureSide) {
          CHECK(SameTexel(texel_of(bordered, x, y), At(*recomposed, x, y)));
          continue;
        }
        const auto [place_x, place_y] = PlaceOf(x, y, turn);
        CHECK(SameTexel(texel_of(bordered, x, y), At(sources.grass, wrap(place_x), wrap(place_y))));
      }
    }
  }
  // Sans quart de tour, l'herbe bordée d'elle-même se prolonge d'un bord à
  // l'autre : le texel au-delà de l'est est celui de l'ouest.
  const Image grass = moonclient::BorderMotif(sources.grass, sources.grass, 0);
  for (std::int32_t y = 0; y < kTextureSide; ++y) {
    CHECK(SameTexel(texel_of(grass, kTextureSide, y), At(sources.grass, 0, y)));
  }
}

TEST_CASE("Terre composée : sur une fenêtre de la prairie livrée, la terre a l'aire des chemins",
          "[app][survivors]") {
  // L'aire de terre, en cases : la part de terre de chaque texel des cubes
  // composés, plus quatre cases par cube de terre pleine. Elle vaut l'aire des
  // cases de chemin à kAreaSlack près : la lisière ne gagne ni ne perd de
  // largeur, elle ne fait qu'arrondir les marches.
  constexpr double kAreaSlack = 0.01;
  const GroundComposeSources sources = PlainSources();
  const GroundPlan plan =
      PlanAt(GroundWindowAround(ro::sim::kTerrainStartCell, ro::sim::kTerrainStartCell));
  const ComposedGround composed = ComposeGround(sources, kSeed, plan);
  REQUIRE(composed.overflow == 0);
  const double cells_per_texel =
      static_cast<double>(kGroundCellsPerCube * kGroundCellsPerCube) / (kCubeTexels * kCubeTexels);
  double dirt_cells = 0.0;
  for (std::size_t rank = 0; rank < plan.cubes.size(); ++rank) {
    if (GroundBlendOf(plan.path_blocks[rank]) == GroundBlend::kDirt) {
      dirt_cells += kGroundCellsPerCube * kGroundCellsPerCube;
    }
    if (!composed.tiles[rank].has_value()) continue;
    const std::vector<std::uint8_t> tile = TileInPages(composed, *composed.tiles[rank]);
    for (std::int32_t row = kComposedBorderTexels; row < kSide - kComposedBorderTexels; ++row) {
      for (std::int32_t column = kComposedBorderTexels; column < kSide - kComposedBorderTexels;
           ++column) {
        dirt_cells += static_cast<double>(DirtShare(tile, column, row)) * cells_per_texel;
      }
    }
  }
  double path_cells = 0.0;
  for (const std::uint8_t on_path : plan.path) path_cells += on_path;
  REQUIRE(path_cells > 0.0);
  CHECK(dirt_cells / path_cells == Catch::Approx(1.0).margin(kAreaSlack));
}

namespace {

// LE POINT DU MONDE (east, north), compté en cubes depuis le coin sud-ouest
// du cube, lu comme le sol le peint : les coordonnées de texture de son
// carreau, mêlées des quatre coins dans l'ordre du fichier (sud-ouest,
// sud-est, nord-ouest, nord-est), puis le texel qu'elles désignent dans sa
// texture. Hors du cube, le prolongement du même carreau : ce que le filtrage
// lit au bord.
const std::uint8_t* ShownTexel(const ro::render::MapScene& scene, std::int32_t i, std::int32_t j,
                               double east, double north) {
  const ro::formats::GndTile& tile =
      scene.gnd.tiles()[static_cast<std::size_t>(scene.gnd.at(i, j).tile_up)];
  const auto corners = [east, north](const std::array<float, 4>& at) {
    const auto corner = [&at](std::size_t rank) { return static_cast<double>(at[rank]); };
    return (1.0 - east) * (1.0 - north) * corner(0) + east * (1.0 - north) * corner(1) +
           (1.0 - east) * north * corner(2) + east * north * corner(3);
  };
  const Image& image = scene.terrain_textures[static_cast<std::size_t>(tile.texture_index)];
  const auto x = static_cast<std::int64_t>(std::floor(corners(tile.u) * image.width));
  const auto y = static_cast<std::int64_t>(std::floor(corners(tile.v) * image.height));
  REQUIRE(x >= 0);
  REQUIRE(y >= 0);
  REQUIRE(x < image.width);
  REQUIRE(y < image.height);
  return &image.pixels[(static_cast<std::size_t>(y) * static_cast<std::size_t>(image.width) +
                        static_cast<std::size_t>(x)) *
                       kChannels];
}

}  // namespace

TEST_CASE(
    "Terre composée : la fenêtre assemblée montre au bord d'un cube composé ce que montre "
    "son voisin, coins compris",
    "[app][survivors]") {
  const ro::test_support::TempDirectory client;
  moonclient::test_support::FillSyntheticPrairie(client);
  auto vfs = ro::vfs::Vfs::Mount({.root = client.path(), .archives = {"essai.grf"}});
  REQUIRE(vfs.has_value());
  auto sources =
      moonclient::OpenGroundSources(*vfs, moonclient::test_support::ShippedPlains(), kSeed,
                                    moonclient::test_support::SyntheticPrairieLook());
  REQUIRE(sources.has_value());
  REQUIRE(sources->compose.has_value());
  const GroundWindowOrigin origin =
      GroundWindowAround(ro::sim::kTerrainStartCell, ro::sim::kTerrainStartCell);
  const GroundPlan plan = moonclient::PlanGroundWindow(*sources->rule, sources->run_seed,
                                                       *sources->look, origin, sources->plan_light);
  auto stage = moonclient::AssembleGroundWindow(*vfs, *sources, plan, "essai");
  REQUIRE(stage.has_value());
  const ro::render::MapScene& scene = stage->scene;
  const auto cube_texels = static_cast<double>(sources->compose->cube_texels);
  // LE CENTRE DU TEXEL `texel` d'un axe du cube, en cubes ; −1 et
  // cube_texels sont les texels de bord, chez le voisin.
  const auto centre = [cube_texels](std::int32_t texel) {
    return (static_cast<double>(texel) + kTexelHalf) / cube_texels;
  };
  std::size_t beside_plain = 0;
  std::size_t corners_compared = 0;
  for (std::int32_t j = 1; j + 1 < kGroundWindowCubes; ++j) {
    for (std::int32_t i = 1; i + 1 < kGroundWindowCubes; ++i) {
      const auto rank =
          static_cast<std::size_t>(j) * kGroundWindowCubes + static_cast<std::size_t>(i);
      if (GroundBlendOf(plan.path_blocks[rank]) != GroundBlend::kComposed) continue;
      for (std::int32_t dj = -1; dj <= 1; ++dj) {
        for (std::int32_t di = -1; di <= 1; ++di) {
          if (di == 0 && dj == 0) continue;
          const auto neighbour = static_cast<std::size_t>(j + dj) * kGroundWindowCubes +
                                 static_cast<std::size_t>(i + di);
          const bool plain = GroundBlendOf(plan.path_blocks[neighbour]) != GroundBlend::kComposed;
          // Le long d'un côté, chaque texel ; en diagonale, le seul du coin.
          const std::int32_t along_count =
              di != 0 && dj != 0 ? 1 : static_cast<std::int32_t>(cube_texels);
          for (std::int32_t along = 0; along < along_count; ++along) {
            const std::int32_t texel_x =
                di > 0 ? static_cast<std::int32_t>(cube_texels) : (di < 0 ? -1 : along);
            const std::int32_t texel_y =
                dj > 0 ? static_cast<std::int32_t>(cube_texels) : (dj < 0 ? -1 : along);
            const double east = centre(texel_x);
            const double north = centre(texel_y);
            INFO("cube (" << i << ", " << j << "), voisin (" << di << ", " << dj << "), texel "
                          << along);
            CHECK(SameTexel(ShownTexel(scene, i, j, east, north),
                            ShownTexel(scene, i + di, j + dj, east - di, north - dj)));
          }
          if (plain) ++beside_plain;
          if (plain && di != 0 && dj != 0) ++corners_compared;
        }
      }
    }
  }
  CHECK(beside_plain >= kLeastCompared);
  CHECK(corners_compared > 0);
}
