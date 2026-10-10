// LES PROTÉGÉS DE DEVOTION d'un acteur : la part du monde qui les note.
//
// Elle vit à part de `world.cpp` pour tenir le plafond de huit cents lignes.

#include <array>
#include <cstdint>

#include "ro/sim/world.h"

namespace ro::sim {

bool World::TellDevotion(std::uint32_t devoter_id,
                         const std::array<std::uint32_t, kDevotionSlots>& devotee_ids) {
  const auto found = actors_.find(devoter_id);
  // ⚠ UN CROISÉ HORS DE VUE N'EST PAS INVENTÉ : le client ne tend aucun trait
  // depuis un acteur qu'il ne connaît pas, et ne défait rien non plus.
  if (found == actors_.end()) {
    return false;
  }
  DevotionList& devotion = found->second.devotion;
  devotion.devotee_ids = devotee_ids;
  devotion.told_serial = ++devotion_serial_;
  return true;
}

}  // namespace ro::sim
