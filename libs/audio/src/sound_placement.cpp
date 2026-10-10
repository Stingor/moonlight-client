#include "ro/audio/sound_placement.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace ro::audio {
namespace {

float Dot(const std::array<float, 3>& left, const std::array<float, 3>& right) {
  return left[0] * right[0] + left[1] * right[1] + left[2] * right[2];
}

// L'indice de l'axe VERTICAL, celui que la distance ignore.
constexpr std::size_t kUpAxis = 1;

}  // namespace

SoundPlacement PlaceSound(const std::array<float, 3>& listener,
                          const std::array<float, 3>& listener_right,
                          const std::array<float, 3>& source) {
  return PlaceSound(listener, listener_right, source, kHearingRange, kNearDistance);
}

SoundPlacement PlaceSound(const std::array<float, 3>& listener,
                          const std::array<float, 3>& listener_right,
                          const std::array<float, 3>& source, float hearing_range,
                          float near_distance) {
  // L'écart AU SOL : la hauteur n'entre pas dans la distance.
  std::array<float, 3> offset = {source[0] - listener[0], source[1] - listener[1],
                                 source[2] - listener[2]};
  offset[kUpAxis] = 0.0F;
  const float distance = std::sqrt(Dot(offset, offset));

  SoundPlacement placement;
  if (distance >= hearing_range) {
    return placement;
  }
  placement.gain = distance <= near_distance ? 1.0F : near_distance / distance;

  // Le côté : la part de l'écart qui va vers la DROITE de l'auditeur, rapportée
  // à la distance. Un son sur place ne panoramique pas.
  if (distance > 0.0F) {
    placement.pan = std::clamp(Dot(offset, listener_right) / distance, -1.0F, 1.0F);
  }
  return placement;
}

}  // namespace ro::audio
