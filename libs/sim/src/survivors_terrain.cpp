#include "ro/sim/survivors_terrain.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "ro/core/pcg32.h"

namespace ro::sim {
namespace {

// Les sels des mélanges, en ASCII : « SURVTERR » pour la graine d'un morceau,
// « SURVDENS » pour les nœuds de densité, « SURVMARK » pour le morceau à
// repère de chaque carré.
constexpr std::uint64_t kTerrainSalt = 0x5355'5256'5445'5252ULL;
constexpr std::uint64_t kDensitySalt = 0x5355'5256'4445'4E53ULL;
constexpr std::uint64_t kLandmarkSalt = 0x5355'5256'4D41'524BULL;
// Les constantes publiées de SplitMix64 : l'incrément (partie fractionnaire du
// nombre d'or) et les deux multiplicateurs du finaliseur.
constexpr std::uint64_t kSplitMixGamma = 0x9E37'79B9'7F4A'7C15ULL;
constexpr std::uint64_t kSplitMixFirstMultiplier = 0xBF58'476D'1CE4'E5B9ULL;
constexpr std::uint64_t kSplitMixSecondMultiplier = 0x94D0'49BB'1331'11EBULL;
constexpr unsigned kSplitMixFirstShift = 30;
constexpr unsigned kSplitMixSecondShift = 27;
constexpr unsigned kSplitMixLastShift = 31;
// Le mélange du morceau à repère donne deux décalages : l'un dans la moitié
// basse, l'autre dans la moitié haute.
constexpr unsigned kHalfWordBits = 32;
constexpr char kBlockedCell = '#';

constexpr std::int32_t kSlotCentreCells = kTerrainSlotCells / 2;

// Un composant entier signé entre dans le mélange par sa valeur modulo 2^64 :
// une conversion définie, et aucun décalage de signé.
[[nodiscard]] constexpr std::uint64_t Component(std::int64_t value) noexcept {
  return static_cast<std::uint64_t>(value);
}

// Un pas de SplitMix64 : l'état avance d'un composant et de l'incrément, puis
// passe par le finaliseur.
[[nodiscard]] constexpr std::uint64_t MixStep(std::uint64_t state,
                                              std::uint64_t component) noexcept {
  std::uint64_t mixed = state + component + kSplitMixGamma;
  mixed = (mixed ^ (mixed >> kSplitMixFirstShift)) * kSplitMixFirstMultiplier;
  mixed = (mixed ^ (mixed >> kSplitMixSecondShift)) * kSplitMixSecondMultiplier;
  return mixed ^ (mixed >> kSplitMixLastShift);
}

[[nodiscard]] constexpr std::uint64_t Mix(std::uint64_t run_seed, std::uint64_t salt,
                                          std::int64_t x, std::int64_t y) noexcept {
  return MixStep(MixStep(MixStep(run_seed, salt), Component(x)), Component(y));
}

[[nodiscard]] constexpr std::int64_t FloorDivide(std::int64_t value, std::int64_t by) noexcept {
  const std::int64_t quotient = value / by;
  return (value % by != 0 && (value < 0) != (by < 0)) ? quotient - 1 : quotient;
}

// La distance de Tchebychev du départ à la case la plus proche d'un rectangle
// [west, east] × [south, north], bornes comprises.
[[nodiscard]] std::int64_t DistanceFromStart(std::int64_t west, std::int64_t south,
                                             std::int64_t east, std::int64_t north) noexcept {
  const std::int64_t dx =
      std::max({west - kTerrainStartCell, std::int64_t{0}, std::int64_t{kTerrainStartCell} - east});
  const std::int64_t dy = std::max(
      {south - kTerrainStartCell, std::int64_t{0}, std::int64_t{kTerrainStartCell} - north});
  return std::max(dx, dy);
}

// La valeur d'un nœud de densité, dans [min, max] ; min seul si la règle les
// inverse.
[[nodiscard]] std::uint64_t DensityNode(const TerrainRule& rule, std::uint64_t run_seed,
                                        std::int64_t node_x, std::int64_t node_y) noexcept {
  const std::uint64_t least = rule.density_min_per_mille;
  if (rule.density_max_per_mille <= rule.density_min_per_mille) return least;
  const std::uint64_t span = std::uint64_t{rule.density_max_per_mille} - least + 1;
  return least + Mix(run_seed, kDensitySalt, node_x, node_y) % span;
}

// Les poids des pièces, ceux de l'autre sorte à zéro : un même index désigne
// la même pièce dans les deux tirages.
[[nodiscard]] std::vector<std::uint32_t> WeightsOf(std::span<const TerrainPieceRule> pieces,
                                                   bool landmark) {
  std::vector<std::uint32_t> weights;
  weights.reserve(pieces.size());
  for (const TerrainPieceRule& piece : pieces) {
    weights.push_back(piece.landmark == landmark ? piece.weight : 0U);
  }
  return weights;
}

// Le coin d'une pièce dans un emplacement de `slot_cells` : deux tirages,
// toujours, même quand la pièce ne tient pas.
struct Corner {
  std::int32_t east = 0;
  std::int32_t north = 0;
  bool fits = false;
};

[[nodiscard]] Corner DrawCorner(core::Pcg32& random, FootprintSize size, std::int32_t slot_cells) {
  const std::int32_t room = slot_cells - 2 * kTerrainBorderCells;
  const bool fits = size.width > 0 && size.height > 0 && size.width <= room && size.height <= room;
  const std::int32_t spare_east = fits ? room - size.width : 0;
  const std::int32_t spare_north = fits ? room - size.height : 0;
  const auto east =
      static_cast<std::int32_t>(random.Below(static_cast<std::uint32_t>(spare_east) + 1U));
  const auto north =
      static_cast<std::int32_t>(random.Below(static_cast<std::uint32_t>(spare_north) + 1U));
  return Corner{
      .east = kTerrainBorderCells + east, .north = kTerrainBorderCells + north, .fits = fits};
}

// Une pièce tirée pour un emplacement, posée ou non.
struct Draw {
  std::size_t piece_index = 0;
  std::uint8_t quarter = 0;
  Corner corner;
};

[[nodiscard]] Draw DrawPiece(core::Pcg32& random, std::span<const TerrainPieceRule> pieces,
                             std::span<const std::uint32_t> weights, std::int32_t slot_cells) {
  Draw draw;
  draw.piece_index = random.PickWeighted(weights);
  draw.quarter = static_cast<std::uint8_t>(random.Below(kTerrainQuarterTurns));
  const FootprintSize size = draw.piece_index < pieces.size()
                                 ? FootprintSizeOf(pieces[draw.piece_index], draw.quarter)
                                 : FootprintSize{};
  draw.corner = DrawCorner(random, size, slot_cells);
  return draw;
}

// Pose une pièce tirée, coin de l'emplacement en coordonnées locales du morceau.
void Place(TerrainChunk& chunk, std::span<const TerrainPieceRule> pieces, const Draw& draw,
           std::int32_t slot_east, std::int32_t slot_north) {
  if (draw.piece_index >= pieces.size() || !draw.corner.fits) return;
  const TerrainPieceRule& piece = pieces[draw.piece_index];
  const FootprintSize size = FootprintSizeOf(piece, draw.quarter);
  const std::int32_t west = slot_east + draw.corner.east;
  const std::int32_t south = slot_north + draw.corner.north;
  for (std::int32_t north = 0; north < size.height; ++north) {
    for (std::int32_t east = 0; east < size.width; ++east) {
      if (!FootprintBlocks(piece, draw.quarter, east, north)) continue;
      const auto cell =
          static_cast<std::size_t>(south + north) * static_cast<std::size_t>(kTerrainChunkCells) +
          static_cast<std::size_t>(west + east);
      chunk.passable[cell] = 0U;
    }
  }
  chunk.pieces.push_back(PlacedPiece{.piece_id = piece.piece_id,
                                     .x = chunk.chunk_x * kTerrainChunkCells + west,
                                     .y = chunk.chunk_y * kTerrainChunkCells + south,
                                     .quarter = draw.quarter});
}

// Vrai si ce morceau est celui de son carré qui porte un repère.
[[nodiscard]] bool HostsLandmark(const TerrainRule& rule, std::uint64_t run_seed,
                                 std::int32_t chunk_x, std::int32_t chunk_y) noexcept {
  const std::int64_t period = rule.landmark_period_chunks;
  if (period <= 0) return false;
  const std::int64_t block_x = FloorDivide(chunk_x, period);
  const std::int64_t block_y = FloorDivide(chunk_y, period);
  const std::uint64_t mixed = Mix(run_seed, kLandmarkSalt, block_x, block_y);
  const auto chosen_x = static_cast<std::int64_t>(mixed % static_cast<std::uint64_t>(period));
  const auto chosen_y =
      static_cast<std::int64_t>((mixed >> kHalfWordBits) % static_cast<std::uint64_t>(period));
  return chunk_x - block_x * period == chosen_x && chunk_y - block_y * period == chosen_y;
}

}  // namespace

bool TerrainChunk::Passable(std::int32_t local_x, std::int32_t local_y) const noexcept {
  if (local_x < 0 || local_y < 0 || local_x >= kTerrainChunkCells ||
      local_y >= kTerrainChunkCells) {
    return false;
  }
  return passable[static_cast<std::size_t>(local_y) * static_cast<std::size_t>(kTerrainChunkCells) +
                  static_cast<std::size_t>(local_x)] != 0U;
}

FootprintSize FootprintSizeOf(const TerrainPieceRule& piece, std::uint8_t quarter) noexcept {
  const auto height = static_cast<std::int32_t>(piece.footprint.size());
  const auto width =
      piece.footprint.empty() ? 0 : static_cast<std::int32_t>(piece.footprint.front().size());
  if (quarter % 2 == 1) return FootprintSize{.width = height, .height = width};
  return FootprintSize{.width = width, .height = height};
}

bool FootprintBlocks(const TerrainPieceRule& piece, std::uint8_t quarter, std::int32_t east,
                     std::int32_t north) noexcept {
  const FootprintSize drawn = FootprintSizeOf(piece, 0);
  // La case du dessin d'origine (colonne, rangée depuis le sud) qui vient en
  // (east, north) après le quart de tour : la rotation inverse.
  std::int32_t column = east;
  std::int32_t row = north;
  switch (quarter % kTerrainQuarterTurns) {
    case 1:
      column = north;
      row = drawn.height - 1 - east;
      break;
    case 2:
      column = drawn.width - 1 - east;
      row = drawn.height - 1 - north;
      break;
    case 3:
      column = drawn.width - 1 - north;
      row = east;
      break;
    default:
      break;
  }
  if (column < 0 || row < 0 || row >= drawn.height) return false;
  const std::string_view line = piece.footprint[static_cast<std::size_t>(drawn.height - 1 - row)];
  if (static_cast<std::size_t>(column) >= line.size()) return false;
  return line[static_cast<std::size_t>(column)] == kBlockedCell;
}

std::int32_t TerrainChunkOf(std::int32_t cell) noexcept {
  return static_cast<std::int32_t>(FloorDivide(cell, kTerrainChunkCells));
}

std::uint64_t TerrainChunkSeed(std::uint64_t run_seed, std::int32_t chunk_x,
                               std::int32_t chunk_y) noexcept {
  return Mix(run_seed, kTerrainSalt, chunk_x, chunk_y);
}

std::uint32_t TerrainDensityPerMille(const TerrainRule& rule, std::uint64_t run_seed,
                                     std::int64_t cell_x, std::int64_t cell_y) {
  const std::int64_t period_cells =
      std::int64_t{std::max(rule.density_period_chunks, 1)} * kTerrainChunkCells;
  const std::int64_t node_x = FloorDivide(cell_x, period_cells);
  const std::int64_t node_y = FloorDivide(cell_y, period_cells);
  // Les poids de l'interpolation bilinéaire : entiers, positifs, de somme
  // period_cells², qui tient en 32 bits pour la plus grande période permise.
  const auto east = static_cast<std::uint64_t>(cell_x - node_x * period_cells);
  const auto north = static_cast<std::uint64_t>(cell_y - node_y * period_cells);
  const auto period = static_cast<std::uint64_t>(period_cells);
  const std::uint64_t west = period - east;
  const std::uint64_t south = period - north;
  const std::uint64_t blended = DensityNode(rule, run_seed, node_x, node_y) * west * south +
                                DensityNode(rule, run_seed, node_x + 1, node_y) * east * south +
                                DensityNode(rule, run_seed, node_x, node_y + 1) * west * north +
                                DensityNode(rule, run_seed, node_x + 1, node_y + 1) * east * north;
  return static_cast<std::uint32_t>(blended / (period * period));
}

TerrainChunk GenerateTerrainChunk(const TerrainRule& rule, std::uint64_t run_seed,
                                  std::int32_t chunk_x, std::int32_t chunk_y) {
  TerrainChunk chunk;
  chunk.chunk_x = chunk_x;
  chunk.chunk_y = chunk_y;
  chunk.passable.fill(1U);
  const std::int64_t chunk_west = std::int64_t{chunk_x} * kTerrainChunkCells;
  const std::int64_t chunk_south = std::int64_t{chunk_y} * kTerrainChunkCells;
  const bool landmark_chunk =
      HostsLandmark(rule, run_seed, chunk_x, chunk_y) &&
      DistanceFromStart(chunk_west, chunk_south, chunk_west + kTerrainChunkCells - 1,
                        chunk_south + kTerrainChunkCells - 1) >= rule.clearing_cells;
  const std::vector<std::uint32_t> ordinary = WeightsOf(rule.pieces, false);
  const std::vector<std::uint32_t> landmarks = WeightsOf(rule.pieces, true);
  core::Pcg32 random(TerrainChunkSeed(run_seed, chunk_x, chunk_y), kTerrainStream);
  for (std::int32_t slot_y = 0; slot_y < kTerrainSlotsPerSide; ++slot_y) {
    for (std::int32_t slot_x = 0; slot_x < kTerrainSlotsPerSide; ++slot_x) {
      const std::int32_t slot_east = slot_x * kTerrainSlotCells;
      const std::int32_t slot_north = slot_y * kTerrainSlotCells;
      const std::int64_t west = chunk_west + slot_east;
      const std::int64_t south = chunk_south + slot_north;
      const std::uint32_t density =
          TerrainDensityPerMille(rule, run_seed, west + kSlotCentreCells, south + kSlotCentreCells);
      const bool occupied = random.Chance(density, kTerrainPerMille);
      const Draw draw = DrawPiece(random, rule.pieces, ordinary, kTerrainSlotCells);
      const bool cleared = DistanceFromStart(west, south, west + kTerrainSlotCells - 1,
                                             south + kTerrainSlotCells - 1) < rule.clearing_cells;
      if (occupied && !cleared && !landmark_chunk) {
        Place(chunk, rule.pieces, draw, slot_east, slot_north);
      }
    }
  }
  const Draw landmark = DrawPiece(random, rule.pieces, landmarks, kTerrainChunkCells);
  if (landmark_chunk) Place(chunk, rule.pieces, landmark, 0, 0);
  return chunk;
}

bool TerrainPassableAt(const TerrainRule& rule, std::uint64_t run_seed, std::int32_t x,
                       std::int32_t y) {
  if (x < 0 || y < 0 || x >= kTerrainWorldCells || y >= kTerrainWorldCells) return false;
  const std::int32_t chunk_x = TerrainChunkOf(x);
  const std::int32_t chunk_y = TerrainChunkOf(y);
  return GenerateTerrainChunk(rule, run_seed, chunk_x, chunk_y)
      .Passable(x - chunk_x * kTerrainChunkCells, y - chunk_y * kTerrainChunkCells);
}

}  // namespace ro::sim
