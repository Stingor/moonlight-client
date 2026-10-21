#include "ro/render/spin_blur.h"

#include <algorithm>
#include <cmath>

#include <glm/glm.hpp>

namespace ro::render {
namespace {

constexpr float kFullTurnDegrees = 360.0F;
constexpr float kHalfTurnDegrees = 180.0F;
constexpr float kMillisecondsPerSecond = 1000.0F;

float Radians(float degrees) { return glm::radians(degrees); }

}  // namespace

float ShortestTurnDegrees(float from_degrees, float to_degrees) {
  const float turned = std::fmod(to_degrees - from_degrees + kHalfTurnDegrees, kFullTurnDegrees);
  return (turned < 0.0F ? turned + kFullTurnDegrees : turned) - kHalfTurnDegrees;
}

SpinSweep SpinSweepOf(const SpinBlurSettings& settings, const CameraTurnRate& rate) {
  const float strength =
      std::clamp(settings.strength, kSpinBlurStrengthRange.lowest, kSpinBlurStrengthRange.highest);
  const float speed = std::hypot(rate.yaw_degrees_per_second, rate.pitch_degrees_per_second);
  if (!settings.enabled || strength <= 0.0F || speed <= spin_blur::kFloorDegreesPerSecond) {
    return {};
  }
  // Le balayage suit ce qui DÉPASSE le seuil : il part de zéro au seuil, sans
  // saut quand la vitesse le franchit.
  const float sweep =
      std::min((speed - spin_blur::kFloorDegreesPerSecond) * spin_blur::kExposureSeconds * strength,
               spin_blur::kMostSweepDegrees);
  const float share = sweep / speed;
  return SpinSweep{.yaw_degrees = rate.yaw_degrees_per_second * share,
                   .pitch_degrees = rate.pitch_degrees_per_second * share};
}

SpinBlurFrame SpinBlurFrameOf(const SpinBlurSettings& settings, const CameraTurnRate& rate,
                              const Camera& camera, float aspect_ratio) {
  const SpinSweep sweep = SpinSweepOf(settings, rate);
  if (sweep.yaw_degrees == 0.0F && sweep.pitch_degrees == 0.0F) {
    return {};
  }
  SpinBlurFrame frame;
  // Vue d'en haut, un cercle du sol reste un cercle ; vue de biais, il
  // s'écrase du sinus de l'élévation.
  const float elevation = Radians(std::abs(camera.pitch_degrees));
  frame.squash = std::max(std::sin(elevation), spin_blur::kSquashFloor);
  frame.arc_radians = Radians(sweep.yaw_degrees);
  // L'inclinaison qui varie fait glisser un point du sol en hauteur d'écran
  // d'autant plus qu'il est loin du point visé : un étirement vertical, de la
  // cotangente de l'élévation par radian.
  frame.stretch = Radians(sweep.pitch_degrees) * std::cos(elevation) / frame.squash;
  const glm::vec4 clip =
      camera.ProjectionMatrix(aspect_ratio) * camera.ViewMatrix() * glm::vec4(camera.look_at, 1.0F);
  if (clip.w > 0.0F) {
    const glm::vec2 ndc = glm::vec2(clip) / clip.w;
    frame.pivot_uv = {ndc.x * 0.5F + 0.5F, ndc.y * 0.5F + 0.5F};
  }
  return frame;
}

CameraTurnRate CameraSpinMeter::Measure(float yaw_degrees, float pitch_degrees, bool turned_by_hand,
                                        std::uint32_t now_ms) {
  if (!started_) {
    started_ = true;
    yaw_degrees_ = yaw_degrees;
    pitch_degrees_ = pitch_degrees;
    last_ms_ = now_ms;
    smoothed_ = {};
    return smoothed_;
  }
  const std::uint32_t elapsed_ms = now_ms - last_ms_;
  if (elapsed_ms == 0) {
    return smoothed_;
  }
  CameraTurnRate measured;
  if (turned_by_hand) {
    const float elapsed_seconds = static_cast<float>(elapsed_ms) / kMillisecondsPerSecond;
    measured.yaw_degrees_per_second =
        ShortestTurnDegrees(yaw_degrees_, yaw_degrees) / elapsed_seconds;
    measured.pitch_degrees_per_second =
        ShortestTurnDegrees(pitch_degrees_, pitch_degrees) / elapsed_seconds;
  }
  // 🔴 LE POIDS SE TIRE DU TEMPS ÉCOULÉ : deux images de dix millisecondes
  // à la même vitesse lissent exactement comme une de vingt.
  const float kept = std::exp(-static_cast<float>(elapsed_ms) / spin_blur::kSmoothingMs);
  smoothed_.yaw_degrees_per_second =
      measured.yaw_degrees_per_second +
      (smoothed_.yaw_degrees_per_second - measured.yaw_degrees_per_second) * kept;
  smoothed_.pitch_degrees_per_second =
      measured.pitch_degrees_per_second +
      (smoothed_.pitch_degrees_per_second - measured.pitch_degrees_per_second) * kept;
  yaw_degrees_ = yaw_degrees;
  pitch_degrees_ = pitch_degrees;
  last_ms_ = now_ms;
  return smoothed_;
}

}  // namespace ro::render
