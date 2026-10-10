#include "ro/render/day_night_light.h"

#include <algorithm>
#include <cmath>

namespace ro::render {

namespace {

// La lumière qu'une ambiante et une diffuse donnent ensemble, sur un canal.
float Screened(float ambient, float diffuse) { return 1.0F - (1.0F - ambient) * (1.0F - diffuse); }

}  // namespace

void DayNightLight::Enter(const SceneLight& map_light) {
  map_diffuse_ = map_light.diffuse;
  map_ambient_ = map_light.ambient;
  diffuse_ = map_light.diffuse;
  pending_ms_ = 0.0F;
}

void DayNightLight::Advance(bool night, float elapsed_ms) {
  pending_ms_ += std::max(elapsed_ms, 0.0F);
  auto frames = static_cast<int>(std::floor(pending_ms_ / kDayNightFrameMs));
  if (frames > kMostDayNightFrames) {
    frames = kMostDayNightFrames;
    pending_ms_ = 0.0F;
  } else {
    pending_ms_ -= static_cast<float>(frames) * kDayNightFrameMs;
  }
  for (int frame = 0; frame < frames; ++frame) {
    Step(night);
  }
}

void DayNightLight::Step(bool night) {
  if (night) {
    // Le bleu, troisième canal, n'est pas touché.
    for (int channel = 0; channel < 2; ++channel) {
      if (diffuse_[channel] > kNightDiffuseFloor) {
        diffuse_[channel] -= kDayNightStep;
      }
    }
    return;
  }
  for (int channel = 0; channel < 3; ++channel) {
    if (diffuse_[channel] < 1.0F && diffuse_[channel] < map_diffuse_[channel]) {
      diffuse_[channel] += kDayNightStep;
    }
  }
}

glm::vec3 DayNightLight::Tint() const {
  glm::vec3 tint{1.0F, 1.0F, 1.0F};
  for (int channel = 0; channel < 3; ++channel) {
    const float of_map = Screened(map_ambient_[channel], map_diffuse_[channel]);
    // Une carte noire sur ce canal n'a rien à assombrir.
    if (of_map > 0.0F) {
      tint[channel] = Screened(map_ambient_[channel], diffuse_[channel]) / of_map;
    }
  }
  return tint;
}

SceneLight DayNightLight::Lit(const SceneLight& map_light) const {
  SceneLight lit = map_light;
  lit.tint = Tint();
  return lit;
}

}  // namespace ro::render
