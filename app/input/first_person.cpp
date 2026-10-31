#include "input/first_person.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>

#include <glm/glm.hpp>

#include "ro/render/camera.h"

namespace moonclient {
namespace {

// En deçà, deux touches opposées se sont annulées : aucune direction.
constexpr float kNoDirection = 1e-3F;

[[nodiscard]] float Held(const StrideKeys& held, StrideKey key) {
  return held[static_cast<std::size_t>(key)] ? 1.0F : 0.0F;
}

}  // namespace

float FirstPersonGazeUnits() {
  const float game_half_field = std::tan(glm::radians(ro::render::kGameFovDegrees) * 0.5F);
  const float own_half_field = std::tan(glm::radians(kFirstPersonFovDegrees) * 0.5F);
  return ro::render::kGameViewDistance * game_half_field / own_half_field;
}

std::optional<ro::sim::CellPosition> FirstPersonStride(const ro::sim::WalkMap& map,
                                                       std::int32_t from_x, std::int32_t from_y,
                                                       float yaw_degrees, const StrideKeys& held) {
  // 🔴 LE REPÈRE DES CASES N'EST PAS CELUI DU MONDE : une case de plus en `y`
  // recule de cinq unités en `z`. Le regard, qui part de l'œil vers le point
  // visé, tombe donc sur (−sin, +cos) en cases, et la droite de l'écran sur
  // (cos, sin).
  const float yaw = glm::radians(yaw_degrees);
  const glm::vec2 forward(-std::sin(yaw), std::cos(yaw));
  const glm::vec2 right(std::cos(yaw), std::sin(yaw));
  const glm::vec2 wanted =
      forward * (Held(held, StrideKey::kForward) - Held(held, StrideKey::kBack)) +
      right * (Held(held, StrideKey::kRight) - Held(held, StrideKey::kLeft));
  if (glm::length(wanted) < kNoDirection) {
    return std::nullopt;
  }
  const glm::vec2 direction = glm::normalize(wanted);
  std::optional<ro::sim::CellPosition> reached;
  for (std::int32_t stride = 1; stride <= kFirstPersonStrideCells; ++stride) {
    const auto x = static_cast<std::int32_t>(
        std::lround(static_cast<float>(from_x) + direction.x * static_cast<float>(stride)));
    const auto y = static_cast<std::int32_t>(
        std::lround(static_cast<float>(from_y) + direction.y * static_cast<float>(stride)));
    if (!map.Passable(x, y)) {
      break;
    }
    if (x != from_x || y != from_y) {
      reached =
          ro::sim::CellPosition{static_cast<std::uint16_t>(x), static_cast<std::uint16_t>(y), 0};
    }
  }
  return reached;
}

void AimFirstPersonCamera(ro::render::Camera& camera, const glm::vec3& eye, float yaw_degrees,
                          float pitch_degrees) {
  camera.distance = FirstPersonGazeUnits();
  camera.yaw_degrees = yaw_degrees;
  camera.pitch_degrees = pitch_degrees;
  camera.fov_degrees = kFirstPersonFovDegrees;
  // L'œil est le point visé plus le recul le long du regard : voir
  // `Camera::Position`. Le point visé est donc l'œil moins ce recul.
  const float pitch = glm::radians(pitch_degrees);
  const float yaw = glm::radians(yaw_degrees);
  const float ground_distance = camera.distance * std::cos(pitch);
  camera.look_at =
      eye - glm::vec3(ground_distance * std::sin(yaw), camera.distance * std::sin(pitch),
                      ground_distance * std::cos(yaw));
}

void FirstPersonView::Toggle(ro::render::Camera& camera) {
  if (active_) {
    camera.distance = kept_.distance;
    camera.pitch_degrees = kept_.pitch_degrees;
    camera.yaw_degrees = kept_.yaw_degrees;
    camera.fov_degrees = kept_.fov_degrees;
    held_ = {};
    active_ = false;
    return;
  }
  kept_ = Kept{.distance = camera.distance,
               .pitch_degrees = camera.pitch_degrees,
               .yaw_degrees = camera.yaw_degrees,
               .fov_degrees = camera.fov_degrees};
  yaw_degrees_ = camera.yaw_degrees;
  pitch_degrees_ = 0.0F;
  active_ = true;
}

void FirstPersonView::Look(float dx_px, float dy_px) {
  // ⚠ LE LACET DE LA CAMÉRA CROÎT VERS LA GAUCHE : tirer vers la droite le fait
  // donc décroître. Tirer vers le bas incline vers le sol, comme la caméra du
  // jeu.
  yaw_degrees_ -= dx_px * kFirstPersonDegreesPerPixel;
  pitch_degrees_ = std::clamp(pitch_degrees_ + dy_px * kFirstPersonDegreesPerPixel,
                              -kFirstPersonMaxPitchDegrees, kFirstPersonMaxPitchDegrees);
}

bool FirstPersonView::StrideDue(std::uint32_t now_ms, std::uint32_t repeat_ms) {
  if (stride_sent_ms_.has_value() && now_ms - *stride_sent_ms_ <= repeat_ms) {
    return false;
  }
  stride_sent_ms_ = now_ms;
  return true;
}

bool FirstPersonView::TakeStrideEnd() {
  const bool ended = stride_sent_ms_.has_value();
  stride_sent_ms_.reset();
  return ended;
}

void FirstPersonView::Aim(ro::render::Camera& camera, const glm::vec3& feet) const {
  AimFirstPersonCamera(camera, feet + glm::vec3(0.0F, kFirstPersonEyeUnits, 0.0F), yaw_degrees_,
                       pitch_degrees_);
}

}  // namespace moonclient
