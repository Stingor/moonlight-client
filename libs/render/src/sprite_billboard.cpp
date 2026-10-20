#include "ro/render/sprite_billboard.h"

#include <algorithm>
#include <cmath>

namespace ro::render {

BillboardBasis MakeBillboardBasis(const Camera& camera) {
  const glm::vec3 forward = glm::normalize(camera.look_at - camera.Position());
  constexpr glm::vec3 world_up{0.0F, 1.0F, 0.0F};

  BillboardBasis basis;
  // L'axe droit reste HORIZONTAL quelle que soit l'inclinaison : il naît du
  // produit vectoriel avec la verticale du monde. Un personnage ne penche donc
  // jamais sur le côté.
  basis.right = glm::normalize(glm::cross(forward, world_up));
  // L'axe haut, lui, suit la caméra : c'est ce qui empêche le sprite de
  // s'écraser quand on plonge sur la scène.
  basis.up = glm::cross(basis.right, forward);
  basis.forward = forward;
  return basis;
}

BillboardBasis ShadowPanelBasis(const glm::vec3& toward_sun, const BillboardBasis& facing_camera) {
  constexpr glm::vec3 world_up{0.0F, 1.0F, 0.0F};
  BillboardBasis basis;
  basis.right = facing_camera.right;
  basis.forward = glm::cross(world_up, basis.right);
  basis.up = world_up;
  // En dessous, la trace du soleil au sol est trop courte pour dire une
  // direction : l'ombre tombe sous les pieds, quelle que soit la face.
  constexpr float kShortestGroundTrace = 1e-3F;
  const glm::vec3 ground_trace(toward_sun.x, 0.0F, toward_sun.z);
  if (glm::length(ground_trace) < kShortestGroundTrace) {
    return basis;
  }
  basis.forward = glm::normalize(ground_trace);
  basis.right = glm::normalize(glm::cross(basis.forward, world_up));
  // LA DROITE DU PANNEAU RESTE DU CÔTÉ DE CELLE DE L'ÉCRAN : la silhouette
  // portée ne se retourne pas.
  if (glm::dot(basis.right, facing_camera.right) < 0.0F) {
    basis.right = -basis.right;
    basis.forward = -basis.forward;
  }
  return basis;
}

glm::vec3 ShadowFootShift(float sunk_height, const BillboardBasis& facing_camera) {
  const float missing = (kShadowFootOverlapPixels / kSpritePixelsPerWorldUnit) - sunk_height;
  const glm::vec3 ground_forward(facing_camera.forward.x, 0.0F, facing_camera.forward.z);
  // Une caméra qui regarde à la verticale n'a pas de devant : le haut de
  // l'écran n'y désigne aucun recul au sol.
  constexpr float kShortestGroundForward = 1e-3F;
  if (missing <= 0.0F || glm::length(ground_forward) < kShortestGroundForward) {
    return glm::vec3(0.0F);
  }
  // Un pas de la hauteur de l'écran, vu sous une plongée `dive`, couvre au sol
  // `1 / dive` dans l'axe du regard.
  const float dive = std::max(-facing_camera.forward.y, kShadowFootMinimumDive);
  return glm::normalize(ground_forward) * (missing / dive);
}

std::array<glm::vec3, 4> BillboardCorners(const std::array<glm::vec2, 4>& quad_corners,
                                          const glm::vec3& ground_position,
                                          const BillboardBasis& basis) {
  std::array<glm::vec3, 4> corners{};
  for (std::size_t index = 0; index < corners.size(); ++index) {
    corners[index] = BillboardPoint(quad_corners[index], ground_position, basis);
  }
  return corners;
}

glm::vec3 BillboardPoint(glm::vec2 sprite_pixels, const glm::vec3& ground_position,
                         const BillboardBasis& basis) {
  const glm::vec2 units = sprite_pixels / kSpritePixelsPerWorldUnit;
  // L'ordonnée du sprite descend, celle du monde monte : d'où le signe.
  return ground_position + basis.right * units.x - basis.up * units.y;
}

std::array<glm::vec3, 4> BillboardCorners(const SpriteQuad& quad, const glm::vec3& ground_position,
                                          const BillboardBasis& basis) {
  return BillboardCorners(quad.corners, ground_position, basis);
}

std::array<glm::vec3, 4> BillboardDepthCorners(const SpriteQuad& quad,
                                               const glm::vec3& ground_position,
                                               const BillboardBasis& basis) {
  return BillboardDepthCorners(quad.corners, ground_position, basis);
}

std::array<glm::vec3, 4> BillboardDepthCorners(const std::array<glm::vec2, 4>& quad_corners,
                                               const glm::vec3& ground_position,
                                               const BillboardBasis& basis) {
  constexpr glm::vec3 world_up{0.0F, 1.0F, 0.0F};
  // Une caméra qui ne plonge pas ne voit aucun sol : rien à coucher dessus, et
  // la division ci-dessous n'aurait pas de sens.
  constexpr float kMinimumDive = 0.01F;
  const bool dives = basis.forward.y < -kMinimumDive;

  // Le plan VERTICAL de l'acteur, face à la caméra : sa normale est le regard
  // ramené à l'horizontale. Une caméra qui regarde droit vers le bas n'en a
  // pas, et le panneau garde alors la verticale du monde.
  const glm::vec3 level_forward{basis.forward.x, 0.0F, basis.forward.z};
  const float level_reach = glm::dot(basis.forward, level_forward);
  constexpr float kMinimumLevelReach = 1e-4F;
  const bool stands = level_reach > kMinimumLevelReach;

  std::array<glm::vec3, 4> corners{};
  for (std::size_t index = 0; index < corners.size(); ++index) {
    const glm::vec2 pixels = quad_corners[index] / kSpritePixelsPerWorldUnit;
    // L'ordonnée d'un sprite descend : négative au-dessus de l'origine.
    if (pixels.y > 0.0F) {
      glm::vec3 corner = ground_position + basis.right * pixels.x - world_up * pixels.y;
      if (dives) {
        // Sous l'origine, on remonte le long du rayon de visée jusqu'au plan du
        // sol : le point obtenu est celui du SOL que l'on voit à cet endroit de
        // l'écran, donc le sprite ne s'y enfonce plus.
        corner += basis.forward * (pixels.y / basis.forward.y);
      }
      corners[index] = corner - basis.forward * kSpriteGroundClearance;
      continue;
    }
    // 🔴 AU-DESSUS, LE POINT DU PLAN VERTICAL QUE L'ON VOIT À CE PIXEL : on part
    // de là où le pixel est DESSINÉ — le long de l'axe haut de la caméra — et on
    // glisse le long du regard jusqu'au plan de l'acteur. Prendre la même
    // hauteur à la verticale penchait le panneau en arrière d'autant que la
    // caméra plonge : à 50°, une tête passait derrière un décor qu'elle a
    // pourtant derrière elle.
    glm::vec3 corner = ground_position + basis.right * pixels.x - basis.up * pixels.y;
    if (stands) {
      corner -= basis.forward * (glm::dot(corner - ground_position, level_forward) / level_reach);
    } else {
      corner = ground_position + basis.right * pixels.x - world_up * pixels.y;
    }
    corners[index] = corner - basis.forward * kSpriteStandingClearance;
  }
  return corners;
}

float SpritePixelsPerScreenPixel(const Camera& camera, const glm::vec3& ground_position,
                                 float viewport_height_pixels) {
  const glm::vec3 eye = camera.Position();
  const glm::vec3 forward = glm::normalize(camera.look_at - eye);
  const float depth = glm::dot(ground_position - eye, forward);
  if (depth <= 0.0F || viewport_height_pixels <= 0.0F) {
    return 0.0F;
  }
  // La hauteur de la vue, en unités du monde, à cette profondeur.
  const float view_height_world = 2.0F * depth * std::tan(glm::radians(camera.fov_degrees) * 0.5F);
  return view_height_world / viewport_height_pixels * kSpritePixelsPerWorldUnit;
}

SpriteQuad GrowSpriteQuad(const SpriteQuad& quad, float grow) {
  SpriteQuad grown = quad;
  const glm::vec2 top = quad.corners[1] - quad.corners[0];
  const glm::vec2 side = quad.corners[3] - quad.corners[0];
  // Un quad sans aire n'a pas de bords où s'élargir.
  if (glm::dot(top, top) <= 0.0F || glm::dot(side, side) <= 0.0F) {
    return grown;
  }
  const glm::vec2 across = glm::normalize(top) * grow;
  const glm::vec2 down = glm::normalize(side) * grow;
  grown.corners[0] += -across - down;
  grown.corners[1] += across - down;
  grown.corners[2] += across + down;
  grown.corners[3] += -across + down;
  return grown;
}

SpriteQuad SpinSpriteQuad(const SpriteQuad& quad, float degrees) {
  SpriteQuad spun = quad;
  const glm::vec2 center =
      (quad.corners[0] + quad.corners[1] + quad.corners[2] + quad.corners[3]) * 0.25F;
  const float radians = glm::radians(degrees);
  const float cosine = std::cos(radians);
  const float sine = std::sin(radians);
  // Y DESCEND : la rotation directe de ce repère tourne à l'écran dans le sens
  // des aiguilles d'une montre — le haut passe à droite au quart de tour.
  for (glm::vec2& corner : spun.corners) {
    const glm::vec2 from = corner - center;
    corner = center + glm::vec2(from.x * cosine - from.y * sine, from.x * sine + from.y * cosine);
  }
  return spun;
}

}  // namespace ro::render
