#include "options/staff_camera_keys.h"

namespace moonclient {
namespace {

constexpr const char* kFreeCameraKey = "staff_camera_free";
constexpr const char* kCameraHudKey = "staff_camera_raised";
constexpr const char* kEyeHeightForcedKey = "staff_camera_eye_forced";
constexpr const char* kEyeHeightPixelsKey = "staff_camera_eye_pixels";

}  // namespace

void DeclareStaffCamera(SettingsFile& into, ro::ui::StaffToolsLook& look) {
  into.Declare(kFreeCameraKey, &look.free_camera);
  into.Declare(kCameraHudKey, &look.camera_hud);
  into.Declare(kEyeHeightForcedKey, &look.eye_height_forced);
  into.Declare(kEyeHeightPixelsKey, &look.eye_height_pixels);
}

}  // namespace moonclient
