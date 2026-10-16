#include "options/staff_sky_keys.h"

namespace moonclient {
namespace {

constexpr const char* kTunedKey = "staff_sky_course_tuned";
constexpr const char* kAzimuthKey = "staff_sky_azimuth_offset";
constexpr const char* kAltitudeForcedKey = "staff_sky_noon_altitude_forced";
constexpr const char* kAltitudeKey = "staff_sky_noon_altitude";
constexpr const char* kSweepKey = "staff_sky_sweep";
constexpr const char* kSunriseKey = "staff_sky_sunrise";
constexpr const char* kSunsetKey = "staff_sky_sunset";

}  // namespace

void DeclareStaffSkyCourse(SettingsFile& into, ro::ui::StaffToolsLook& look) {
  into.Declare(kTunedKey, &look.sky_course_tuned);
  into.Declare(kAzimuthKey, &look.sky_course.azimuth_offset_degrees);
  into.Declare(kAltitudeForcedKey, &look.sky_course.noon_altitude_forced);
  into.Declare(kAltitudeKey, &look.sky_course.noon_altitude_degrees);
  into.Declare(kSweepKey, &look.sky_course.sweep_degrees);
  into.Declare(kSunriseKey, &look.sky_course.sunrise_hour);
  into.Declare(kSunsetKey, &look.sky_course.sunset_hour);
}

}  // namespace moonclient
