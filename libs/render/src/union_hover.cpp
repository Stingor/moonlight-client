#include "ro/render/union_hover.h"

#include <algorithm>
#include <cmath>

namespace ro::render {
namespace {

// L'angle avance de deux degrés par pas, sur un tour.
constexpr std::uint16_t kAngleStepDegrees = 2;
constexpr std::uint16_t kTurnDegrees = 360;
// Le palier ne glisse que quand l'angle tombe sur un multiple de trente.
constexpr std::uint16_t kRestStepEveryDegrees = 30;
// Les deux hauteurs de repos, en pixels.
constexpr std::uint8_t kAfloatRestPixels = 20;
constexpr std::uint8_t kSittingRestPixels = 100;
// L'amplitude de l'oscillation, en pixels.
constexpr float kSwayPixels = 4.0F;

// Le sinus que le client lit dans sa table en degrés : l'angle passe en
// radians en simple précision, avec SA valeur de pi.
constexpr float kClientPi = 3.141592F;
constexpr float kHalfTurnDegrees = 180.0F;

// UNE IMAGE DU CLIENT, en millisecondes : on compte soixante images par
// seconde, l'hypothèse du voile du décor et de la poursuite de la caméra.
constexpr float kClientFrameMs = 1000.0F / 60.0F;

// Le plus long rattrapage d'une image : de quoi mener le palier de zéro à cent,
// à quinze pas par cran. Au-delà, une longue absence ne rejoue pas ses pas un à
// un.
constexpr std::uint32_t kMostStepsPerFrame =
    static_cast<std::uint32_t>(kSittingRestPixels) * (kRestStepEveryDegrees / kAngleStepDegrees);

float ClientSinDegrees(std::uint16_t degrees) {
  const float radians = static_cast<float>(degrees) / kHalfTurnDegrees * kClientPi;
  return static_cast<float>(std::sin(static_cast<double>(radians)));
}

// Un cran vers `target`, seulement sur un multiple de trente degrés.
std::uint8_t GlideToward(std::uint8_t rest, std::uint8_t target, std::uint16_t angle_degrees) {
  if (rest == target || angle_degrees % kRestStepEveryDegrees != 0) {
    return rest;
  }
  return rest < target ? static_cast<std::uint8_t>(rest + 1) : static_cast<std::uint8_t>(rest - 1);
}

}  // namespace

UnionHoverPosture UnionHoverPostureOf(Motion motion, bool red_veiled) noexcept {
  if (red_veiled) {
    return UnionHoverPosture::kAfloat;
  }
  switch (motion) {
    case Motion::kIdle:
    case Motion::kWalk:
    case Motion::kReady:
      return UnionHoverPosture::kAfloat;
    case Motion::kSit:
      return UnionHoverPosture::kSitting;
    case Motion::kPickup:
    case Motion::kAttack:
    case Motion::kCast:
    case Motion::kHurt:
    case Motion::kDie:
      return UnionHoverPosture::kLanding;
  }
  return UnionHoverPosture::kLanding;
}

UnionHover StepUnionHover(UnionHover hover, UnionHoverPosture posture) noexcept {
  hover.angle_degrees = static_cast<std::uint16_t>(hover.angle_degrees + kAngleStepDegrees);
  if (hover.angle_degrees >= kTurnDegrees) {
    hover.angle_degrees = static_cast<std::uint16_t>(hover.angle_degrees - kTurnDegrees);
  }
  switch (posture) {
    case UnionHoverPosture::kAfloat:
      hover.rest_pixels = GlideToward(hover.rest_pixels, kAfloatRestPixels, hover.angle_degrees);
      break;
    case UnionHoverPosture::kSitting:
      hover.rest_pixels = GlideToward(hover.rest_pixels, kSittingRestPixels, hover.angle_degrees);
      break;
    case UnionHoverPosture::kLanding:
      // ⚠ SANS ATTENDRE l'angle : la descente prend un cran à chaque pas.
      if (hover.rest_pixels > 0) {
        --hover.rest_pixels;
      }
      break;
  }
  return hover;
}

std::int32_t UnionHoverPixels(UnionHover hover) noexcept {
  const float height =
      kSwayPixels * ClientSinDegrees(hover.angle_degrees) + static_cast<float>(hover.rest_pixels);
  return std::max(0, static_cast<std::int32_t>(height));
}

std::int32_t UnionHoverClock::PixelsAt(std::uint32_t now_ms, UnionHoverPosture posture,
                                       std::uint32_t parts_drawn) {
  // La première image en est une : le client calcule la hauteur dès qu'il peint.
  std::uint32_t frames = 1;
  if (last_frame_ms_.has_value()) {
    constexpr float kMostOwedMs = static_cast<float>(kMostStepsPerFrame) * kClientFrameMs;
    owed_ms_ = std::min(owed_ms_ + static_cast<float>(now_ms - *last_frame_ms_), kMostOwedMs);
    frames = static_cast<std::uint32_t>(owed_ms_ / kClientFrameMs);
    owed_ms_ -= static_cast<float>(frames) * kClientFrameMs;
  }
  last_frame_ms_ = now_ms;
  const std::uint32_t steps = std::min(frames * std::max(parts_drawn, 1U), kMostStepsPerFrame);
  for (std::uint32_t step = 0; step < steps; ++step) {
    hover_ = StepUnionHover(hover_, posture);
  }
  return UnionHoverPixels(hover_);
}

}  // namespace ro::render
