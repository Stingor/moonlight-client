#pragma once

#include <glm/glm.hpp>

#include "ro/render/camera.h"
#include "ro/render/sprite_vignette.h"

namespace moonclient {

// LA CAMÉRA QUI POSE `feet` AUX PIEDS D'UN PANTIN, à son échelle, dans une boîte
// de cette taille : ce qui s'y peint tombe sur le personnage comme en jeu.
//
// L'ÉCHELLE : un pixel de sprite vaut un septième d'unité — pour un effet comme
// pour un corps —, et le cadrage du pantin en fait `framing.scale` pixels de
// boîte. Au plan que la caméra vise, une unité occupe
// `hauteur / (2 · recul · tan(champ / 2))` pixels : le recul en découle.
//
// LA VISÉE se décale ensuite dans le plan de l'écran, que le panneau d'un sprite
// épouse, pour que les pieds tombent sur `framing.origin` et non au centre.
//
// Élévation et champ sont ceux du jeu : un effet s'y voit sous l'angle où il a
// été dessiné.
[[nodiscard]] ro::render::Camera DollCamera(glm::vec3 feet,
                                            const ro::render::VignetteFraming& framing,
                                            float box_width, float box_height);

}  // namespace moonclient
