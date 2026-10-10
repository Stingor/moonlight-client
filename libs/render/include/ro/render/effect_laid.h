#pragma once

#include <array>
#include <functional>
#include <span>
#include <vector>

#include <glm/glm.hpp>

#include "ro/render/effect_node_world.h"
#include "ro/render/effect_scene.h"

namespace ro::render {

// UN EFFET EN FICHIER COUCHÉ AU SOL, au lieu de faire face à la caméra.
//
// Ses pixels deviennent des unités du monde : l'abscisse de l'image va vers la
// droite de l'ÉCRAN, son ordonnée vers le spectateur, puis le tout tourne
// autour de la verticale de l'orientation demandée. Chaque coin se pose à
// `kLaidEffectLiftUnits` au-dessus du relief, là où il tombe — l'image épouse
// donc le terrain au lieu de rester plane.
//
// ⚠ L'ORIENTATION PART DE L'ÉCRAN, pas du monde : à orientation nulle, l'image
// se pose telle qu'on la verrait de face, quel que soit le cap de la caméra.

// Combien au-dessus du relief chaque coin se pose.
inline constexpr float kLaidEffectLiftUnits = 2.0F;

// Ce qui place un effet couché.
struct LaidEffectPose {
  // Le point du sol qui le porte, dans notre repère.
  glm::vec3 center{0.0F};
  // L'orientation, en degrés, depuis la droite de l'écran.
  float turn_degrees = 0.0F;
  // Le cap de la caméra, `Camera::yaw_degrees`.
  float camera_yaw_degrees = 0.0F;
  float scale = 1.0F;
  bool flip_x = false;
  bool flip_y = false;
};

// Les quads du monde que ces quads d'image donnent une fois couchés, ajoutés à
// `out`. `ground_height` rend la hauteur du relief en un point du monde ; vide,
// les coins restent à la hauteur du centre.
void LayEffectQuads(std::span<const EffectQuad> quads, const LaidEffectPose& pose,
                    const std::function<float(float world_x, float world_z)>& ground_height,
                    std::vector<EffectWorldQuad>& out);

// UN EFFET DEBOUT, grandi, retourné et tourné dans le plan de l'écran.
//
// Les coins sont en pixels autour de l'ANCRE, l'ordonnée descendant ; l'ancre
// est relevée de `lift_pixels` au-dessus du point du sol. Tout se fait autour
// de ce POINT DU SOL, et non de l'ancre : un effet grandi s'éloigne du sol en
// même temps qu'il grandit. L'échelle d'abord, puis les retournements, puis la
// rotation — dans le sens des aiguilles d'une montre, et seulement si elle est
// strictement positive.
void ShapeStandingEffectCorners(std::array<glm::vec2, 4>& corners, float lift_pixels,
                                float spin_degrees, float scale, bool flip_x, bool flip_y);

}  // namespace ro::render
