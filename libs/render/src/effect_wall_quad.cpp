#include "ro/render/effect_wall_quad.h"

namespace ro::render {

EffectWorldQuad ComposeEffectWallQuad(const glm::vec2& near_foot, const glm::vec2& far_foot,
                                      float height, std::int16_t alpha,
                                      const std::array<std::uint8_t, 3>& color,
                                      std::int16_t blend_mode, glm::vec3 node_origin,
                                      const EffectOwnerTint& owner) {
  const float head = kEffectWallFloorHeight + height;
  // ⚠ LES PIEDS SONT DANS LE REPÈRE DU CLIENT : leur seconde coordonnée est sa
  // profondeur, qui se nie chez nous comme sa hauteur. La garder dresserait les
  // murs en miroir.
  const float near_depth = -near_foot.y;
  const float far_depth = -far_foot.y;
  EffectParticleQuadRequest request;
  request.corners = {glm::vec3{near_foot.x, kEffectWallFloorHeight, near_depth},
                     glm::vec3{far_foot.x, kEffectWallFloorHeight, far_depth},
                     glm::vec3{far_foot.x, head, far_depth},
                     glm::vec3{near_foot.x, head, near_depth}};
  request.alpha = static_cast<std::uint8_t>(alpha);
  request.color = color;
  request.blend_mode = blend_mode;
  // ⚠ L'IMAGE EST RETOURNÉE : les deux dessinateurs donnent un aux deux
  // premiers coins et zéro aux deux derniers, là où le défaut fait l'inverse.
  request.v_at_a_and_b = 1.0F;
  request.v_at_c_and_d = 0.0F;
  return ComposeEffectParticleQuad(request, node_origin, owner);
}

}  // namespace ro::render
