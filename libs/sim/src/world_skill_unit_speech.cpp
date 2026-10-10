// CE QU'UNE UNITÉ AU SOL DIT : la part du monde qui note le message d'une
// Talkie Box.
//
// Elle vit à part de `world.cpp` pour tenir le plafond de huit cents lignes ;
// cette méthode ne touche que l'unité qu'elle nomme.

#include <cstdint>
#include <string>
#include <utility>

#include "ro/sim/world.h"

namespace ro::sim {

bool World::SaySkillUnit(std::uint32_t id, std::string text, std::uint32_t now_ms) {
  const auto unit = skill_units_.find(id);
  if (unit == skill_units_.end()) {
    return false;
  }
  unit->second.said = std::move(text);
  unit->second.said_since_ms = now_ms;
  return true;
}

}  // namespace ro::sim
