#include "ro/render/effect_tapered_sheet.h"

#include <algorithm>

#include "ro/render/effect_particle_quad.h"
#include "ro/render/effect_quad_rotation.h"
#include "ro/render/effect_track.h"

#include "effect_track_angles.h"

namespace ro::render {
namespace {

// LES DEUX PISTES qui portent les angles de panneau.
constexpr std::size_t kTrackThatTakesTheTurn = 0;
constexpr std::size_t kTrackThatTakesTheTilt = 1;

// LE MODE DE MÉLANGE : zéro, l'ADDITION.
constexpr std::int16_t kAddedBlend = 0;
// LES QUATRE NOMBRES DE TEXTURE `0, 1, 0, 1` : l'image est retournée.
constexpr float kImageVAtFirstCorners = 1.0F;
constexpr float kImageVAtLastCorners = 0.0F;

// Un point à `distance` le long d'un angle du client, à plat.
[[nodiscard]] glm::vec3 AlongDegrees(std::int32_t degrees, float distance) {
  const std::int32_t angle = WithinFullTurn(degrees);
  return {distance * CosOfDegrees(angle), 0.0F, distance * SinOfDegrees(angle)};
}

}  // namespace

void AdvanceTaperedSheetTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                               glm::vec3 node_position, std::int16_t billboard_turn_degrees,
                               std::int16_t billboard_tilt_degrees) {
  tracks[kTrackThatTakesTheTurn].style = billboard_turn_degrees;
  tracks[kTrackThatTakesTheTilt].style = billboard_tilt_degrees;
  for (EffectTrack& effect_track : tracks) {
    if (!effect_track.active) {
      continue;
    }
    ++effect_track.ticks;
    if (effect_track.ticks <= 0) {
      continue;
    }
    effect_track.azimuth_degrees =
        static_cast<std::int16_t>(effect_track.azimuth_degrees + kTaperedSheetStretchStep);
    if (effect_track.azimuth_degrees > kTaperedSheetStretchCeiling) {
      effect_track.alpha = static_cast<std::int16_t>(effect_track.alpha - kTaperedSheetAlphaFall);
      effect_track.azimuth_degrees = kTaperedSheetStretchCeiling;
    }
    effect_track.alpha = std::clamp<std::int16_t>(effect_track.alpha, 0, kEffectTrackAlphaMax);
    // ⚠ L'ANCRE SUIT LE NŒUD À PLAT, mais garde sa hauteur de naissance.
    effect_track.anchor = {node_position.x, effect_track.tail[kTaperedSheetSlotOfHeight],
                           node_position.z};
  }
}

std::optional<EffectWorldQuad> ComposeTaperedSheetQuad(const EffectTrack& track,
                                                       std::int16_t turn_degrees,
                                                       std::int16_t tilt_degrees) {
  if (!track.active || track.alpha <= 0) {
    return std::nullopt;
  }
  const float radius = track.radius_at_anchor;
  const std::int32_t cap = track.degrees;
  // LE BORD INTÉRIEUR recule à mesure que la feuille s'étire.
  const float inner = radius - ((static_cast<float>(track.azimuth_degrees) * radius) /
                                kTaperedSheetStretchPerRadius);
  const float half_width = radius * kTaperedSheetHalfWidthShare;
  const glm::vec3 dedans = AlongDegrees(cap, inner);
  const glm::vec3 dehors = AlongDegrees(cap, radius);
  const glm::vec3 left = AlongDegrees(cap - kEffectTrackFullTurnDegrees / 4, half_width);
  const glm::vec3 right = AlongDegrees(cap + kEffectTrackFullTurnDegrees / 4, half_width);
  std::array<glm::vec3, 4> corners = {dedans + left, dedans + right, dehors + right, dehors + left};
  RotateEffectQuadCorners(corners, turn_degrees, tilt_degrees);

  EffectParticleQuadRequest request;
  for (std::size_t corner = 0; corner < corners.size(); ++corner) {
    // ⚠ LE PANNEAU EST DANS LE REPÈRE DU CLIENT, tourné par les angles de SA
    // caméra : il revient au nôtre par `S` entier ; l'ancre y est déjà.
    request.corners[corner] = track.anchor + EffectPointFromClient(corners[corner]);
  }
  request.absolute = true;
  request.alpha = static_cast<std::uint8_t>(track.alpha);
  request.color = kTaperedSheetColor;
  request.blend_mode = kAddedBlend;
  request.texture = track.mark[kTaperedSheetMarkOfImage];
  request.v_at_a_and_b = kImageVAtFirstCorners;
  request.v_at_c_and_d = kImageVAtLastCorners;
  // ⚠ CETTE FIGURE PASSE TOUJOURS DEVANT TOUT : voir `EffectQuadDepth`.
  request.depth = EffectQuadDepth::kFrontmost;
  return ComposeEffectParticleQuad(request, glm::vec3{0.0F, 0.0F, 0.0F});
}

}  // namespace ro::render
