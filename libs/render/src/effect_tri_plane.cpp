#include "ro/render/effect_tri_plane.h"

#include <algorithm>
#include <cstddef>

#include "ro/render/effect_particle_quad.h"
#include "ro/render/effect_quad_rotation.h"
#include "ro/render/effect_track.h"
#include "ro/render/effect_track_spread.h"

#include "effect_track_angles.h"

namespace ro::render {
namespace {

// Le mode que la voie A entend comme une ADDITION.
constexpr std::int16_t kAddedBlend = 0;

// CE QUE LE MOUVEMENT DU 111 POSE.
constexpr std::int32_t kDriftingRisesUntilTick = 10;
constexpr std::int32_t kDriftingFadesAfterTick = 30;
constexpr float kDriftingAlphaRise = 20.0F;
constexpr float kDriftingAlphaFall = 3.0F;
// ⚠ LE TROISIÈME PLAN DÉRIVE VINGT POUR CENT PLUS VITE que les deux autres.
constexpr float kDriftingThirdPlaneHaste = 1.2F;

// CE QUE LE MOUVEMENT DU 115 POSE.
constexpr std::int32_t kFlyingSlowsAfterTick = 25;
constexpr float kFlyingSlowdown = 0.9F;
constexpr float kFlyingAlphaFall = 6.0F;

// LES DEUX ANGLES d'un plan, dans l'ordre où le client les lit : le CAP
// d'abord, la PENTE ensuite.
[[nodiscard]] std::int32_t CapDuPlan(const EffectTrack& effect_track, std::size_t plan) {
  return static_cast<std::int32_t>(effect_track.value[4 * plan]);
}

[[nodiscard]] std::int32_t PlaneSlope(const EffectTrack& effect_track, std::size_t plan) {
  return static_cast<std::int32_t>(effect_track.value[4 * plan + 2]);
}

// L'ALPHA d'un plan, tronqué et borné par le bas comme le client le borne.
[[nodiscard]] std::int16_t AlphaDuPlan(const EffectTrack& effect_track, std::size_t plan) {
  const auto integer =
      static_cast<std::int32_t>(effect_track.value[kTriPlaneSlotOfFirstAlpha + plan]);
  return static_cast<std::int16_t>(integer < 0 ? 0 : integer);
}

// LES DEUX ANGLES DE CAMÉRA, rangés dans le style des pistes zéro et un —
// une seule fois par tic, et non par piste.
void StoreCameraAngles(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                       std::int16_t camera_turn_degrees, std::int16_t camera_tilt_degrees) {
  StoreEffectBillboardAngles(tracks, camera_turn_degrees, camera_tilt_degrees);
}

}  // namespace

glm::vec3 EffectTriPlaneOffsetOf(const EffectTrack& track, std::size_t plane) {
  if (plane == kTriPlaneOffsetInTheAnchor) {
    return track.anchor;
  }
  const std::size_t base = plane == 0 ? kTriPlaneSlotOfFirstOffset : kTriPlaneSlotOfThirdOffset;
  return {track.tail[base], track.tail[base + 1], track.tail[base + 2]};
}

void SetEffectTriPlaneOffset(EffectTrack& track, std::size_t plane, glm::vec3 offset) {
  if (plane == kTriPlaneOffsetInTheAnchor) {
    track.anchor = offset;
    return;
  }
  const std::size_t base = plane == 0 ? kTriPlaneSlotOfFirstOffset : kTriPlaneSlotOfThirdOffset;
  track.tail[base] = offset.x;
  track.tail[base + 1] = offset.y;
  track.tail[base + 2] = offset.z;
}

bool EffectTriPlaneDraws(float color) {
  return color == kTriPlaneBlueColor || color == kTriPlanePalePinkColor ||
         color == kTriPlanePinkColor || color == kTriPlaneDeepPinkColor ||
         color == kTriPlanePerPlaneColor;
}

std::array<std::uint8_t, 3> EffectTriPlaneColor(float color, std::size_t plane) {
  if (color == kTriPlanePalePinkColor) {
    return kTriPlanePalePink;
  }
  if (color == kTriPlanePinkColor) {
    return kTriPlanePink;
  }
  if (color == kTriPlaneDeepPinkColor) {
    return kTriPlaneDeepPink;
  }
  if (color == kTriPlanePerPlaneColor) {
    return kTriPlanePerPlane[plane];
  }
  return kTriPlaneBlue;
}

std::vector<EffectWorldQuad> ComposeTriPlaneQuads(const EffectTrack& track,
                                                  std::int16_t turn_degrees,
                                                  std::int16_t tilt_degrees, glm::vec3 node_origin,
                                                  const EffectOwnerTint& owner) {
  std::vector<EffectWorldQuad> quads;
  if (!track.active) {
    return quads;
  }
  const float tint = track.value[kTriPlaneSlotOfColor];
  // ⚠ LE CLIENT COMPARE SA TEINTE À CINQ VALEURS EXACTES et n'a pas de cas par
  // défaut : toute autre — y compris entre deux — ne dessine RIEN. Se contenter
  // d'un intervalle peindrait ce qu'il laisse noir.
  if (!EffectTriPlaneDraws(tint)) {
    return quads;
  }

  // LE CARRÉ DE DÉPART : à plat, tourné d'un huitième de tour, de la taille du
  // rayon à l'ancre — puis couché par les deux angles de caméra.
  std::array<glm::vec3, 4> quad;
  for (std::size_t corner = 0; corner < quad.size(); ++corner) {
    const std::int32_t angle =
        kTriPlaneFirstCornerDegrees + kTriPlaneDegreesPerCorner * static_cast<std::int32_t>(corner);
    quad[corner] = {track.radius_at_anchor * CosOfDegrees(angle), 0.0F,
                    track.radius_at_anchor * SinOfDegrees(angle)};
  }
  RotateEffectQuadCorners(quad, turn_degrees, tilt_degrees);

  quads.reserve(kTriPlanePlanes);
  for (std::size_t plan = 0; plan < kTriPlanePlanes; ++plan) {
    const glm::vec3 shift = EffectTriPlaneOffsetOf(track, plan);
    // ⚠ LA GARDE PORTE SUR LE PREMIER COIN, pas sur le centre.
    if (quad[0].y + shift.y > 0.0F) {
      continue;
    }
    EffectParticleQuadRequest request;
    for (std::size_t corner = 0; corner < quad.size(); ++corner) {
      // ⚠ LE COIN ET SON DÉCALAGE SONT DANS LE REPÈRE DU CLIENT — le carré
      // tourné par les angles de SA caméra, le décalage qui dérive dans son
      // monde — et reviennent au nôtre ENSEMBLE, par `S` entier, au dernier
      // moment.
      request.corners[corner] = EffectPointFromClient(quad[corner] + shift);
    }
    request.alpha = static_cast<std::uint8_t>(AlphaDuPlan(track, plan));
    request.color = EffectTriPlaneColor(tint, plan);
    request.blend_mode = kAddedBlend;
    // ⚠ L'IMAGE EST RETOURNÉE : le client donne un aux deux premiers coins et
    // zéro aux deux derniers, là où le défaut fait l'inverse.
    request.v_at_a_and_b = 1.0F;
    request.v_at_c_and_d = 0.0F;
    quads.push_back(ComposeEffectParticleQuad(request, node_origin, owner));
  }
  return quads;
}

void AdvanceDriftingTriPlaneTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                                   std::int16_t camera_turn_degrees,
                                   std::int16_t camera_tilt_degrees, std::uint32_t& generator) {
  StoreCameraAngles(tracks, camera_turn_degrees, camera_tilt_degrees);
  for (EffectTrack& effect_track : tracks) {
    if (!effect_track.active) {
      continue;
    }
    ++effect_track.ticks;
    ChaseEffectTrackAngles(effect_track, kTriPlaneAnglePairs, generator);

    for (std::size_t plan = 0; plan < kTriPlanePlanes; ++plan) {
      float& alpha = effect_track.value[kTriPlaneSlotOfFirstAlpha + plan];
      if (effect_track.ticks < kDriftingRisesUntilTick) {
        alpha += kDriftingAlphaRise;
      } else if (effect_track.ticks > kDriftingFadesAfterTick) {
        alpha = std::max(alpha - kDriftingAlphaFall, 0.0F);
      }
    }

    const float step = effect_track.lift_key;
    // ⚠ LE PREMIER PLAN MONTE, LE DEUXIÈME DESCEND, ET LE TROISIÈME NE BOUGE
    // PAS. Le client écrit la hauteur des deux premiers en sens contraire et
    // n'écrit jamais celle du troisième ; la garde « au-dessus du nœud » ne
    // devient donc vraie que pour le premier. Les faire dériver tous les trois
    // inventerait deux mouvements.
    glm::vec3 first = EffectTriPlaneOffsetOf(effect_track, 0);
    if (first.y < 0.0F) {
      first.x += step * SinOfDegrees(CapDuPlan(effect_track, 0));
      first.z += step * SinOfDegrees(PlaneSlope(effect_track, 0));
    }
    first.y -= step;
    SetEffectTriPlaneOffset(effect_track, 0, first);

    glm::vec3 deuxieme = EffectTriPlaneOffsetOf(effect_track, 1);
    if (deuxieme.y < 0.0F) {
      deuxieme.x -= step * SinOfDegrees(CapDuPlan(effect_track, 1));
      deuxieme.z -= step * SinOfDegrees(PlaneSlope(effect_track, 1));
    }
    deuxieme.y += step;
    SetEffectTriPlaneOffset(effect_track, 1, deuxieme);

    glm::vec3 troisieme = EffectTriPlaneOffsetOf(effect_track, 2);
    if (troisieme.y < 0.0F) {
      const float hate = step * kDriftingThirdPlaneHaste;
      troisieme.x += hate * SinOfDegrees(CapDuPlan(effect_track, 2));
      troisieme.z -= hate * SinOfDegrees(PlaneSlope(effect_track, 2));
      SetEffectTriPlaneOffset(effect_track, 2, troisieme);
    }
  }
}

void AdvanceFlyingTriPlaneTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                                 std::int16_t camera_turn_degrees,
                                 std::int16_t camera_tilt_degrees) {
  StoreCameraAngles(tracks, camera_turn_degrees, camera_tilt_degrees);
  for (EffectTrack& effect_track : tracks) {
    if (!effect_track.active) {
      continue;
    }
    ++effect_track.ticks;
    const float step = effect_track.lift_key;
    for (std::size_t plan = 0; plan < kTriPlanePlanes; ++plan) {
      glm::vec3 shift = EffectTriPlaneOffsetOf(effect_track, plan);
      const std::int32_t pitch = PlaneSlope(effect_track, plan);
      const std::int32_t cap = CapDuPlan(effect_track, plan);
      // ⚠ LA PENTE MONTE dans notre repère : le client la retranche de sa
      // hauteur, et sa hauteur descend.
      shift.y -= step * SinOfDegrees(pitch);
      const float radial = step * CosOfDegrees(pitch);
      shift.x += radial * SinOfDegrees(cap);
      shift.z += radial * CosOfDegrees(cap);
      SetEffectTriPlaneOffset(effect_track, plan, shift);
    }
    if (effect_track.ticks <= kFlyingSlowsAfterTick) {
      continue;
    }
    // ⚠ LA VITESSE ET LES TROIS ALPHAS TOMBENT ENSEMBLE, et seulement passé le
    // vingt-cinquième tic. Les séparer ferait s'éteindre les plans en vol ou
    // les laisserait filer éclatants.
    effect_track.lift_key = step * kFlyingSlowdown;
    for (std::size_t plan = 0; plan < kTriPlanePlanes; ++plan) {
      float& alpha = effect_track.value[kTriPlaneSlotOfFirstAlpha + plan];
      alpha = std::max(alpha - kFlyingAlphaFall, 0.0F);
    }
  }
}

}  // namespace ro::render
