#include "ro/render/effect_haunt.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "ro/render/effect_node.h"

namespace ro::render {

namespace {

constexpr std::int32_t kFullTurnDegrees = 360;
constexpr std::int32_t kHalfTurnDegrees = 180;

// Le plafond d'alpha du fantôme et de la chauve-souris lente, et celui de la
// chauve-souris qui dessine un huit.
constexpr float kQuietAlphaCeiling = 30.0F;
constexpr float kLoopingBatAlphaCeiling = 250.0F;
// Ce que l'alpha perd à chaque cran du départ.
constexpr float kQuietFadeStep = 1.0F;
constexpr float kLoopingBatFadeStep = 5.0F;
// Le pas d'orbite de la chauve-souris qui dessine un huit.
constexpr std::int32_t kLoopingBatOrbitStep = 2;

// LES DEUX TIRAGES DE POSE : un reste qui vaut un sur ce diviseur passe à
// l'autre pose, un reste nul sur le second ramène au repos. Le second vient
// APRÈS le premier et l'emporte.
constexpr std::uint32_t kBatOtherPoseDraws = 999;
constexpr std::uint32_t kBatOtherPoseHit = 1;
constexpr std::uint32_t kBatRestPoseDraws = 1001;

[[nodiscard]] std::int32_t WithinTurn(std::int32_t degrees) {
  return degrees >= kFullTurnDegrees ? degrees - kFullTurnDegrees : degrees;
}

[[nodiscard]] float Radians(std::int32_t degrees) {
  return static_cast<float>(degrees) * std::numbers::pi_v<float> /
         static_cast<float>(kHalfTurnDegrees);
}

// L'ACTION DU FANTÔME : il montre sa face quand l'angle sous lequel la caméra
// le voit tombe dans le premier demi-tour, son dos dans le second.
[[nodiscard]] std::int32_t GhostActionFacing(std::int32_t camera_yaw_degrees,
                                             std::int32_t orbit_degrees) {
  std::int32_t vu = camera_yaw_degrees + orbit_degrees;
  if (vu < 0) {
    vu += kFullTurnDegrees;
  }
  vu = WithinTurn(vu);
  return vu >= kHalfTurnDegrees ? kEffectHauntGhostTurnedAction : kEffectHauntGhostFacingAction;
}

}  // namespace

EffectHaunt StartEffectHaunt(EffectHauntFlight flight) {
  EffectHaunt haunt;
  haunt.flight = flight;
  if (flight == EffectHauntFlight::kLoopingBat) {
    haunt.orbit_step_degrees = kLoopingBatOrbitStep;
    haunt.alpha_ceiling = kLoopingBatAlphaCeiling;
  } else {
    haunt.alpha_ceiling = kQuietAlphaCeiling;
  }
  return haunt;
}

glm::vec3 AdvanceEffectHaunt(EffectHaunt& haunt, std::int32_t camera_yaw_degrees,
                             std::uint32_t& generator) {
  const std::int32_t age = ++haunt.age_ticks;
  const bool loop = haunt.flight == EffectHauntFlight::kLoopingBat;
  if (age <= kEffectHauntRiseTicks && haunt.alpha_ceiling > haunt.alpha) {
    haunt.alpha += 1.0F;
  }
  if (age >= kEffectHauntLeaveTick) {
    // LE DÉPART : les deux vols lents accélèrent d'un degré par tour et
    // s'éloignent en spirale ; tous s'effacent par crans.
    if (!loop) {
      if (age % (kFullTurnDegrees / haunt.orbit_step_degrees) == 0) {
        ++haunt.orbit_step_degrees;
      }
      haunt.radius *= kEffectHauntSpiralFactor;
    }
    if (age % kEffectHauntFadePeriodTicks == 0) {
      haunt.alpha = std::max(haunt.alpha - (loop ? kLoopingBatFadeStep : kQuietFadeStep), 0.0F);
    }
  }
  haunt.bob_degrees = WithinTurn(haunt.bob_degrees + kEffectHauntBobDegreesPerTick);
  haunt.orbit_degrees = WithinTurn(haunt.orbit_degrees + haunt.orbit_step_degrees);

  // LE HUIT : la profondeur prend l'angle doublé.
  const std::int32_t depth = loop ? WithinTurn(2 * haunt.orbit_degrees) : haunt.orbit_degrees;
  const glm::vec3 offset{haunt.radius * std::cos(Radians(haunt.orbit_degrees)),
                         std::sin(Radians(haunt.bob_degrees)) * kEffectHauntBobHeight,
                         haunt.radius * std::sin(Radians(depth))};

  if (haunt.flight == EffectHauntFlight::kGhost) {
    haunt.action = GhostActionFacing(camera_yaw_degrees, haunt.orbit_degrees);
  } else {
    if (NextEffectNodeDraw(generator) % kBatOtherPoseDraws == kBatOtherPoseHit) {
      haunt.action = kEffectHauntBatOtherAction;
    }
    if (NextEffectNodeDraw(generator) % kBatRestPoseDraws == 0U) {
      haunt.action = kEffectHauntBatRestAction;
    }
  }
  return offset;
}

}  // namespace ro::render
