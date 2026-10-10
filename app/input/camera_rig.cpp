#include "input/camera_rig.h"

#include <array>
#include <limits>
#include <string>
#include <vector>

#include "ro/core/i18n.h"
#include "ro/core/log.h"
#include "ro/render/terrain_mesh.h"

namespace moonclient {
namespace {

constexpr std::string_view kIndoorTablePath = "data\\indoorrswtable.txt";

constexpr std::uint32_t kWheelQuietMs = 15;

[[nodiscard]] std::string TextOf(ro::vfs::Vfs& vfs, std::string_view path) {
  auto bytes = vfs.Read(path);
  if (!bytes) {
    return {};
  }
  return std::string(reinterpret_cast<const char*>(bytes->data()), bytes->size());
}

}  // namespace

void CameraRig::LoadTables(ro::vfs::Vfs& vfs) {
  // `viewpointtable.txt` n'est pas lue : le jeu livré ne l'applique pas.
  tables_ = MapCameraTables::Parse(TextOf(vfs, kIndoorTablePath), {});
  ro::core::LogInfo("caméra : {} carte(s) intérieure(s)", tables_.indoor_count());
}

void CameraRig::EnterMap(ro::render::Camera& camera, std::string_view map_name) {
  // Sous les bornes de la carte quittée : elles disent si elle était dehors.
  if (entered_) {
    RememberCamera(memory_, camera, limits_);
  }
  entered_ = true;
  rules_ = tables_.RulesFor(map_name);
  Recompute();
  ArriveWithCamera(camera, memory_, limits_);
}

void CameraRig::SetBounded(bool bounded) {
  if (bounded != bounded_) {
    bounded_ = bounded;
    Recompute();
  }
}

void CameraRig::SetExpandedZoom(bool expanded) {
  if (expanded != expanded_zoom_) {
    expanded_zoom_ = expanded;
    Recompute();
  }
}

bool CameraRig::TakeWheel(std::uint32_t now_ms) {
  if (wheeled_ && now_ms - last_wheel_ms_ <= kWheelQuietMs) {
    return false;
  }
  wheeled_ = true;
  last_wheel_ms_ = now_ms;
  return true;
}

void CameraRig::FloorEye(ro::render::Camera& camera, const MapStage& stage) const {
  camera.eye_floor_height = std::numeric_limits<float>::lowest();
  if (!limits_.bounded) {
    return;
  }
  // Le sol praticable, celui où les acteurs se posent.
  const glm::vec3 eye = camera.Position();
  if (stage.gat.has_value()) {
    const std::array<float, 2> cell = ro::render::FractionalCellAt(stage.scene.gnd, eye.x, eye.z);
    camera.eye_floor_height = ro::render::WalkableHeightAt(*stage.gat, cell[0], cell[1]);
  } else {
    camera.eye_floor_height = ro::render::GroundHeightAt(stage.scene.gnd, eye.x, eye.z);
  }
}

std::optional<CameraMemory> CameraRig::MemoryNow(const ro::render::Camera& camera) const {
  if (!entered_) {
    return std::nullopt;
  }
  CameraMemory now = memory_;
  RememberCamera(now, camera, limits_);
  return now;
}

ro::ui::CameraHudView CameraRig::HudOf(const ro::render::Camera& camera, std::string_view map_name,
                                       bool ctrl_held) const {
  const NativeView native = NativeViewOf(camera);
  const glm::vec3 eye = camera.Position();
  const char* kind = ro::core::Tr("dehors");
  if (limits_.rules.viewpoint.has_value()) {
    kind = ro::core::Tr("bornes propres");
  } else if (limits_.rules.indoor) {
    kind = ro::core::Tr("dedans");
  }
  return ro::ui::CameraHudView{.map_name = map_name,
                               .map_kind = kind,
                               .bounded = limits_.bounded,
                               .ctrl_held = ctrl_held,
                               .yaw_degrees = camera.yaw_degrees,
                               .pitch_degrees = camera.pitch_degrees,
                               .distance = camera.distance,
                               .fov_degrees = camera.fov_degrees,
                               .native_yaw_degrees = native.yaw_degrees,
                               .native_altitude_degrees = native.altitude_degrees,
                               .native_zoom = native.zoom,
                               .zoom_nearest = limits_.zoom_nearest,
                               .zoom_farthest = limits_.zoom_farthest,
                               .altitude_floor = limits_.altitude_floor,
                               .altitude_ceiling = limits_.altitude_ceiling,
                               .altitude_ceiling_ctrl = limits_.altitude_ceiling_ctrl,
                               .look_at = {camera.look_at.x, camera.look_at.y, camera.look_at.z},
                               .eye = {eye.x, eye.y, eye.z},
                               .eye_floor_height = camera.eye_floor_height,
                               .eye_floored = eye.y <= camera.eye_floor_height};
}

void CameraRig::Recompute() { limits_ = LimitsFor(rules_, expanded_zoom_, bounded_); }

}  // namespace moonclient
