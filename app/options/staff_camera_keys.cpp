#include "options/staff_camera_keys.h"

namespace moonclient {
namespace {

constexpr const char* kFreeCameraKey = "staff_camera_free";
constexpr const char* kCameraHudKey = "staff_camera_raised";

}  // namespace

void DeclareStaffCamera(SettingsFile& into, ro::ui::StaffToolsLook& look) {
  into.Declare(kFreeCameraKey, &look.free_camera);
  into.Declare(kCameraHudKey, &look.camera_hud);
}

}  // namespace moonclient
