#include "survivors/arena_camera.h"

#include <algorithm>
#include <array>
#include <cmath>

#include "ro/render/terrain_mesh.h"

namespace moonclient {
namespace {

// Les coins de l'image en coordonnées normalisées, et les profondeurs du plan
// proche et du plan lointain.
constexpr std::array<glm::vec2, 4> kImageCorners = {glm::vec2(-1.0F, -1.0F), glm::vec2(1.0F, -1.0F),
                                                    glm::vec2(1.0F, 1.0F), glm::vec2(-1.0F, 1.0F)};
constexpr float kNearDepth = -1.0F;
constexpr float kFarDepth = 1.0F;

// La descente ralentit en cube : vive au départ, posée à l'arrivée.
[[nodiscard]] float EaseOutCubic(float progress) {
  const float remaining = 1.0F - progress;
  return 1.0F - remaining * remaining * remaining;
}

}  // namespace

float ArenaDescentDistance(float rest_distance, std::uint32_t arrival_ms) {
  if (arrival_ms >= kArenaDescentMs) {
    return rest_distance;
  }
  const float progress = static_cast<float>(arrival_ms) / static_cast<float>(kArenaDescentMs);
  const float from_distance = rest_distance * kArenaDescentFromFactor;
  return from_distance + (rest_distance - from_distance) * EaseOutCubic(progress);
}

float ArenaFovDegrees(float aspect_ratio) {
  if (aspect_ratio <= kArenaWidestAspect) {
    return kArenaFovDegrees;
  }
  // La demi-largeur vue est tan(champ / 2) × rapport : la garder à celle du
  // rapport le plus large resserre le champ vertical d'autant.
  const float widest_half_tangent =
      std::tan(glm::radians(kArenaFovDegrees) * 0.5F) * kArenaWidestAspect;
  return glm::degrees(2.0F * std::atan(widest_half_tangent / aspect_ratio));
}

std::optional<GroundFootprint> GroundFootprintOf(const ro::render::Camera& camera,
                                                 float aspect_ratio) {
  const glm::mat4 unproject =
      glm::inverse(camera.ProjectionMatrix(aspect_ratio) * camera.ViewMatrix());
  const auto world_of = [&unproject](glm::vec2 corner, float depth) {
    const glm::vec4 point = unproject * glm::vec4(corner, depth, 1.0F);
    return glm::vec3(point) / point.w;
  };
  GroundFootprint footprint;
  for (const glm::vec2 corner : kImageCorners) {
    const glm::vec3 near_point = world_of(corner, kNearDepth);
    const glm::vec3 ray = world_of(corner, kFarDepth) - near_point;
    // Un rayon qui ne descend pas ne rencontre pas le sol.
    if (ray.y >= 0.0F) {
      return std::nullopt;
    }
    const float along = (camera.look_at.y - near_point.y) / ray.y;
    const glm::vec3 ground = near_point + ray * along;
    // 🔴 LE NORD EST VERS LES Z DÉCROISSANTS : la grille des cases est inversée
    // sur cet axe.
    const float east_cells = (ground.x - camera.look_at.x) / ro::render::kWorldUnitsPerCell;
    const float north_cells = (camera.look_at.z - ground.z) / ro::render::kWorldUnitsPerCell;
    footprint.west_cells = std::max(footprint.west_cells, -east_cells);
    footprint.east_cells = std::max(footprint.east_cells, east_cells);
    footprint.north_cells = std::max(footprint.north_cells, north_cells);
    footprint.south_cells = std::max(footprint.south_cells, -north_cells);
  }
  return footprint;
}

ArenaCamera::ArenaCamera() {
  camera_.fov_degrees = kArenaFovDegrees;
  camera_.pitch_degrees = kArenaElevationDegrees;
  camera_.yaw_degrees = kArenaYawDegrees;
  camera_.distance = wheel_distance_;
  // 🔴 SUIVI RIGIDE : le héros marche plus vite que le seuil du rattrapage, et
  // une caméra qui traîne laisserait la horde entrer par un bord.
  aim_.tracking.fixed = true;
}

void ArenaCamera::Enter() noexcept { aim_.tracking.Reset(); }

void ArenaCamera::Wheel(float notches) noexcept {
  SetDistance(wheel_distance_ - notches * kArenaWheelStepDistance);
}

void ArenaCamera::SetDistance(float distance) noexcept {
  wheel_distance_ = std::clamp(distance, kArenaNearestDistance, kArenaFarthestDistance);
}

const ro::render::Camera& ArenaCamera::Pose(const glm::vec3& hero_ground, std::uint32_t arrival_ms,
                                            float aspect_ratio, std::uint32_t now_ms,
                                            const ro::sim::World& effects_world) {
  aim_.shake.Undo(camera_);
  // Le recul, l'élévation, le lacet et le champ se reposent entiers à chaque
  // image : rien de la précédente n'y survit, et le lacet reste celui du nord.
  camera_.distance = ArenaDescentDistance(wheel_distance_, arrival_ms);
  camera_.pitch_degrees = kArenaElevationDegrees;
  camera_.yaw_degrees = kArenaYawDegrees;
  camera_.fov_degrees = ArenaFovDegrees(aspect_ratio);
  aim_.tracking.Follow(camera_, hero_ground, now_ms);
  aim_.shake.Apply(camera_, effects_world, now_ms);
  return camera_;
}

}  // namespace moonclient
