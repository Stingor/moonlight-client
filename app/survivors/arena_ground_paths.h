#pragma once

#include <cstdint>

#include "survivors/arena_ground_look.h"

namespace moonclient {

// LES CARREAUX DES CHEMINS : de quatre coins de cube, de terre ou d'herbe, le
// carreau qui les raccorde et son quart de tour (`GroundTextureRole`, ADR 0134).
// Des fonctions pures, sans fenêtre : le plan du sol les applique cube par cube.

// Les coins de terre d'un cube, un bit chacun.
inline constexpr std::uint8_t kDirtSouthWest = 1U;
inline constexpr std::uint8_t kDirtSouthEast = 2U;
inline constexpr std::uint8_t kDirtNorthWest = 4U;
inline constexpr std::uint8_t kDirtNorthEast = 8U;
inline constexpr std::uint8_t kDirtCorners =
    kDirtSouthWest | kDirtSouthEast | kDirtNorthWest | kDirtNorthEast;

// Les coins après un quart de tour dans le sens trigonométrique, vu du ciel :
// le sud-ouest passe au sud-est, le sud-est au nord-est, et ainsi de suite.
[[nodiscard]] std::uint8_t TurnDirtCorners(std::uint8_t corners) noexcept;

// UN CARREAU : son rôle, et le quart de tour qui pose ses coins de terre sur
// ceux du cube. Aucun coin : l'herbe. Deux coins opposés : la terre pleine.
struct PathTile {
  GroundTextureRole role = GroundTextureRole::kGrass;
  std::uint8_t turn = 0;
  [[nodiscard]] bool operator==(const PathTile&) const = default;
};
[[nodiscard]] PathTile PathTileOf(std::uint8_t dirt_corners) noexcept;

// Le cube (u, v) de la texture, de 0 à 3, que montre le cube (`quarter_east`,
// `quarter_north`) d'un motif de 4 × 4 cubes posé avec `turn` quarts de tour.
struct TextureCube {
  std::uint8_t u = 0;
  std::uint8_t v = 0;
  [[nodiscard]] bool operator==(const TextureCube&) const = default;
};
[[nodiscard]] TextureCube TextureCubeOf(std::uint8_t quarter_east, std::uint8_t quarter_north,
                                        std::uint8_t turn) noexcept;
// L'inverse : la place dans le motif qui montre le cube `shown` de la texture.
struct MotifCube {
  std::uint8_t quarter_east = 0;
  std::uint8_t quarter_north = 0;
  [[nodiscard]] bool operator==(const MotifCube&) const = default;
};
[[nodiscard]] MotifCube MotifCubeShowing(TextureCube shown, std::uint8_t turn) noexcept;

}  // namespace moonclient
