#pragma once

#include <optional>

#include <glm/glm.hpp>

#include "ro/render/scene_light.h"

namespace ro::render {

// LA COURSE DU SOLEIL ET DE LA LUNE, à une heure donnée.
//
// La carte déclare UNE lumière : son azimut est celui de MIDI. Le soleil tourne
// à vitesse constante sur le cercle qui passe par son lever, par son midi et
// par son coucher, que règle la course — voir `SkyCourse`. Il poursuit son
// cercle sous l'horizon la nuit, et la lune tient alors le point opposé du
// ciel : elle se lève où le soleil s'est couché, et refait de nuit l'arc qu'il
// a fait de jour.
//
// 🔴 L'ÉCLAIRAGE DU SOL ET DU DÉCOR RESTE CELUI DE MIDI, et l'heure ne fait que
// le MULTIPLIER — voir `SceneLight::daylight`. L'astre ne sert qu'aux ombres
// et à sa place dans le ciel. Éclairer le sol depuis l'astre défaisait la
// saturation que la carte doit à ses graphistes : la nuit, rien ne sature plus,
// et les normales inclinées d'un bord d'île sortaient en bande sombre, case par
// case. À midi, rien ne change, au bit près.
//
// Près de l'horizon, la lumière et l'ombre s'éteignent ensemble : une ombre
// rasante s'étirerait sinon à l'infini.
//
// Aucune dépendance à OpenGL, et aucune à l'horloge du système : l'heure est
// donnée, jamais lue.

inline constexpr float kHoursPerDay = 24.0F;
inline constexpr float kSunriseHour = 6.0F;
inline constexpr float kSunsetHour = 18.0F;
inline constexpr float kNoonHour = 12.0F;

// La hauteur au-dessous de laquelle l'astre s'estompe, en degrés.
inline constexpr float kHorizonFadeDegrees = 10.0F;
// Une carte dont la lumière rase le sol garde au moins cette hauteur à midi.
inline constexpr float kLowestNoonAltitudeDegrees = 15.0F;
// LES BORNES DU BALAYAGE, en degrés : à zéro comme au tour complet, le lever
// et le coucher se confondent, et trois points ne disent plus de cercle.
inline constexpr float kNarrowestSweepDegrees = 10.0F;
inline constexpr float kWidestSweepDegrees = 350.0F;

// LA LUNE : une diffuse bleutée, bien plus faible que celle de la carte, et des
// ombres plus légères.
inline constexpr glm::vec3 kMoonDiffuseShare{0.25F, 0.30F, 0.42F};
inline constexpr float kMoonShadowStrength = 0.55F;

// LA COURSE DES ASTRES, réglable par l'établi du staff. Ses défauts sont ceux
// que le joueur a choisis à l'établi : un soleil haut, levé à six heures et
// couché à vingt, sur un quart de tour d'azimut tourné de soixante degrés
// depuis celui de la carte. Les champs à zéro, faux, 180° et de six heures à
// dix-huit rendent la course tirée de la seule lumière de la carte.
struct SkyCourse {
  // Tourne toute la course autour de la verticale, en degrés.
  float azimuth_offset_degrees = kDefaultAzimuthOffsetDegrees;
  // La hauteur de midi, imposée au lieu de celle de la carte.
  bool noon_altitude_forced = true;
  float noon_altitude_degrees = kDefaultNoonAltitudeDegrees;
  // L'azimut qui sépare le lever du coucher, en degrés, ramené entre
  // `kNarrowestSweepDegrees` et `kWidestSweepDegrees`.
  float sweep_degrees = kDefaultSweepDegrees;
  // ⚠ LE COUCHER SUIT LE LEVER D'AU MOINS UNE HEURE : un jour plus court
  // n'aurait plus d'arc.
  float sunrise_hour = kSunriseHour;
  float sunset_hour = kDefaultSunsetHour;

  static constexpr float kDefaultAzimuthOffsetDegrees = 60.0F;
  static constexpr float kDefaultNoonAltitudeDegrees = 70.0F;
  static constexpr float kDefaultSweepDegrees = 90.0F;
  static constexpr float kDefaultSunsetHour = 20.0F;

  // L'ASTRE DE L'HEURE `hour` est-il le soleil : vrai du lever au coucher,
  // la nuit passant minuit comprise.
  [[nodiscard]] bool SunAt(float hour) const;

  friend bool operator==(const SkyCourse&, const SkyCourse&) = default;
};

struct SkyLight {
  // La lumière à peindre : celle de la carte, son `daylight` posé.
  SceneLight light;
  // La direction VERS l'astre, normalisée : celle des ombres et de sa place
  // dans le ciel.
  glm::vec3 body_direction{0.0F, 1.0F, 0.0F};
  // Ce que l'ombre retire, voir `SunShadowView::strength`.
  float shadow_strength = 1.0F;
  bool moon = false;
  // L'ASTRE SE MONTRE-T-IL : vrai pour un ciel à l'heure, faux pour la lumière
  // fixe de la carte, qui n'a pas d'astre.
  bool body_shown = false;
  // L'heure du ciel, dans [0, 24) ; rien pour la lumière fixe de la carte.
  std::optional<float> hour;

  // LA LUMIÈRE QUI PORTE OMBRE : celle de la carte, venue de l'astre quand le
  // ciel en a un.
  [[nodiscard]] SceneLight CastingLight() const {
    SceneLight casting = light;
    if (body_shown) {
      casting.direction = body_direction;
    }
    return casting;
  }
};

// La lumière de `map_light` à l'heure `hour`, ramenée dans [0, 24), sur la
// course `course`. La lune tient le reste du jour, du coucher au lever.
[[nodiscard]] SkyLight SkyLightAt(const SceneLight& map_light, float hour,
                                  const SkyCourse& course = SkyCourse());

// L'heure qui suit `hour` après `elapsed_ms` réels, à `game_hours_per_minute`
// heures du ciel par minute réelle. Ramenée dans [0, 24).
[[nodiscard]] float AdvanceSkyHour(float hour, float elapsed_ms, float game_hours_per_minute);

}  // namespace ro::render
