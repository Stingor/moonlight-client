#pragma once

#include <array>
#include <cstddef>
#include <optional>

#include <glm/glm.hpp>

#include "ro/render/effect_node_world.h"

namespace ro::render {

// OÙ LES SIX SOMMETS D'UN QUAD DU MONDE SE POSENT, selon sa variante de
// profondeur. Voir `EffectQuadDepth`.
//
// Le quad se dessine en deux triangles, ABC puis ACD. Un sommet porte deux
// positions : celle qui décide de sa place à l'écran, et celle dont le
// renderer tire sa profondeur.
//
// ⚠ PUR, ET C'EST POUR POUVOIR L'ÉPROUVER : aucune ressource graphique n'entre
// ici, seulement la matrice de vue et de projection et la place de l'œil.

inline constexpr std::size_t kEffectQuadVertexCount = 6;
// Les coins que prennent les six sommets : ABC, puis ACD.
inline constexpr std::array<std::size_t, kEffectQuadVertexCount> kEffectQuadVertexCorners = {
    0, 1, 2, 0, 2, 3};

struct EffectQuadDepthVertex {
  glm::vec3 position{0.0F, 0.0F, 0.0F};
  glm::vec3 depth_position{0.0F, 0.0F, 0.0F};
};

// Vrai quand la variante fait passer le quad DEVANT tout, hors brouillard, sa
// texture posée sans correction de perspective.
[[nodiscard]] constexpr bool EffectQuadDepthIsFrontmost(EffectQuadDepth depth) noexcept {
  return depth == EffectQuadDepth::kFrontmost || depth == EffectQuadDepth::kScreenRecentred;
}

// Les six sommets, ou rien quand le recentrage demande un point derrière l'œil.
[[nodiscard]] std::optional<std::array<EffectQuadDepthVertex, kEffectQuadVertexCount>>
PlaceEffectQuadVertices(const EffectWorldQuad& quad, const glm::mat4& view_projection,
                        glm::vec3 eye);

}  // namespace ro::render
