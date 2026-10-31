#include "input/free_flight.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <optional>

#include <glm/glm.hpp>

#include "ro/render/camera.h"
#include "ro/sim/packed_position.h"

#include "input/first_person.h"

namespace moonclient {
namespace {

constexpr float kMsPerSecond = 1000.0F;

[[nodiscard]] float Held(const FlightKeys& held, FlightKey key) {
  return held[static_cast<std::size_t>(key)] ? 1.0F : 0.0F;
}

}  // namespace

glm::vec3 FreeFlightStep(float yaw_degrees, float pitch_degrees, const FlightKeys& held, bool fast,
                         std::uint32_t elapsed_ms) {
  // Le regard va de l'œil vers le point visé : l'opposé de `Camera::Position`.
  const float yaw = glm::radians(yaw_degrees);
  const float pitch = glm::radians(pitch_degrees);
  const glm::vec3 forward(-std::sin(yaw) * std::cos(pitch), -std::sin(pitch),
                          -std::cos(yaw) * std::cos(pitch));
  const glm::vec3 right(std::cos(yaw), 0.0F, -std::sin(yaw));
  const glm::vec3 up(0.0F, 1.0F, 0.0F);
  const glm::vec3 wanted =
      forward * (Held(held, FlightKey::kForward) - Held(held, FlightKey::kBack)) +
      right * (Held(held, FlightKey::kRight) - Held(held, FlightKey::kLeft)) +
      up * (Held(held, FlightKey::kUp) - Held(held, FlightKey::kDown));
  if (glm::length(wanted) < kNoDirection) {
    return glm::vec3(0.0F);
  }
  const float speed = kFreeFlightUnitsPerSecond * (fast ? kFreeFlightFastFactor : 1.0F);
  return glm::normalize(wanted) * speed * static_cast<float>(elapsed_ms) / kMsPerSecond;
}

std::optional<ro::sim::CellPosition> FreeFlightLeashCell(
    std::int32_t eye_cell_x, std::int32_t eye_cell_y, std::int32_t body_cell_x,
    std::int32_t body_cell_y, std::int32_t map_width, std::int32_t map_height) {
  if (map_width <= 0 || map_height <= 0) {
    return std::nullopt;
  }
  const std::int32_t x = std::clamp(eye_cell_x, 0, map_width - 1);
  const std::int32_t y = std::clamp(eye_cell_y, 0, map_height - 1);
  if (std::abs(x - body_cell_x) < kFreeFlightLeashCells &&
      std::abs(y - body_cell_y) < kFreeFlightLeashCells) {
    return std::nullopt;
  }
  return ro::sim::CellPosition{static_cast<std::uint16_t>(x), static_cast<std::uint16_t>(y), 0};
}

bool FreeFlight::LeashDue(std::uint32_t now_ms) {
  if (leash_sent_ms_.has_value() && now_ms - *leash_sent_ms_ < kFreeFlightLeashRepeatMs) {
    return false;
  }
  leash_sent_ms_ = now_ms;
  return true;
}

void FreeFlight::Enter(const ro::render::Camera& camera) {
  kept_ = camera;
  eye_ = camera.Position();
  yaw_degrees_ = camera.yaw_degrees;
  pitch_degrees_ =
      std::clamp(camera.pitch_degrees, -kFirstPersonMaxPitchDegrees, kFirstPersonMaxPitchDegrees);
  held_ = {};
  last_ms_.reset();
  leash_sent_ms_.reset();
  active_ = true;
}

void FreeFlight::Leave(ro::render::Camera& camera) {
  camera = kept_;
  held_ = {};
  active_ = false;
}

void FreeFlight::Look(float dx_px, float dy_px) {
  TurnGaze(yaw_degrees_, pitch_degrees_, dx_px, dy_px);
}

void FreeFlight::Advance(std::uint32_t now_ms, bool fast) {
  if (last_ms_.has_value()) {
    eye_ += FreeFlightStep(yaw_degrees_, pitch_degrees_, held_, fast, now_ms - *last_ms_);
  }
  last_ms_ = now_ms;
}

void FreeFlight::Aim(ro::render::Camera& camera) const {
  AimFirstPersonCamera(camera, eye_, yaw_degrees_, pitch_degrees_);
}

}  // namespace moonclient
