#pragma once

#include <span>
#include <string>

#include <glm/glm.hpp>

#include "ro/formats/image.h"
#include "ro/render/effect_magic_target.h"

namespace moonclient {

// LES CASES DE TERRAIN qu'une troupe demande de peindre, et sous quelle image.
//
// ⚠ CE N'EST PAS UN LOT DE QUADS, et c'est tout l'intérêt : la figure qui les
// rend ne connaît pas la carte, et une case n'a de coins que sur un relief
// donné. C'est `app` — qui tient le .gnd et le .gat — qui les ramène à des
// quads, juste avant de les peindre.
//
// `ground_position` est le point du nœud : les écarts de cases s'y rapportent.
struct DrawnGroundCells {
  std::span<const ro::render::EffectGroundCellPaint> cells;
  glm::vec3 ground_position{0.0F, 0.0F, 0.0F};
  const std::string* texture_path = nullptr;
  const ro::formats::Image* texture = nullptr;
};

}  // namespace moonclient
