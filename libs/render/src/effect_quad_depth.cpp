#include "ro/render/effect_quad_depth.h"

#include <cmath>

namespace ro::render {
namespace {

// Sous ce w, le point est sur l'œil ou derrière lui : il ne se projette pas.
constexpr float kSmallestVisibleW = 0.0001F;
// L'écran normalisé va de moins un à un : sa hauteur vaut deux.
constexpr float kNormalizedScreenSpan = 2.0F;

// OÙ VA CHAQUE SOMMET DANS LE RECENTRAGE : à droite ou à gauche de l'ancre, et
// par la demi-largeur de quel triangle. Le premier triangle se mesure de A à B,
// le second de D à C ; A et D passent à droite, B et C à gauche.
struct Recentring {
  bool right_of_anchor;
  bool second_triangle;
};
constexpr std::array<Recentring, kEffectQuadVertexCount> kRecentring = {{
    {true, false},
    {false, false},
    {false, false},
    {true, true},
    {false, true},
    {true, true},
}};

}  // namespace

std::optional<std::array<EffectQuadDepthVertex, kEffectQuadVertexCount>> PlaceEffectQuadVertices(
    const EffectWorldQuad& quad, const glm::mat4& view_projection, glm::vec3 eye) {
  std::array<EffectQuadDepthVertex, kEffectQuadVertexCount> sommets{};
  for (std::size_t rang = 0; rang < sommets.size(); ++rang) {
    const glm::vec3 corner = quad.corners[kEffectQuadVertexCorners[rang]];
    sommets[rang] = {corner, corner};
  }

  if (quad.depth == EffectQuadDepth::kNudged) {
    // L'INVERSE DE LA PROFONDEUR GAGNE UN SOUFFLE : le point glisse vers l'œil
    // le long de son rayon, et l'écran ne voit rien bouger. Sur ce rayon, la
    // profondeur est proportionnelle à la distance à l'œil.
    for (EffectQuadDepthVertex& vertex : sommets) {
      const float w = (view_projection * glm::vec4(vertex.position, 1.0F)).w;
      if (w > kSmallestVisibleW) {
        const float part = 1.0F / (1.0F + kEffectQuadDepthNudge * w);
        vertex.depth_position = eye + (vertex.position - eye) * part;
      }
    }
    return sommets;
  }
  if (quad.depth != EffectQuadDepth::kScreenRecentred) {
    return sommets;
  }

  const auto projeter = [&view_projection](glm::vec3 point) -> std::optional<glm::vec3> {
    const glm::vec4 clip = view_projection * glm::vec4(point, 1.0F);
    if (clip.w <= kSmallestVisibleW) {
      return std::nullopt;
    }
    return glm::vec3(clip) / clip.w;
  };
  const std::optional<glm::vec3> anchor = projeter(quad.screen_anchor);
  std::array<glm::vec3, 4> corners{};
  for (std::size_t corner = 0; corner < corners.size(); ++corner) {
    const std::optional<glm::vec3> projete = projeter(quad.corners[corner]);
    if (!anchor.has_value() || !projete.has_value()) {
      return std::nullopt;
    }
    corners[corner] = *projete;
  }
  const float first_half_width = std::abs(corners[0].x - corners[1].x) * 0.5F;
  const float second_half_width = std::abs(corners[3].x - corners[2].x) * 0.5F;
  const float levee = kEffectQuadRecentredRiseScreenShare * kNormalizedScreenSpan;
  const glm::mat4 inverse = glm::inverse(view_projection);
  for (std::size_t rang = 0; rang < sommets.size(); ++rang) {
    const glm::vec3& corner = corners[kEffectQuadVertexCorners[rang]];
    const float half = kRecentring[rang].second_triangle ? second_half_width : first_half_width;
    const float x = kRecentring[rang].right_of_anchor ? anchor->x + half : anchor->x - half;
    const glm::vec4 world = inverse * glm::vec4(x, corner.y + levee, corner.z, 1.0F);
    const glm::vec3 point = glm::vec3(world) / world.w;
    sommets[rang] = {point, point};
  }
  return sommets;
}

}  // namespace ro::render
