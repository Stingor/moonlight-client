#include "ro/render/sky_clock.h"

#include <cmath>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <glm/glm.hpp>

#include "ro/render/scene_light.h"

using ro::render::AdvanceSkyHour;
using ro::render::SceneLight;
using ro::render::SkyLight;
using ro::render::SkyLightAt;

namespace {

constexpr float kTolerance = 1e-4F;

// Une carte au soleil haut, venu du sud-est.
SceneLight MapLight() {
  SceneLight light;
  light.direction = glm::normalize(glm::vec3(0.5F, 0.7F, 0.5F));
  light.ambient = glm::vec3(0.4F);
  light.diffuse = glm::vec3(0.9F, 0.8F, 0.7F);
  return light;
}

float AzimuthOf(const glm::vec3& direction) { return std::atan2(direction.x, direction.z); }

// LA COURSE TIRÉE DE LA SEULE CARTE : la lumière du RSW à midi, un demi-tour
// d'azimut, de six heures à dix-huit. Les essais du mécanisme la prennent pour
// repère.
ro::render::SkyCourse MapCourse() {
  ro::render::SkyCourse course;
  course.azimuth_offset_degrees = 0.0F;
  course.noon_altitude_forced = false;
  course.sweep_degrees = 180.0F;
  course.sunrise_hour = ro::render::kSunriseHour;
  course.sunset_hour = ro::render::kSunsetHour;
  return course;
}

}  // namespace

TEST_CASE("À midi, le ciel rend la lumière de la carte telle quelle", "[render][sky]") {
  const SceneLight map = MapLight();
  const SkyLight noon = SkyLightAt(map, ro::render::kNoonHour, MapCourse());
  CHECK_FALSE(noon.moon);
  CHECK(noon.light.direction == map.direction);
  CHECK(noon.light.diffuse == map.diffuse);
  CHECK(noon.light.ambient == map.ambient);
  CHECK(noon.shadow_strength == 1.0F);
  CHECK(noon.light.daylight == glm::vec3(1.0F));
  CHECK(noon.body_direction == map.direction);
}

TEST_CASE("Le soir et la nuit, la lumière de midi baisse pour ce que la direction n'éclaire pas",
          "[render][sky]") {
  const SceneLight map = MapLight();
  constexpr float kEvening = 17.0F;
  constexpr float kMidnight = 0.0F;
  const glm::vec3 evening = SkyLightAt(map, kEvening, MapCourse()).light.daylight;
  const glm::vec3 midnight = SkyLightAt(map, kMidnight, MapCourse()).light.daylight;
  CHECK(evening.r < 1.0F);
  CHECK(evening.r > 0.0F);
  CHECK(midnight.r < 1.0F);
  // La lune est bleutée : son bleu baisse moins que son rouge.
  CHECK(midnight.b > midnight.r);
}

TEST_CASE("Une ambiante pleine laisse midi intact et la nuit tomber", "[render][sky]") {
  // La lumière de yuno : ambiante et diffuse pleines.
  SceneLight map = MapLight();
  map.ambient = glm::vec3(1.0F);
  map.diffuse = glm::vec3(1.0F);
  constexpr float kMidnight = 0.0F;
  CHECK(SkyLightAt(map, ro::render::kNoonHour, MapCourse()).light.daylight == glm::vec3(1.0F));
  const SkyLight midnight = SkyLightAt(map, kMidnight, MapCourse());
  CHECK(midnight.light.daylight.r < 1.0F);
  // Le rendu garde l'ambiante déclarée : seule la part de l'heure la borne.
  CHECK(midnight.light.ambient == map.ambient);
}

TEST_CASE("Le matin, le soleil est plus bas et tourné d'un côté ; le soir, de l'autre",
          "[render][sky]") {
  const SceneLight map = MapLight();
  constexpr float kMorning = 9.0F;
  constexpr float kEvening = 15.0F;
  const SkyLight morning = SkyLightAt(map, kMorning, MapCourse());
  const SkyLight evening = SkyLightAt(map, kEvening, MapCourse());
  CHECK(morning.body_direction.y < map.direction.y);
  CHECK(morning.body_direction.y > 0.0F);
  // Symétriques autour de midi : même hauteur, azimuts de part et d'autre.
  CHECK(std::abs(morning.body_direction.y - evening.body_direction.y) < kTolerance);
  const float noon_azimuth = AzimuthOf(map.direction);
  CHECK(AzimuthOf(morning.body_direction) < noon_azimuth);
  CHECK(AzimuthOf(evening.body_direction) > noon_azimuth);
}

TEST_CASE("Au lever, l'astre s'efface : ni ombre, et seule l'ambiante reste", "[render][sky]") {
  const SceneLight map = MapLight();
  const SkyLight dawn = SkyLightAt(map, ro::render::kSunriseHour, MapCourse());
  CHECK(dawn.shadow_strength == 0.0F);
  CHECK(dawn.light.daylight.r < 1.0F);
  // L'éclairage lui-même reste celui de midi : seul son facteur baisse.
  CHECK(dawn.light.diffuse == map.diffuse);
  CHECK(dawn.light.direction == map.direction);
}

TEST_CASE("À minuit, la lune éclaire plus faiblement et ombre plus légèrement", "[render][sky]") {
  const SceneLight map = MapLight();
  constexpr float kMidnight = 0.0F;
  const SkyLight midnight = SkyLightAt(map, kMidnight, MapCourse());
  CHECK(midnight.moon);
  CHECK(midnight.body_direction.y > 0.0F);
  CHECK(midnight.light.daylight.r < 1.0F);
  // Bleutée : le bleu perd moins que le rouge.
  CHECK(midnight.light.daylight.b > midnight.light.daylight.r);
  CHECK(midnight.shadow_strength > 0.0F);
  CHECK(midnight.shadow_strength < 1.0F);
  // Vingt-quatre heures plus tard, la même lune.
  CHECK(SkyLightAt(map, ro::render::kHoursPerDay, MapCourse()).body_direction ==
        midnight.body_direction);
}

TEST_CASE("La course réglée tourne le soleil, impose sa hauteur et déplace son lever",
          "[render][sky]") {
  const SceneLight map = MapLight();
  ro::render::SkyCourse course = MapCourse();
  constexpr float kQuarterTurnDegrees = 90.0F;
  course.azimuth_offset_degrees = kQuarterTurnDegrees;
  const SkyLight turned = SkyLightAt(map, ro::render::kNoonHour, course);
  // Même hauteur à midi, azimut tourné d'un quart.
  CHECK(std::abs(turned.body_direction.y - map.direction.y) < kTolerance);
  CHECK(std::abs(AzimuthOf(turned.body_direction) -
                 (AzimuthOf(map.direction) + glm::radians(kQuarterTurnDegrees))) < kTolerance);

  constexpr float kHighNoonDegrees = 80.0F;
  course = MapCourse();
  course.noon_altitude_forced = true;
  course.noon_altitude_degrees = kHighNoonDegrees;
  const SkyLight high = SkyLightAt(map, ro::render::kNoonHour, course);
  CHECK(std::abs(high.body_direction.y - std::sin(glm::radians(kHighNoonDegrees))) < kTolerance);

  // Un lever à huit heures : à sept heures, c'est encore la lune.
  constexpr float kLateSunrise = 8.0F;
  constexpr float kBeforeLateSunrise = 7.0F;
  course = MapCourse();
  course.sunrise_hour = kLateSunrise;
  CHECK(SkyLightAt(map, kBeforeLateSunrise, course).moon);
  CHECK(SkyLightAt(map, kLateSunrise, course).shadow_strength == 0.0F);
  CHECK_FALSE(SkyLightAt(map, ro::render::kNoonHour, course).moon);
}

TEST_CASE("Le soleil tourne sur un cercle, d'un pas égal d'heure en heure", "[render][sky]") {
  const SceneLight map = MapLight();
  // Le soleil de chaque heure, la nuit compris : la lune en tient l'opposé.
  std::vector<glm::vec3> suns;
  for (int hour = 0; hour < static_cast<int>(ro::render::kHoursPerDay); ++hour) {
    const SkyLight sky = SkyLightAt(map, static_cast<float>(hour), MapCourse());
    suns.push_back(sky.moon ? -sky.body_direction : sky.body_direction);
  }
  // Tous dans un même plan : celui de six heures, midi et dix-huit heures.
  const glm::vec3 axis = glm::normalize(glm::cross(suns[12] - suns[6], suns[18] - suns[12]));
  const float plane_offset = glm::dot(axis, suns[6]);
  for (const glm::vec3& sun : suns) {
    CHECK(std::abs(glm::dot(axis, sun) - plane_offset) < kTolerance);
  }
  // Un pas égal d'une heure à la suivante, de jour comme de nuit.
  const float step = glm::length(suns[1] - suns[0]);
  for (std::size_t hour = 1; hour < suns.size(); ++hour) {
    CHECK(std::abs(glm::length(suns[hour] - suns[hour - 1]) - step) < kTolerance);
  }
}

TEST_CASE("Un balayage hors bornes garde un cercle", "[render][sky]") {
  const SceneLight map = MapLight();
  ro::render::SkyCourse course = MapCourse();
  course.sweep_degrees = 0.0F;
  const SkyLight closed = SkyLightAt(map, 9.0F, course);
  CHECK(std::isfinite(closed.body_direction.x));
  CHECK(std::abs(glm::length(closed.body_direction) - 1.0F) < kTolerance);
  course.sweep_degrees = 360.0F;
  const SkyLight full = SkyLightAt(map, 9.0F, course);
  CHECK(std::isfinite(full.body_direction.x));
  CHECK(std::abs(glm::length(full.body_direction) - 1.0F) < kTolerance);
}

TEST_CASE("La course par défaut : un soleil haut, tourné, levé à six heures et couché à vingt",
          "[render][sky]") {
  const SceneLight map = MapLight();
  const ro::render::SkyCourse course;
  // Midi tombe à mi-chemin du lever au coucher : treize heures.
  constexpr float kDefaultNoon = 13.0F;
  const SkyLight noon = SkyLightAt(map, kDefaultNoon, course);
  CHECK_FALSE(noon.moon);
  CHECK(std::abs(noon.body_direction.y -
                 std::sin(glm::radians(ro::render::SkyCourse::kDefaultNoonAltitudeDegrees))) <
        kTolerance);
  CHECK(std::abs(AzimuthOf(noon.body_direction) -
                 (AzimuthOf(map.direction) +
                  glm::radians(ro::render::SkyCourse::kDefaultAzimuthOffsetDegrees))) < kTolerance);
  // À dix-neuf heures, encore le soleil ; à vingt et une, la lune.
  CHECK(course.SunAt(19.0F));
  CHECK_FALSE(SkyLightAt(map, 19.0F, course).moon);
  CHECK_FALSE(course.SunAt(21.0F));
  CHECK(SkyLightAt(map, 21.0F, course).moon);
  CHECK_FALSE(course.SunAt(5.0F));
}

TEST_CASE("L'heure du ciel avance avec le temps réel et fait le tour du cadran", "[render][sky]") {
  constexpr float kOneMinuteMs = 60000.0F;
  constexpr float kHoursPerMinute = 2.0F;
  CHECK(std::abs(AdvanceSkyHour(10.0F, kOneMinuteMs, kHoursPerMinute) - 12.0F) < kTolerance);
  CHECK(std::abs(AdvanceSkyHour(23.0F, kOneMinuteMs, kHoursPerMinute) - 1.0F) < kTolerance);
  CHECK(AdvanceSkyHour(5.0F, 0.0F, kHoursPerMinute) == 5.0F);
}
