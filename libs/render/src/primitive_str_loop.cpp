#include "ro/render/primitive_str_loop.h"

#include <algorithm>

#include "ro/render/effect_class_registry.h"

namespace ro::render {

bool PrimitiveStrLoopsForLife(std::uint32_t effect_id) {
  // UNE CLASSE INSCRITE PASSE DEVANT le répartiteur des primitifs : son cas
  // n'y est jamais atteint.
  if (ClientRegistersEffectClass(effect_id)) {
    return false;
  }
  return std::ranges::binary_search(PrimitiveStrLoopTable(), effect_id, {},
                                    [](std::uint16_t id) { return std::uint32_t{id}; });
}

}  // namespace ro::render
