// L'élément d'attaque des joueurs en vue. Voir `Actor::attack_element`.

#include <cstdint>

#include "ro/sim/world.h"

namespace ro::sim {

bool World::TellAttackElement(std::uint32_t id, Element element) {
  const auto found = actors_.find(id);
  if (found == actors_.end()) {
    return false;
  }
  found->second.attack_element = element;
  return true;
}

}  // namespace ro::sim
