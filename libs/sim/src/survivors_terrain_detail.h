#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>

#include "ro/sim/survivors_terrain.h"

// CE QUE PARTAGENT LES FICHIERS DU TERRAIN GÉNÉRÉ : les mélanges de la graine,
// la case locale d'un morceau, la distance au départ et le morceau à repère.
// Interne à `sim`.
namespace ro::sim::terrain_detail {

// Les constantes publiées de SplitMix64 : l'incrément (partie fractionnaire du
// nombre d'or) et les deux multiplicateurs du finaliseur.
inline constexpr std::uint64_t kSplitMixGamma = 0x9E37'79B9'7F4A'7C15ULL;
inline constexpr std::uint64_t kSplitMixFirstMultiplier = 0xBF58'476D'1CE4'E5B9ULL;
inline constexpr std::uint64_t kSplitMixSecondMultiplier = 0x94D0'49BB'1331'11EBULL;
inline constexpr unsigned kSplitMixFirstShift = 30;
inline constexpr unsigned kSplitMixSecondShift = 27;
inline constexpr unsigned kSplitMixLastShift = 31;
// Un mélange donne deux décalages, le morceau à repère comme le carrefour : x
// est le reste du mot entier de 64 bits, y celui de sa seule moitié haute.
inline constexpr unsigned kHalfWordBits = 32;

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
[[nodiscard]] constexpr std::int64_t DistanceFromStart(std::int64_t west, std::int64_t south,
                                                       std::int64_t east,
                                                       std::int64_t north) noexcept {
  const std::int64_t dx =
      std::max({west - kTerrainStartCell, std::int64_t{0}, std::int64_t{kTerrainStartCell} - east});
  const std::int64_t dy = std::max(
      {south - kTerrainStartCell, std::int64_t{0}, std::int64_t{kTerrainStartCell} - north});
  return std::max(dx, dy);
}

[[nodiscard]] constexpr std::size_t LocalCell(std::int32_t local_x, std::int32_t local_y) noexcept {
  return static_cast<std::size_t>(local_y) * static_cast<std::size_t>(kTerrainChunkCells) +
         static_cast<std::size_t>(local_x);
}

// Vrai si ce morceau porte un repère : l'élu de son carré, hors de la
// clairière ; avec des chemins, le seul morceau de place de sa cellule.
[[nodiscard]] bool CarriesLandmark(const TerrainRule& rule, std::uint64_t run_seed,
                                   std::int64_t chunk_x, std::int64_t chunk_y) noexcept;

// LES SALLES (`survivors_terrain_rooms.cpp`).
[[nodiscard]] bool HasRooms(const TerrainRule& rule) noexcept;
// Vrai si le morceau est un bloc de couloirs, qui ne porte aucune pièce.
[[nodiscard]] bool IsCorridorBlock(const TerrainRule& rule, std::uint64_t run_seed,
                                   std::int64_t chunk_x, std::int64_t chunk_y);
// Peint les murs, les portes, les parvis et les couloirs du morceau.
void PaintRooms(const TerrainRule& rule, std::uint64_t run_seed, TerrainChunk& chunk);

}  // namespace ro::sim::terrain_detail
