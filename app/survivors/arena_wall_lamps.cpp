#include "survivors/arena_wall_lamps.h"

#include <algorithm>
#include <cstddef>

#include "ro/core/pcg32.h"

#include "survivors/arena_ground_relief.h"

namespace moonclient {
namespace {

using ro::sim::kTerrainChunkCells;

// Le sel des lampes, en ASCII : rien de ce qu'il tire ne décide une case.
constexpr std::uint64_t kLampSalt = 0x4C41'4D50'0000'0000ULL;  // « LAMP »
// Le flux du `Pcg32` d'une lampe.
constexpr std::uint64_t kLampStream = 0x0000'004C'414D'5053ULL;
constexpr float kUnitFromDraw = 1.0F / 4294967296.0F;  // 2^-32

constexpr std::int32_t kCubesPerChunk = kTerrainChunkCells / kGroundCellsPerCube;
// Le centre d'un cube, en cases depuis son coin sud-ouest.
constexpr float kCubeCentreCells = static_cast<float>(kGroundCellsPerCube) / 2.0F;
// Au-delà de tant de morceaux retenus, la mémoire repart de zéro : le héros
// marche, et ce qu'il a laissé loin derrière ne se relit plus.
constexpr std::size_t kMostRememberedChunks = 256;

// LA FACE D'UNE LAMPE : elle se tourne vers le sol qu'elle éclaire, au quart
// de tour, dans l'ordre où le cube le borde (sud, nord, est, ouest).
constexpr float kFacingSouthDegrees = 0.0F;
constexpr float kFacingNorthDegrees = 180.0F;
constexpr float kFacingEastDegrees = 90.0F;
constexpr float kFacingWestDegrees = 270.0F;

// Les lampes du laboratoire : le gyrophare des usines d'Einbroch, une
// applique ; la lampe-cage des sous-sols et la lampe d'intérieur, au sol.
constexpr std::array<WallLampLook, 3> kLabLamps{{
    {.model = "아인브로크\\경보등.rsm",
     .scale = 1.0F,
     .weight = 2,
     .lift_world = 10.0F,
     .light_height_world = 14.0F},
    {.model = "내부소품\\지하등1.rsm", .scale = 1.0F, .weight = 1, .light_height_world = 14.0F},
    {.model = "내부소품\\실내등6.rsm", .scale = 1.0F, .weight = 1, .light_height_world = 8.0F},
}};

// Le pas d'une face vers le sol qu'elle regarde, en cubes.
struct FaceStep {
  std::int32_t east = 0;
  std::int32_t north = 0;
};
[[nodiscard]] constexpr FaceStep StepOf(WallFace face) noexcept {
  switch (face) {
    case WallFace::kSouth:
      return {.east = 0, .north = -1};
    case WallFace::kNorth:
      return {.east = 0, .north = 1};
    case WallFace::kEast:
      return {.east = 1, .north = 0};
    case WallFace::kWest:
      return {.east = -1, .north = 0};
  }
  return {};
}

[[nodiscard]] constexpr float YawOf(WallFace face) noexcept {
  switch (face) {
    case WallFace::kSouth:
      return kFacingSouthDegrees;
    case WallFace::kNorth:
      return kFacingNorthDegrees;
    case WallFace::kEast:
      return kFacingEastDegrees;
    case WallFace::kWest:
      return kFacingWestDegrees;
  }
  return kFacingSouthDegrees;
}

[[nodiscard]] std::int32_t FloorDiv(std::int32_t value, std::int32_t divisor) noexcept {
  const std::int32_t quotient = value / divisor;
  return (value % divisor != 0 && (value < 0) != (divisor < 0)) ? quotient - 1 : quotient;
}

[[nodiscard]] std::int32_t FloorMod(std::int32_t value, std::int32_t divisor) noexcept {
  return value - FloorDiv(value, divisor) * divisor;
}

[[nodiscard]] float UnitOf(std::uint32_t draw) noexcept {
  return static_cast<float>(draw) * kUnitFromDraw;
}

}  // namespace

std::span<const WallLampLook> WallLampLooksOf(const BiomeLook& biome) noexcept {
  if (biome.darkness != ArenaDarkness::kBlackout) return {};
  return kLabLamps;
}

glm::vec2 WallLampCell(const WallLamp& lamp) noexcept {
  const glm::vec2 centre(static_cast<float>(lamp.cube_x * kGroundCellsPerCube) + kCubeCentreCells,
                         static_cast<float>(lamp.cube_y * kGroundCellsPerCube) + kCubeCentreCells);
  const FaceStep step = StepOf(lamp.face);
  return centre + glm::vec2(static_cast<float>(step.east), static_cast<float>(step.north)) *
                      (kCubeCentreCells + kWallLampGapCells);
}

WallLampField::WallLampField(const ro::sim::TerrainRule& rule, std::uint64_t run_seed,
                             const BiomeLook& biome)
    : rule_(&rule), run_seed_(run_seed), looks_(WallLampLooksOf(biome)) {
  phase_cubes_ = static_cast<std::int32_t>(VisualMix(run_seed, kLampSalt, 0, 0) %
                                           static_cast<std::uint64_t>(kWallLampSpacingCubes));
}

void WallLampField::Remember(const ro::sim::TerrainChunk& chunk) {
  if (!has_lamps()) return;
  Trim();
  walls_.try_emplace({chunk.chunk_x, chunk.chunk_y}, chunk.wall);
}

const WallLampField::ChunkWalls& WallLampField::WallsOf(std::int32_t chunk_x,
                                                        std::int32_t chunk_y) {
  const auto found = walls_.find({chunk_x, chunk_y});
  if (found != walls_.end()) return found->second;
  return walls_
      .emplace(ChunkKey{chunk_x, chunk_y},
               ro::sim::GenerateTerrainChunk(*rule_, run_seed_, chunk_x, chunk_y).wall)
      .first->second;
}

// Un mur suit les cubes du sol : sa case sud-ouest dit le cube entier.
bool WallLampField::WallCube(std::int32_t cube_x, std::int32_t cube_y) {
  const std::int32_t cell_x = cube_x * kGroundCellsPerCube;
  const std::int32_t cell_y = cube_y * kGroundCellsPerCube;
  const std::int32_t chunk_x = FloorDiv(cell_x, kTerrainChunkCells);
  const std::int32_t chunk_y = FloorDiv(cell_y, kTerrainChunkCells);
  const ChunkWalls& walls = WallsOf(chunk_x, chunk_y);
  const auto local =
      static_cast<std::size_t>(FloorMod(cell_y, kTerrainChunkCells) * kTerrainChunkCells +
                               FloorMod(cell_x, kTerrainChunkCells));
  return walls[local] != 0U;
}

std::vector<WallLamp> WallLampField::PlanChunk(std::int32_t chunk_x, std::int32_t chunk_y) {
  std::vector<WallLamp> lamps;
  std::array<std::uint32_t, kLabLamps.size()> weights{};
  const std::size_t count = std::min(looks_.size(), weights.size());
  for (std::size_t rank = 0; rank < count; ++rank) weights[rank] = looks_[rank].weight;
  for (std::int32_t row = 0; row < kCubesPerChunk; ++row) {
    for (std::int32_t column = 0; column < kCubesPerChunk; ++column) {
      const std::int32_t cube_x = chunk_x * kCubesPerChunk + column;
      const std::int32_t cube_y = chunk_y * kCubesPerChunk + row;
      if (!WallCube(cube_x, cube_y)) continue;
      const bool floor_south = !WallCube(cube_x, cube_y - 1);
      const bool floor_north = !WallCube(cube_x, cube_y + 1);
      const bool floor_east = !WallCube(cube_x + 1, cube_y);
      const bool floor_west = !WallCube(cube_x - 1, cube_y);
      const bool along_x = (floor_south || floor_north) &&
                           FloorMod(cube_x - phase_cubes_, kWallLampSpacingCubes) == 0;
      const bool along_y =
          (floor_east || floor_west) && FloorMod(cube_y - phase_cubes_, kWallLampSpacingCubes) == 0;
      if (!along_x && !along_y) continue;
      WallFace face = WallFace::kWest;
      if (along_x) {
        face = floor_south ? WallFace::kSouth : WallFace::kNorth;
      } else if (floor_east) {
        face = WallFace::kEast;
      }
      // Toujours deux tirages : le modèle, le rang dans la panne.
      ro::core::Pcg32 random(VisualMix(run_seed_, kLampSalt, cube_x, cube_y), kLampStream);
      const std::size_t look = random.PickWeighted(std::span(weights.data(), count));
      const float blackout_rank = UnitOf(random.Next());
      if (look >= count) continue;
      lamps.push_back(WallLamp{.cube_x = cube_x,
                               .cube_y = cube_y,
                               .face = face,
                               .look = static_cast<std::uint8_t>(look),
                               .blackout_rank = blackout_rank,
                               .yaw_degrees = YawOf(face)});
    }
  }
  return lamps;
}

std::span<const WallLamp> WallLampField::LampsOfChunk(std::int32_t chunk_x, std::int32_t chunk_y) {
  if (!has_lamps()) return {};
  Trim();
  const auto found = lamps_.find({chunk_x, chunk_y});
  if (found != lamps_.end()) return found->second;
  std::vector<WallLamp> planned = PlanChunk(chunk_x, chunk_y);
  return lamps_.emplace(ChunkKey{chunk_x, chunk_y}, std::move(planned)).first->second;
}

void WallLampField::LampsOfWindow(GroundWindowOrigin origin, std::vector<WallLamp>& out) {
  out.clear();
  if (!has_lamps()) return;
  constexpr std::int32_t kWindowChunks = kGroundWindowCells / kTerrainChunkCells;
  const std::int32_t first_x = ro::sim::TerrainChunkOf(origin.x_cells);
  const std::int32_t first_y = ro::sim::TerrainChunkOf(origin.y_cells);
  for (std::int32_t row = 0; row < kWindowChunks; ++row) {
    for (std::int32_t column = 0; column < kWindowChunks; ++column) {
      const std::span<const WallLamp> lamps = LampsOfChunk(first_x + column, first_y + row);
      out.insert(out.end(), lamps.begin(), lamps.end());
    }
  }
}

void WallLampField::Trim() {
  if (walls_.size() <= kMostRememberedChunks && lamps_.size() <= kMostRememberedChunks) return;
  walls_.clear();
  lamps_.clear();
}

void AppendWallLampPoses(WallLampField& field, GroundWindowOrigin origin,
                         std::vector<PlannedPose>& poses) {
  const std::span<const WallLampLook> looks = field.looks();
  std::vector<WallLamp> lamps;
  field.LampsOfWindow(origin, lamps);
  for (const WallLamp& lamp : lamps) {
    const glm::vec2 cell = WallLampCell(lamp);
    poses.push_back(PlannedPose{.model = looks[lamp.look].model,
                                .east = cell.x - static_cast<float>(origin.x_cells),
                                .north = cell.y - static_cast<float>(origin.y_cells),
                                .yaw_degrees = lamp.yaw_degrees,
                                .scale = looks[lamp.look].scale,
                                // Une hauteur négative sous le sol : l'applique monte.
                                .sink_units = -looks[lamp.look].lift_world,
                                .piece = kSeedPose});
  }
}

}  // namespace moonclient
