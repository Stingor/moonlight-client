#include "survivors/arena_ground_paths.h"

#include <array>

namespace moonclient {
namespace {

constexpr std::uint8_t kQuarterTurns = 4;
// Le plus grand rang d'un cube dans son motif de 4 × 4 cubes.
constexpr std::uint8_t kLastQuarter = 3;

// LES CARREAUX À LEUR QUART DE TOUR NUL, comme `GroundTextureRole` les décrit.
struct BaseTile {
  GroundTextureRole role;
  std::uint8_t corners;
};
constexpr std::array<BaseTile, 6> kBaseTiles{{
    {.role = GroundTextureRole::kPathFull, .corners = kDirtCorners},
    {.role = GroundTextureRole::kPathEdge, .corners = kDirtSouthWest | kDirtNorthWest},
    {.role = GroundTextureRole::kPathOuter, .corners = kDirtNorthWest},
    {.role = GroundTextureRole::kPathInner,
     .corners = kDirtSouthEast | kDirtNorthWest | kDirtNorthEast},
    // Deux coins opposés : aucun carreau ne les raccorde, la terre les couvre.
    {.role = GroundTextureRole::kPathFull, .corners = kDirtSouthWest | kDirtNorthEast},
    {.role = GroundTextureRole::kPathFull, .corners = kDirtSouthEast | kDirtNorthWest},
}};

}  // namespace

std::uint8_t TurnDirtCorners(std::uint8_t corners) noexcept {
  std::uint8_t turned = 0;
  if ((corners & kDirtSouthWest) != 0) turned |= kDirtSouthEast;
  if ((corners & kDirtSouthEast) != 0) turned |= kDirtNorthEast;
  if ((corners & kDirtNorthEast) != 0) turned |= kDirtNorthWest;
  if ((corners & kDirtNorthWest) != 0) turned |= kDirtSouthWest;
  return turned;
}

PathTile PathTileOf(std::uint8_t dirt_corners) noexcept {
  const std::uint8_t wanted = dirt_corners & kDirtCorners;
  if (wanted == 0) return PathTile{};
  for (const BaseTile& base : kBaseTiles) {
    std::uint8_t corners = base.corners;
    for (std::uint8_t turn = 0; turn < kQuarterTurns; ++turn) {
      if (corners == wanted) return PathTile{.role = base.role, .turn = turn};
      corners = TurnDirtCorners(corners);
    }
  }
  return PathTile{.role = GroundTextureRole::kPathFull};
}

// Le motif tourné se lit comme le pose le sol dessiné : un quart de tour
// envoie l'est du motif vers les v décroissants et le nord vers les u
// croissants.
TextureCube TextureCubeOf(std::uint8_t quarter_east, std::uint8_t quarter_north,
                          std::uint8_t turn) noexcept {
  const auto flip = [](std::uint8_t quarter) {
    return static_cast<std::uint8_t>(kLastQuarter - quarter);
  };
  switch (turn % kQuarterTurns) {
    case 1:
      return TextureCube{.u = quarter_north, .v = flip(quarter_east)};
    case 2:
      return TextureCube{.u = flip(quarter_east), .v = flip(quarter_north)};
    case 3:
      return TextureCube{.u = flip(quarter_north), .v = quarter_east};
    default:
      return TextureCube{.u = quarter_east, .v = quarter_north};
  }
}

MotifCube MotifCubeShowing(TextureCube shown, std::uint8_t turn) noexcept {
  const auto flip = [](std::uint8_t quarter) {
    return static_cast<std::uint8_t>(kLastQuarter - quarter);
  };
  switch (turn % kQuarterTurns) {
    case 1:
      return MotifCube{.quarter_east = flip(shown.v), .quarter_north = shown.u};
    case 2:
      return MotifCube{.quarter_east = flip(shown.u), .quarter_north = flip(shown.v)};
    case 3:
      return MotifCube{.quarter_east = shown.v, .quarter_north = flip(shown.u)};
    default:
      return MotifCube{.quarter_east = shown.u, .quarter_north = shown.v};
  }
}

}  // namespace moonclient
