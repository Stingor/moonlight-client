#include "ro/render/sound_chance.h"

#include "ro/render/effect_node.h"

namespace ro::render {

bool SoundChance::Heard(std::uint32_t one_in) {
  if (one_in <= 1) {
    return true;
  }
  // Le bruit sonne quand le reste est nul, comme chez le client.
  return NextEffectNodeDraw(generator_) % one_in == 0;
}

}  // namespace ro::render
