// LE DÉLAI DE COMBO d'un acteur : la part du monde qui le note.
//
// Elle vit à part de `world.cpp` pour tenir le plafond de huit cents lignes ;
// elle ne parle qu'aux deux champs de l'acteur qui le portent.

#include <cstdint>

#include "ro/sim/world.h"

namespace ro::sim {

bool World::RecordComboDelay(std::uint32_t id, std::uint32_t delay_ms, std::uint32_t now_ms) {
  const auto found = actors_.find(id);
  if (found == actors_.end()) {
    return false;
  }
  found->second.combo_delay_ms = delay_ms;
  found->second.combo_delay_since_ms = now_ms;
  return true;
}

}  // namespace ro::sim
