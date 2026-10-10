#include "ro/render/effect_four_particle_slots.h"

#include <array>

#include "ro/render/effect_track.h"

namespace ro::render {
namespace {

// LA PARTICULE QUI VIT DANS L'ANCRE plutôt que dans la queue.
constexpr std::size_t kParticleInAnchor = 1;

// Où les trois autres commencent dans la queue. La case de celle de l'ancre ne
// sert pas ; elle vaut zéro pour que la table reste lisible.
constexpr std::array<std::size_t, kEffectFourParticles> kTailSlots = {3, 0, 6, 0};

}  // namespace

glm::vec3 EffectFourParticleOffsetOf(const EffectTrack& track, std::size_t particle) {
  if (particle == kParticleInAnchor) {
    return track.anchor;
  }
  const std::size_t base = kTailSlots[particle];
  return {track.tail[base], track.tail[base + 1], track.tail[base + 2]};
}

void SetEffectFourParticleOffset(EffectTrack& track, std::size_t particle, glm::vec3 offset) {
  if (particle == kParticleInAnchor) {
    track.anchor = offset;
    return;
  }
  const std::size_t base = kTailSlots[particle];
  track.tail[base] = offset.x;
  track.tail[base + 1] = offset.y;
  track.tail[base + 2] = offset.z;
}

}  // namespace ro::render
