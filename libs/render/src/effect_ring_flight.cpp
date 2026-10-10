#include "ro/render/effect_ring_flight.h"

#include <cmath>

#include "ro/render/effect_node.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_node_world.h"

namespace ro::render {
namespace {

[[nodiscard]] bool SharesTheDraw(const EffectRingPattern& ring, std::size_t channel) {
  return (ring.color_shared_channels & (1U << channel)) != 0U;
}

}  // namespace

std::int32_t DrawEffectClientInteger(const EffectClientIntegerDraw& draw,
                                     std::uint32_t& generator) {
  if (draw.modulus <= 0) {
    return draw.bias;
  }
  const auto remainder = static_cast<std::int32_t>(NextEffectNodeDraw(generator) %
                                                   static_cast<std::uint32_t>(draw.modulus));
  const std::int32_t divisor = draw.divisor != 0 ? draw.divisor : 1;
  return (remainder + draw.addend) / divisor + draw.bias;
}

void DrawEffectRingTint(const EffectRingPattern& ring, std::array<float, 3>& color,
                        std::uint32_t& generator) {
  // LA TEINTE À DEUX VALEURS : un tirage par canal qui change, et la borne
  // haute pour le premier des `color_high_one_in` restes.
  if (ring.color_high_one_in > 0) {
    for (std::size_t channel = 0; channel < color.size(); ++channel) {
      const auto high = static_cast<float>(ring.color_high[channel]);
      if (high == color[channel]) {
        continue;
      }
      if (NextEffectNodeDraw(generator) % ring.color_high_one_in == 0U) {
        color[channel] = high;
      }
    }
    return;
  }
  // LE TIRAGE ENTIER, bornes comprises — `rand() % (haut − bas + 1) + bas` : un
  // reste pour les canaux qui le partagent, pris une seule fois et seulement si
  // l'un d'eux a de quoi tirer, puis un par canal seul.
  bool shared_needed = false;
  for (std::size_t channel = 0; channel < color.size(); ++channel) {
    shared_needed =
        shared_needed || (SharesTheDraw(ring, channel) &&
                          static_cast<float>(ring.color_high[channel]) > color[channel]);
  }
  const std::uint32_t shared = shared_needed ? NextEffectNodeDraw(generator) : 0U;
  for (std::size_t channel = 0; channel < color.size(); ++channel) {
    const auto high = static_cast<std::int32_t>(ring.color_high[channel]);
    const auto low = static_cast<std::int32_t>(color[channel]);
    if (high <= low) {
      continue;
    }
    const std::uint32_t remainder =
        SharesTheDraw(ring, channel) ? shared : NextEffectNodeDraw(generator);
    color[channel] = static_cast<float>(
        low + static_cast<std::int32_t>(remainder % static_cast<std::uint32_t>(high - low + 1)));
  }
}

glm::vec3 EffectNodeDepthAxis(float yaw_degrees, float pitch_degrees, float roll_degrees) {
  return -EffectNodeOriented(yaw_degrees, pitch_degrees, roll_degrees)[2];
}

glm::vec3 EffectNodeDepthAdvance(const PooledEffectNode& pooled) {
  if (!pooled.node.translates_along_axis) {
    return glm::vec3{0.0F, 0.0F, 0.0F};
  }
  // L'AXE DE LA MATRICE QUE LE CLIENT BÂTIT de ses tables : voir
  // `EffectNodeOrientedAsClient`.
  return -EffectNodeOrientedAsClient(pooled.node_yaw_degrees, pooled.node_pitch_degrees,
                                     pooled.node_roll_degrees)[2] *
         pooled.node.axial_translation;
}

// Un seul bit, lu deux fois par le même mouvement : il tue le nœud à l'arrivée
// et recopie l'angle du corps dans son lacet.
static_assert(kEffectNodeDiesAtTarget == kEffectOrientedQuadYawFollowsBody);

float EffectNodeBodyTowardTarget(glm::vec3 offset_to_target) {
  constexpr float kClientPi = 3.141592F;
  constexpr float kHalfTurnDegrees = 180.0F;
  constexpr float kFullTurnDegrees = 360.0F;
  // Un écart nul n'écrit rien : la face garde le zéro de la naissance.
  float face_degrees = 0.0F;
  if (offset_to_target.x != 0.0F || offset_to_target.z != 0.0F) {
    // La profondeur du client est l'opposée de la nôtre, et sa face se mesure
    // contre son opposée : notre profondeur entre donc telle quelle.
    face_degrees = static_cast<float>(std::atan2(offset_to_target.x, offset_to_target.z)) /
                   kClientPi * kHalfTurnDegrees;
    if (face_degrees >= kFullTurnDegrees) {
      face_degrees -= kFullTurnDegrees;
    }
    if (face_degrees < 0.0F) {
      face_degrees += kFullTurnDegrees;
    }
  }
  const float body_degrees = kEffectBodyAngleMirrorDegrees - face_degrees;
  return body_degrees < 0.0F ? body_degrees + kFullTurnDegrees : body_degrees;
}

bool EffectNodeArrivedAtTarget(const PooledEffectNode& pooled) {
  // Le mouvement des sous-types quinze et seize lit ce bit ; celui d'un sprite
  // visé porte le sien, que le semis traduit en `dies_near_target`.
  const bool reads_the_bit = pooled.shape == PooledNodeShape::kCrossedPlanes ||
                             pooled.shape == PooledNodeShape::kOrientedQuad;
  const bool dies_here = pooled.dies_near_target ||
                         (reads_the_bit && (pooled.node.flags & kEffectNodeDiesAtTarget) != 0U);
  if (!dies_here || !pooled.target_position.has_value()) {
    return false;
  }
  const glm::vec2 offset{pooled.ground_position.x - pooled.target_position->x,
                         pooled.ground_position.z - pooled.target_position->z};
  return glm::length(offset) <= kEffectNodeArrivalDistance;
}

}  // namespace ro::render
