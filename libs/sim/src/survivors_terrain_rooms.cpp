#include <algorithm>
#include <cstddef>
#include <cstdint>

#include "ro/sim/survivors_terrain.h"

#include "survivors_terrain_detail.h"

namespace ro::sim::terrain_detail {
namespace {

// ── LES SALLES ET LES COULOIRS (voir kTerrainWallCells).

// « SURVROOM » pour la sorte d'un morceau, « SURVDOOR » pour la place d'une
// porte, « SURVHALL » pour un bord ouvert, « SURVHUB » pour le carrefour d'un
// bloc de couloirs.
constexpr std::uint64_t kRoomSalt = 0x5355'5256'524F'4F4DULL;
constexpr std::uint64_t kDoorSalt = 0x5355'5256'444F'4F52ULL;
constexpr std::uint64_t kHallSalt = 0x5355'5256'4841'4C4CULL;
constexpr std::uint64_t kHubSalt = 0x5355'5256'4855'4220ULL;
// Le bord d'un morceau se mélange à son côté.
constexpr std::uint64_t kWestEdge = 0;
constexpr std::uint64_t kSouthEdge = 1;
constexpr std::int32_t kSide = kTerrainChunkCells;

}  // namespace

bool HasRooms(const TerrainRule& rule) noexcept { return rule.rooms && rule.door_cells > 0; }

// Ni morceau à repère, ni à moins d'un morceau de la clairière.
bool IsCorridorBlock(const TerrainRule& rule, std::uint64_t run_seed, std::int64_t chunk_x,
                     std::int64_t chunk_y) {
  const std::int64_t west = chunk_x * kSide;
  const std::int64_t south = chunk_y * kSide;
  if (DistanceFromStart(west, south, west + kSide - 1, south + kSide - 1) <
      std::int64_t{rule.clearing_cells} + kSide) {
    return false;
  }
  if (CarriesLandmark(rule, run_seed, chunk_x, chunk_y)) return false;
  return Mix(run_seed, kRoomSalt, chunk_x, chunk_y) % kTerrainPerMille < rule.corridor_per_mille;
}

namespace {

// Vrai si le bord ouest (ou sud) du morceau s'ouvre : entre deux salles.
[[nodiscard]] bool EdgeOpen(const TerrainRule& rule, std::uint64_t run_seed, std::int64_t chunk_x,
                            std::int64_t chunk_y, std::uint64_t edge) {
  const std::int64_t other_x = edge == kWestEdge ? chunk_x - 1 : chunk_x;
  const std::int64_t other_y = edge == kWestEdge ? chunk_y : chunk_y - 1;
  if (IsCorridorBlock(rule, run_seed, chunk_x, chunk_y) ||
      IsCorridorBlock(rule, run_seed, other_x, other_y)) {
    return false;
  }
  return MixStep(Mix(run_seed, kHallSalt, chunk_x, chunk_y), edge) % kTerrainPerMille <
         rule.hall_per_mille;
}

// Un rang pair de [kTerrainWallCells, kSide − kTerrainWallCells − width],
// tiré de `mixed` : une porte ou un carrefour, aligné sur les cubes du sol.
[[nodiscard]] std::int32_t EvenRankOf(std::uint64_t mixed, std::int32_t width) noexcept {
  const auto places = static_cast<std::uint64_t>((kSide - 2 * kTerrainWallCells - width) / 2 + 1);
  return kTerrainWallCells + 2 * static_cast<std::int32_t>(mixed % places);
}

// Le premier rang de la porte du bord ouest (ou sud) du morceau.
[[nodiscard]] std::int32_t DoorOf(const TerrainRule& rule, std::uint64_t run_seed,
                                  std::int64_t chunk_x, std::int64_t chunk_y, std::uint64_t edge) {
  return EvenRankOf(MixStep(Mix(run_seed, kDoorSalt, chunk_x, chunk_y), edge), rule.door_cells);
}

// Les rectangles du morceau, [x0, x1) × [y0, y1) en cases locales.
struct RoomCanvas {
  TerrainChunk& chunk;

  void Mark(std::int32_t x0, std::int32_t y0, std::int32_t x1, std::int32_t y1,
            std::uint8_t passable, std::uint8_t path, std::uint8_t wall) const {
    for (std::int32_t y = std::max(0, y0); y < std::min(kSide, y1); ++y) {
      for (std::int32_t x = std::max(0, x0); x < std::min(kSide, x1); ++x) {
        const std::size_t cell = LocalCell(x, y);
        chunk.passable[cell] = passable;
        chunk.path[cell] = path;
        chunk.wall[cell] = wall;
      }
    }
  }
  void Wall(std::int32_t x0, std::int32_t y0, std::int32_t x1, std::int32_t y1) const {
    Mark(x0, y0, x1, y1, 0U, 0U, 1U);
  }
  void Corridor(std::int32_t x0, std::int32_t y0, std::int32_t x1, std::int32_t y1) const {
    Mark(x0, y0, x1, y1, 1U, 1U, 0U);
  }
};

// Les portes du pourtour d'un morceau : ouest et sud sont les siennes, est et
// nord celles de ses voisins.
struct Doors {
  std::int32_t west = 0;
  std::int32_t south = 0;
  std::int32_t east = 0;
  std::int32_t north = 0;
};

// Un bloc de couloirs : un mur plein, un couloir droit de chaque porte
// jusqu'à l'axe du carrefour, et la croix du carrefour qui les joint.
void PaintCorridorBlock(const TerrainRule& rule, std::uint64_t run_seed, const RoomCanvas& canvas,
                        const Doors& doors) {
  const std::int32_t width = rule.door_cells;
  const std::uint64_t hub = Mix(run_seed, kHubSalt, canvas.chunk.chunk_x, canvas.chunk.chunk_y);
  const std::int32_t hub_x = EvenRankOf(hub, width);
  const std::int32_t hub_y = EvenRankOf(hub >> kHalfWordBits, width);
  canvas.Wall(0, 0, kSide, kSide);
  canvas.Corridor(0, doors.west, hub_x + width, doors.west + width);
  canvas.Corridor(hub_x, doors.east, kSide, doors.east + width);
  canvas.Corridor(doors.south, 0, doors.south + width, hub_y + width);
  canvas.Corridor(doors.north, hub_y, doors.north + width, kSide);
  canvas.Corridor(hub_x, std::min({doors.west, doors.east, hub_y}), hub_x + width,
                  std::max({doors.west, doors.east, hub_y}) + width);
  canvas.Corridor(std::min({doors.south, doors.north, hub_x}), hub_y,
                  std::max({doors.south, doors.north, hub_x}) + width, hub_y + width);
}

// Une salle : ses murs ouest et sud et leurs portes, sauf sur un bord ouvert,
// et le parvis de chaque porte de son pourtour.
void PaintRoom(const TerrainRule& rule, std::uint64_t run_seed, const RoomCanvas& canvas,
               const Doors& doors) {
  const std::int64_t chunk_x = canvas.chunk.chunk_x;
  const std::int64_t chunk_y = canvas.chunk.chunk_y;
  const std::int32_t width = rule.door_cells;
  const bool west_open = EdgeOpen(rule, run_seed, chunk_x, chunk_y, kWestEdge);
  const bool south_open = EdgeOpen(rule, run_seed, chunk_x, chunk_y, kSouthEdge);
  if (!west_open) canvas.Wall(0, 0, kTerrainWallCells, kSide);
  if (!south_open) canvas.Wall(0, 0, kSide, kTerrainWallCells);
  if (west_open && south_open) canvas.Wall(0, 0, kTerrainWallCells, kTerrainWallCells);
  if (!west_open) canvas.Corridor(0, doors.west, kTerrainApronCells, doors.west + width);
  if (!south_open) canvas.Corridor(doors.south, 0, doors.south + width, kTerrainApronCells);
  if (!EdgeOpen(rule, run_seed, chunk_x + 1, chunk_y, kWestEdge)) {
    canvas.Corridor(kSide - kTerrainApronCells, doors.east, kSide, doors.east + width);
  }
  if (!EdgeOpen(rule, run_seed, chunk_x, chunk_y + 1, kSouthEdge)) {
    canvas.Corridor(doors.north, kSide - kTerrainApronCells, doors.north + width, kSide);
  }
}

}  // namespace

// Les murs, les portes et les couloirs du morceau ; puis la clairière, qui ne
// garde aucun cube de mur dont une case l'atteint.
void PaintRooms(const TerrainRule& rule, std::uint64_t run_seed, TerrainChunk& chunk) {
  if (!HasRooms(rule)) return;
  const std::int64_t chunk_x = chunk.chunk_x;
  const std::int64_t chunk_y = chunk.chunk_y;
  const Doors doors{.west = DoorOf(rule, run_seed, chunk_x, chunk_y, kWestEdge),
                    .south = DoorOf(rule, run_seed, chunk_x, chunk_y, kSouthEdge),
                    .east = DoorOf(rule, run_seed, chunk_x + 1, chunk_y, kWestEdge),
                    .north = DoorOf(rule, run_seed, chunk_x, chunk_y + 1, kSouthEdge)};
  const RoomCanvas canvas{.chunk = chunk};
  if (IsCorridorBlock(rule, run_seed, chunk_x, chunk_y)) {
    PaintCorridorBlock(rule, run_seed, canvas, doors);
  } else {
    PaintRoom(rule, run_seed, canvas, doors);
  }
  const std::int64_t west = chunk_x * kSide;
  const std::int64_t south = chunk_y * kSide;
  for (std::int32_t y = 0; y < kSide; y += kTerrainWallCells) {
    for (std::int32_t x = 0; x < kSide; x += kTerrainWallCells) {
      if (DistanceFromStart(west + x, south + y, west + x + kTerrainWallCells - 1,
                            south + y + kTerrainWallCells - 1) >= rule.clearing_cells ||
          chunk.wall[LocalCell(x, y)] == 0U) {
        continue;
      }
      for (std::int32_t dy = 0; dy < kTerrainWallCells; ++dy) {
        for (std::int32_t dx = 0; dx < kTerrainWallCells; ++dx) {
          const std::size_t cell = LocalCell(x + dx, y + dy);
          chunk.wall[cell] = 0U;
          chunk.passable[cell] = 1U;
        }
      }
    }
  }
}

}  // namespace ro::sim::terrain_detail
