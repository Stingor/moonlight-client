#include "ro/sim/survivors_ground.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <utility>

#include "ro/sim/survivors_terrain.h"
#include "ro/sim/walk_map.h"

namespace ro::sim {

namespace {

[[nodiscard]] bool InsideWorld(std::int32_t cell) noexcept {
  return cell >= 0 && cell < kTerrainWorldCells;
}

[[nodiscard]] bool InsideWorldChunks(std::int32_t chunk) noexcept {
  return chunk >= 0 && chunk < kTerrainWorldChunks;
}

}  // namespace

SurvivorsGround SurvivorsGround::Bounded(WalkMap map) {
  SurvivorsGround ground;
  ground.bounded_ = true;
  ground.map_ = std::move(map);
  return ground;
}

SurvivorsGround SurvivorsGround::Generated(const TerrainRule& rule, std::uint64_t run_seed,
                                           std::int32_t cache_side_chunks) {
  SurvivorsGround ground;
  ground.bounded_ = false;
  ground.rule_ = &rule;
  ground.run_seed_ = run_seed;
  ground.cache_side_chunks_ = std::clamp(cache_side_chunks, 1, kTerrainWorldChunks);
  const auto side = static_cast<std::size_t>(ground.cache_side_chunks_);
  ground.cache_.resize(side * side);
  return ground;
}

std::int32_t SurvivorsGround::width() const noexcept {
  return bounded_ ? map_.width() : kTerrainWorldCells;
}

std::int32_t SurvivorsGround::height() const noexcept {
  return bounded_ ? map_.height() : kTerrainWorldCells;
}

const TerrainChunk& SurvivorsGround::Cached(std::int32_t chunk_x, std::int32_t chunk_y) const {
  // Le monde n'a que des morceaux de coordonnées positives : le reste de la
  // division désigne la case du cache, l'étiquette (chunk_x, chunk_y) dit si
  // elle porte le bon morceau.
  const auto side = static_cast<std::size_t>(cache_side_chunks_);
  const std::size_t slot =
      (static_cast<std::size_t>(chunk_y) % side) * side + static_cast<std::size_t>(chunk_x) % side;
  CachedChunk& cached = cache_[slot];
  if (!cached.filled || cached.chunk.chunk_x != chunk_x || cached.chunk.chunk_y != chunk_y) {
    cached.chunk = GenerateTerrainChunk(*rule_, run_seed_, chunk_x, chunk_y);
    cached.filled = true;
    ++chunks_generated_;
  }
  return cached.chunk;
}

bool SurvivorsGround::GeneratedPassable(std::int32_t x, std::int32_t y) const {
  if (rule_ == nullptr || !InsideWorld(x) || !InsideWorld(y)) return false;
  const TerrainChunk& chunk = Cached(x / kTerrainChunkCells, y / kTerrainChunkCells);
  return chunk.Passable(x % kTerrainChunkCells, y % kTerrainChunkCells);
}

const TerrainChunk* SurvivorsGround::ChunkAt(std::int32_t chunk_x, std::int32_t chunk_y) const {
  if (bounded_ || rule_ == nullptr || !InsideWorldChunks(chunk_x) || !InsideWorldChunks(chunk_y)) {
    return nullptr;
  }
  return &Cached(chunk_x, chunk_y);
}

}  // namespace ro::sim
