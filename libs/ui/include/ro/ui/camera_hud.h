#pragma once

#include <array>
#include <string_view>

#include "ro/core/i18n.h"

namespace ro::ui {

// Le relevé de la caméra en direct, rempli par `app` à chaque image.
struct CameraHudView {
  std::string_view map_name;
  std::string_view map_kind;
  bool bounded = true;
  bool ctrl_held = false;

  float yaw_degrees = 0.0F;
  float pitch_degrees = 0.0F;
  float distance = 0.0F;
  float fov_degrees = 0.0F;

  // Altitude négative quand la caméra est en l'air.
  float native_yaw_degrees = 0.0F;
  float native_altitude_degrees = 0.0F;
  float native_zoom = 0.0F;

  float zoom_nearest = 0.0F;
  float zoom_farthest = 0.0F;
  float altitude_floor = 0.0F;
  float altitude_ceiling = 0.0F;
  float altitude_ceiling_ctrl = 0.0F;

  std::array<float, 3> look_at{};
  std::array<float, 3> eye{};
  float eye_floor_height = 0.0F;
  bool eye_floored = false;
};

inline constexpr const char* kCameraHudTitle = core::TrText("Caméra en direct");
inline constexpr const char* kCameraHudId = "camera_hud";

// `open` est la case de l'établi : la croix la décoche.
void DrawCameraHud(const CameraHudView& view, bool& open);

}  // namespace ro::ui
