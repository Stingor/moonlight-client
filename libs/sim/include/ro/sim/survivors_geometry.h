#pragma once

#include <cstdint>

namespace ro::sim {

// UNE CASE = 1024 SOUS-UNITÉS. Positions en int32, distances au carré en int64 :
// aucun flottant, pour que MSVC et g++ rendent le même état.
inline constexpr std::int32_t kSubunitsPerCell = 1024;
inline constexpr std::uint32_t kSurvivorsStepMs = 20;
inline constexpr std::uint32_t kStepsPerSecond = 1000 / kSurvivorsStepMs;
inline constexpr std::uint32_t kStepsPerMinute = 60 * kStepsPerSecond;
// Un cap sur 64 valeurs : 0 au nord, dans le sens trigonométrique ; la direction
// serveur d vaut le cap 8 d.
inline constexpr std::uint8_t kHeadingSteps = 64;
inline constexpr std::uint8_t kHeadingsPerDirection = kHeadingSteps / 8;
inline constexpr std::uint8_t kNoHeading = 0xFF;

// En sous-unités ; y vers le NORD, comme WalkMap.
struct FieldPoint {
  std::int32_t x = 0;
  std::int32_t y = 0;
  [[nodiscard]] friend bool operator==(const FieldPoint&, const FieldPoint&) = default;
};

[[nodiscard]] constexpr FieldPoint operator+(FieldPoint a, FieldPoint b) noexcept {
  return {a.x + b.x, a.y + b.y};
}
[[nodiscard]] constexpr FieldPoint operator-(FieldPoint a, FieldPoint b) noexcept {
  return {a.x - b.x, a.y - b.y};
}

[[nodiscard]] constexpr std::int64_t DistanceSquared(FieldPoint a, FieldPoint b) noexcept {
  const std::int64_t dx = static_cast<std::int64_t>(a.x) - b.x;
  const std::int64_t dy = static_cast<std::int64_t>(a.y) - b.y;
  return dx * dx + dy * dy;
}

// Racine entière par défaut : r * r <= value < (r + 1) * (r + 1). Chiffre par
// chiffre en base 4, sans division : exacte sur tout uint64.
[[nodiscard]] constexpr std::uint32_t IntegerSqrt(std::uint64_t value) noexcept {
  std::uint64_t root = 0;
  std::uint64_t digit = std::uint64_t{1} << 62U;
  while (digit > value) digit >>= 2U;
  while (digit != 0) {
    if (value >= root + digit) {
      value -= root + digit;
      root = (root >> 1U) + digit;
    } else {
      root >>= 1U;
    }
    digit >>= 2U;
  }
  return static_cast<std::uint32_t>(root);
}

// Division PLANCHER par kSubunitsPerCell : -1 est dans la case -1.
[[nodiscard]] constexpr std::int32_t CellOf(std::int32_t subunits) noexcept {
  const std::int64_t value = subunits;
  const std::int64_t floored =
      value >= 0 ? value / kSubunitsPerCell : (value - (kSubunitsPerCell - 1)) / kSubunitsPerCell;
  return static_cast<std::int32_t>(floored);
}

[[nodiscard]] constexpr FieldPoint CellCenter(std::int32_t cell_x, std::int32_t cell_y) noexcept {
  return {cell_x * kSubunitsPerCell + kSubunitsPerCell / 2,
          cell_y * kSubunitsPerCell + kSubunitsPerCell / 2};
}

[[nodiscard]] constexpr std::int32_t Cells(std::int32_t cells) noexcept {
  return cells * kSubunitsPerCell;
}

// Des dixièmes de case : tenths * 1024 / 10, tronqué.
[[nodiscard]] constexpr std::int32_t CellTenths(std::int32_t tenths) noexcept {
  constexpr std::int32_t kTenthsPerCell = 10;
  return tenths * kSubunitsPerCell / kTenthsPerCell;
}

// Sous-unités par pas pour une marche en ms par case ; 0 pour 0 (immobile).
[[nodiscard]] constexpr std::int32_t UnitsPerStep(std::uint32_t ms_per_cell) noexcept {
  if (ms_per_cell == 0) return 0;
  return static_cast<std::int32_t>(std::uint64_t{kSubunitsPerCell} * kSurvivorsStepMs /
                                   ms_per_cell);
}

// Nombre de pas d'une durée, arrondi au-dessus : 0 → 0, 1 → 1, 20 → 1, 21 → 2.
[[nodiscard]] constexpr std::uint32_t StepsOf(std::uint32_t ms) noexcept {
  return static_cast<std::uint32_t>((std::uint64_t{ms} + kSurvivorsStepMs - 1) / kSurvivorsStepMs);
}

// Vecteur de `length` sous-unités au cap `heading`, pris modulo 64, par la
// table écrite : le cap 0 pointe au nord (0, length), le cap 16 à l'ouest.
[[nodiscard]] FieldPoint TurnVector(std::uint8_t heading, std::int32_t length) noexcept;
// Le cap le plus proche d'un vecteur (plus grand produit scalaire avec
// TurnVector(h, 1024), le plus petit h à égalité) ; `fallback` pour un vecteur nul.
[[nodiscard]] std::uint8_t HeadingOf(FieldPoint delta, std::uint8_t fallback) noexcept;
// Vecteur de `length` sous-unités de `from` vers `to` (delta * length / racine,
// en int64, troncature vers zéro) ; nul s'ils se confondent.
[[nodiscard]] FieldPoint Toward(FieldPoint from, FieldPoint to, std::int32_t length) noexcept;

}  // namespace ro::sim
