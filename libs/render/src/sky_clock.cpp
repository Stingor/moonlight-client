#include "ro/render/sky_clock.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "ro/render/sun_shadow.h"

namespace ro::render {
namespace {

constexpr float kMillisecondsPerMinute = 60000.0F;
// Le jour le plus court, et la nuit la plus courte : un arc a besoin de place.
constexpr float kShortestArcHours = 1.0F;

[[nodiscard]] float Wrapped(float hour) {
  const float wrapped = std::fmod(hour, kHoursPerDay);
  return wrapped < 0.0F ? wrapped + kHoursPerDay : wrapped;
}

// Le point de la sphère céleste à `altitude` et `azimuth`, en radians.
[[nodiscard]] glm::vec3 SkyPoint(float altitude, float azimuth) {
  return {std::cos(altitude) * std::sin(azimuth), std::sin(altitude),
          std::cos(altitude) * std::cos(azimuth)};
}

// LE CERCLE DU SOLEIL : celui qui passe par son lever, son midi et son
// coucher. Le soleil y tourne autour de l'axe `axis`, en partant du lever.
struct SunCircle {
  glm::vec3 center{0.0F};
  glm::vec3 axis{0.0F, 1.0F, 0.0F};
  // Les deux axes du plan du cercle : `start` vise le lever depuis le centre.
  glm::vec3 start{1.0F, 0.0F, 0.0F};
  glm::vec3 quarter{0.0F, 0.0F, 1.0F};
  float radius = 1.0F;
  // L'angle parcouru du lever au coucher, en radians.
  float day_angle = std::numbers::pi_v<float>;

  [[nodiscard]] glm::vec3 PointAt(float angle) const {
    return glm::normalize(center +
                          (radius * ((start * std::cos(angle)) + (quarter * std::sin(angle)))));
  }
};

[[nodiscard]] SunCircle SunCircleThrough(const glm::vec3& rise, const glm::vec3& noon,
                                         const glm::vec3& set) {
  SunCircle circle;
  // Orienté dans le sens de la course : du lever vers midi, puis le coucher.
  circle.axis = glm::normalize(glm::cross(noon - rise, set - noon));
  circle.center = circle.axis * glm::dot(circle.axis, rise);
  const glm::vec3 to_rise = rise - circle.center;
  circle.radius = glm::length(to_rise);
  circle.start = to_rise / circle.radius;
  circle.quarter = glm::cross(circle.axis, circle.start);
  const glm::vec3 to_set = set - circle.center;
  const float set_angle =
      std::atan2(glm::dot(to_set, circle.quarter), glm::dot(to_set, circle.start));
  constexpr float kFullTurn = 2.0F * std::numbers::pi_v<float>;
  circle.day_angle = set_angle < 0.0F ? set_angle + kFullTurn : set_angle;
  return circle;
}

}  // namespace

bool SkyCourse::SunAt(float hour) const {
  const float sunrise = Wrapped(sunrise_hour);
  const float day_hours =
      std::clamp(sunset_hour - sunrise_hour, kShortestArcHours, kHoursPerDay - kShortestArcHours);
  return Wrapped(Wrapped(hour) - sunrise) < day_hours;
}

SkyLight SkyLightAt(const SceneLight& map_light, float hour, const SkyCourse& course) {
  const float sunrise = Wrapped(course.sunrise_hour);
  const float day_hours = std::clamp(course.sunset_hour - course.sunrise_hour, kShortestArcHours,
                                     kHoursPerDay - kShortestArcHours);
  const float sunset = Wrapped(sunrise + day_hours);
  // La part du jour écoulée depuis le lever : le soleil tient la première, la
  // lune le reste.
  const float since_sunrise = Wrapped(Wrapped(hour) - sunrise);
  const bool moon = since_sunrise >= day_hours;
  const float arc_hours = moon ? kHoursPerDay - day_hours : day_hours;
  // La part de l'arc parcourue : zéro au lever, un demi au zénith, un au
  // coucher. La lune part au coucher du soleil.
  const float progress = (moon ? Wrapped(Wrapped(hour) - sunset) : since_sunrise) / arc_hours;

  const glm::vec3 noon = glm::normalize(map_light.direction);
  const float declared_altitude = std::asin(std::clamp(noon.y, -1.0F, 1.0F));
  const float lowest_altitude = glm::radians(kLowestNoonAltitudeDegrees);
  const float noon_altitude = course.noon_altitude_forced
                                  ? glm::radians(course.noon_altitude_degrees)
                                  : std::max(declared_altitude, lowest_altitude);
  const float noon_azimuth =
      std::atan2(noon.x, noon.z) + glm::radians(course.azimuth_offset_degrees);

  // LE SOLEIL TOURNE SUR UN CERCLE, à vitesse constante sur le jour et sur la
  // nuit : celui qui passe par son lever, son midi et son coucher. Comme le vrai
  // ciel autour de son axe, ce qui fait d'une ombre une aiguille de cadran
  // solaire. La nuit, il poursuit sous l'horizon, et la lune tient le point
  // opposé : elle se lève où il se couche.
  const float half_sweep =
      glm::radians(std::clamp(course.sweep_degrees, kNarrowestSweepDegrees, kWidestSweepDegrees)) *
      0.5F;
  const SunCircle circle = SunCircleThrough(SkyPoint(0.0F, noon_azimuth - half_sweep),
                                            SkyPoint(noon_altitude, noon_azimuth),
                                            SkyPoint(0.0F, noon_azimuth + half_sweep));
  constexpr float kFullTurn = 2.0F * std::numbers::pi_v<float>;
  const float sun_angle = moon ? circle.day_angle + (progress * (kFullTurn - circle.day_angle))
                               : progress * circle.day_angle;
  const glm::vec3 sun_point = circle.PointAt(sun_angle);
  const glm::vec3 body_point = moon ? -sun_point : sun_point;
  const float altitude = std::asin(std::clamp(body_point.y, -1.0F, 1.0F));
  constexpr float kNoonProgress = 0.5F;

  SkyLight sky;
  sky.moon = moon;
  sky.body_shown = true;
  sky.hour = Wrapped(hour);
  // ⚠ MIDI SUR LA COURSE D'ORIGINE GARDE LA DIRECTION DE LA CARTE TELLE
  // QUELLE : la recomposer par sinus et cosinus la décalerait d'un arrondi.
  const bool untouched_noon = !moon && progress == kNoonProgress &&
                              course.azimuth_offset_degrees == 0.0F &&
                              !course.noon_altitude_forced && declared_altitude >= lowest_altitude;
  sky.body_direction = untouched_noon ? map_light.direction : body_point;
  const float fade = std::clamp(altitude / glm::radians(kHorizonFadeDegrees), 0.0F, 1.0F);
  sky.shadow_strength = fade * (moon ? kMoonShadowStrength : 1.0F);

  // L'ÉCLAIRAGE RESTE CELUI DE MIDI, et l'heure le multiplie : ce que l'astre
  // donnerait à un sol plat, sur ce que la carte lui donne à midi. Les deux sous
  // l'ambiante bornée : une ambiante pleine éclairerait seule le sol à minuit.
  const SceneLight bound_light = SkyBoundLight(map_light);
  SceneLight body_light = bound_light;
  body_light.direction = sky.body_direction;
  body_light.diffuse = map_light.diffuse * fade * (moon ? kMoonDiffuseShare : glm::vec3(1.0F));
  constexpr float kDarkestNoon = 1e-4F;
  sky.light = map_light;
  sky.light.daylight =
      glm::clamp(FlatGroundLightOf(body_light) /
                     glm::max(FlatGroundLightOf(bound_light), glm::vec3(kDarkestNoon)),
                 0.0F, 1.0F);
  return sky;
}

float AdvanceSkyHour(float hour, float elapsed_ms, float game_hours_per_minute) {
  return Wrapped(hour + elapsed_ms / kMillisecondsPerMinute * game_hours_per_minute);
}

}  // namespace ro::render
