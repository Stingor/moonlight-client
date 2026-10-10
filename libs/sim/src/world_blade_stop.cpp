// BLADE STOP : la part du monde qui note la prise d'une lame.
//
// Le moine saisit la lame de sa cible : les deux se figent, grisés, tant que la
// prise dure. Elle vit à part de `world.cpp` pour tenir le plafond de huit cents
// lignes.

#include <cstdint>

#include "ro/sim/world.h"

namespace ro::sim {

bool World::SetBladeStop(std::uint32_t caster_id, std::uint32_t target_id, bool active,
                         std::uint32_t now_ms) {
  const auto caster = actors_.find(caster_id);
  const auto target = actors_.find(target_id);
  const bool caster_known = caster != actors_.end();
  const bool target_known = target != actors_.end();
  if (active) {
    // 🔴 UNE PRISE NE SE POSE QU'ENTRE DEUX ACTEURS CONNUS : sans l'un, l'autre
    // ne se fige pas.
    if (!caster_known || !target_known) {
      return false;
    }
    caster->second.blade_stop_since_ms = now_ms;
    caster->second.blade_stop_caster = true;
    caster->second.blade_stop_released_ms.reset();
    target->second.blade_stop_since_ms = now_ms;
    target->second.blade_stop_caster = false;
    return true;
  }
  // Le relâchement libère chacun de ceux qu'on connaît, indépendamment.
  //
  // ⚠ LE MOINE GARDE SA POSE jusqu'à sa motion suivante : le relâchement ne fait
  // qu'échoir la pose menée, dont l'image reste. On note l'instant, et le côté.
  if (caster_known) {
    if (caster->second.blade_stop_since_ms.has_value() && caster->second.blade_stop_caster) {
      caster->second.blade_stop_released_ms = now_ms;
    }
    caster->second.blade_stop_since_ms.reset();
  }
  if (target_known) {
    target->second.blade_stop_since_ms.reset();
    target->second.blade_stop_caster = false;
  }
  return caster_known || target_known;
}

}  // namespace ro::sim
