#include "ro/render/effect_shockwave_ray.h"

#include "ro/render/effect_particle_quad.h"
#include "ro/render/effect_quad_rotation.h"
#include "ro/render/effect_track.h"

#include "effect_track_angles.h"

namespace ro::render {
namespace {

// LES DEUX PISTES qui portent les angles de panneau, que la figure relit à des
// emplacements FIXES du nœud, quelle que soit la piste dessinée.
constexpr std::size_t kTrackThatTakesTheTurn = 0;
constexpr std::size_t kTrackThatTakesTheTilt = 1;

// LE MODE DE MÉLANGE des deux passages : zéro, l'ADDITION.
constexpr std::int16_t kAddedBlend = 0;
// LES QUATRE NOMBRES DE TEXTURE `0, 1, 0, 1` : l'image est retournée, un aux
// deux premiers coins et zéro aux deux derniers.
constexpr float kImageVAtFirstCorners = 1.0F;
constexpr float kImageVAtLastCorners = 0.0F;

// UN TRIANGLE du centre jusqu'au rayon, à `demi_ouverture` degrés de part et
// d'autre de l'angle de la piste, redressé vers la caméra puis rendu à notre
// repère.
void EmitRay(const EffectTrack& track, std::int32_t half_opening_degrees, std::int16_t alpha,
             const std::array<std::uint8_t, 3>& tint, std::int16_t turn_degrees,
             std::int16_t tilt_degrees, glm::vec3 node_origin,
             std::vector<EffectWorldQuad>& quads) {
  const float radius = track.radius_at_anchor;
  const std::int32_t before = WithinFullTurn(track.tilt_degrees - half_opening_degrees);
  const std::int32_t after = WithinFullTurn(track.tilt_degrees + half_opening_degrees);
  std::array<glm::vec3, 4> corners = {
      glm::vec3{0.0F, 0.0F, 0.0F},
      glm::vec3{radius * CosOfDegrees(before), 0.0F, radius * SinOfDegrees(before)},
      glm::vec3{radius * CosOfDegrees(after), 0.0F, radius * SinOfDegrees(after)},
      glm::vec3{0.0F, 0.0F, 0.0F}};
  RotateEffectQuadCorners(corners, turn_degrees, tilt_degrees);

  EffectParticleQuadRequest request;
  for (std::size_t corner = 0; corner < corners.size(); ++corner) {
    // ⚠ LE PANNEAU EST DANS LE REPÈRE DU CLIENT, tourné par les angles de SA
    // caméra : il revient au nôtre par `S` entier.
    request.corners[corner] = EffectPointFromClient(corners[corner]);
  }
  request.absolute = false;
  request.alpha = static_cast<std::uint8_t>(alpha);
  request.color = tint;
  request.blend_mode = kAddedBlend;
  request.texture = 0;
  request.v_at_a_and_b = kImageVAtFirstCorners;
  request.v_at_c_and_d = kImageVAtLastCorners;
  quads.push_back(ComposeEffectParticleQuad(request, node_origin));
}

}  // namespace

void AdvanceShockwaveRayTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                               std::int16_t billboard_turn_degrees,
                               std::int16_t billboard_tilt_degrees) {
  tracks[kTrackThatTakesTheTurn].style = billboard_turn_degrees;
  tracks[kTrackThatTakesTheTilt].style = billboard_tilt_degrees;
  for (EffectTrack& effect_track : tracks) {
    if (!effect_track.active) {
      continue;
    }
    ++effect_track.ticks;
    if (effect_track.ticks <= kShockwaveRayRisesUntilTick) {
      effect_track.alpha = static_cast<std::int16_t>(effect_track.alpha + kShockwaveRayAlphaRise);
    } else if (effect_track.ticks > kShockwaveRayFadesAfterTick) {
      effect_track.alpha = static_cast<std::int16_t>(effect_track.alpha - kShockwaveRayAlphaFall);
      if (effect_track.alpha < 0) {
        effect_track.alpha = 0;
      }
    }
  }
}

void ComposeShockwaveRayQuads(const EffectTrack& track, std::int16_t turn_degrees,
                              std::int16_t tilt_degrees, glm::vec3 node_origin,
                              std::vector<EffectWorldQuad>& quads) {
  if (!track.active || track.alpha <= 0) {
    return;
  }
  // ⚠ LA MOITIÉ DE L'ALPHA EST PRISE EN FLOTTANT PUIS TRONQUÉE vers zéro.
  const auto moitie =
      static_cast<std::int16_t>(static_cast<float>(track.alpha) * kShockwaveRayNarrowAlphaShare);
  EmitRay(track, kShockwaveRayNarrowHalfDegrees, moitie, kShockwaveRayNarrowColor, turn_degrees,
          tilt_degrees, node_origin, quads);
  EmitRay(track, kShockwaveRayWideHalfDegrees, track.alpha, kShockwaveRayWideColor, turn_degrees,
          tilt_degrees, node_origin, quads);
}

}  // namespace ro::render
