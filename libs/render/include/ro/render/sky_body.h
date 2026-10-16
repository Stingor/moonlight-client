#pragma once

#include <glm/glm.hpp>

#include "ro/render/camera.h"

namespace ro::render {

// L'ASTRE DANS LE CIEL : le soleil, ou la lune, à sa vraie place.
//
// Il se tient à l'infini dans la direction que la lumière du ciel donne, et se
// peint sur le fond — là où ni le sol ni le décor ne le cachent. La caméra de
// jeu, qui plonge vers le sol, ne le voit pas : il faut lever les yeux, avec une
// caméra plus inclinée ou sans bornes.

// Le rayon apparent du disque, en degrés : bien plus que le vrai, pour qu'il se
// lise à l'écran.
inline constexpr float kSunAngularRadiusDegrees = 2.0F;
inline constexpr float kMoonAngularRadiusDegrees = 1.6F;
// Au-dessous de cette hauteur, en degrés, l'astre s'efface vers l'horizon.
inline constexpr float kSkyBodyFadeDegrees = 10.0F;

struct SkyBodyOnScreen {
  // Le centre du disque, en coordonnées normalisées de l'écran.
  glm::vec2 center{0.0F};
  // Son rayon, en part de la demi-hauteur de l'écran.
  float radius = 0.0F;
  // De zéro — sous l'horizon, ou derrière la caméra — à un.
  float visibility = 0.0F;
};

// Où se peint l'astre dont `toward_body` donne la direction, vu par `camera`
// sur un écran de rapport `aspect_ratio`, avec ce rayon apparent.
[[nodiscard]] SkyBodyOnScreen PlaceSkyBody(const glm::vec3& toward_body, const Camera& camera,
                                           float aspect_ratio, float angular_radius_degrees);

}  // namespace ro::render
