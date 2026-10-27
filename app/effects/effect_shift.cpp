// LE MONDE QUI SE DÉPLACE SOUS LES EFFETS : une scène dont l'origine glisse
// (le sol généré de l'arène se recentre par pas entiers de cases) déplace
// d'autant chaque point du monde que les effets tiennent, et chaque case de leur
// monde. Ce qui est refait à chaque image — quads couchés, placements, lots —
// n'a rien à garder ; seul ce qui dure d'une image à l'autre se déplace ici.

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

#include "ro/sim/packed_position.h"

#include "effects/effect_view.h"

namespace moonclient {
namespace {

// La case `cell` déplacée de `cell_delta`, ou rien quand elle sort du monde.
[[nodiscard]] std::optional<ro::sim::CellPosition> ShiftedCell(ro::sim::CellPosition cell,
                                                               glm::ivec2 cell_delta) {
  const std::int32_t x = static_cast<std::int32_t>(cell.x) + cell_delta.x;
  const std::int32_t y = static_cast<std::int32_t>(cell.y) + cell_delta.y;
  constexpr std::int32_t kLastCell = std::numeric_limits<std::uint16_t>::max();
  if (x < 0 || y < 0 || x > kLastCell || y > kLastCell) return std::nullopt;
  return ro::sim::CellPosition{static_cast<std::uint16_t>(x), static_cast<std::uint16_t>(y),
                               cell.direction};
}

}  // namespace

void EffectsInPlay::Shift(glm::vec3 world_delta, glm::ivec2 cell_delta) {
  for (Playing& playing : per_map_.playing) playing.ground_position += world_delta;
  for (auto& [place, bearer] : per_map_.chimneys) place += world_delta;
  for (LaunchedEffect& launched : per_map_.just_launched) {
    for (std::size_t axis = 0; axis < launched.ground.size(); ++axis) {
      launched.ground[axis] += world_delta[static_cast<glm::length_t>(axis)];
    }
  }
  for (ShowerArrow& arrow : per_map_.shower_arrows) {
    arrow.from_ground += world_delta;
    arrow.to_ground += world_delta;
  }
  // LES CASES DU MONDE : ce qui en sort n'a plus de place où se semer.
  std::erase_if(per_map_.just_sown_named_sounds, [cell_delta](SownNamedSound& sound) {
    const std::optional<ro::sim::CellPosition> moved = ShiftedCell(sound.cell, cell_delta);
    if (moved.has_value()) sound.cell = *moved;
    return !moved.has_value();
  });
  std::erase_if(per_map_.pending_named, [cell_delta](PendingNamed& pending) {
    const std::optional<ro::sim::CellPosition> moved = ShiftedCell(pending.cell, cell_delta);
    if (moved.has_value()) pending.cell = *moved;
    return !moved.has_value();
  });
  std::erase_if(per_map_.pending_hits, [cell_delta](PendingHit& pending) {
    if (!pending.unit_cell.has_value()) return false;
    pending.unit_cell = ShiftedCell(*pending.unit_cell, cell_delta);
    return !pending.unit_cell.has_value();
  });
  std::erase_if(per_map_.sown_units, [cell_delta](auto& entry) {
    const std::optional<ro::sim::CellPosition> moved = ShiftedCell(entry.second.cell, cell_delta);
    if (moved.has_value()) entry.second.cell = *moved;
    return !moved.has_value();
  });
  classes_.Shift(world_delta);
}

}  // namespace moonclient
