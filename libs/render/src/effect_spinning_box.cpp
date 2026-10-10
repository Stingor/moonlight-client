// La petite boîte qui tourne. Voir l'en-tête pour ses six faces et son repère.

#include "ro/render/effect_spinning_box.h"

#include <algorithm>
#include <cstdint>

#include "ro/render/effect_particle_quad.h"
#include "ro/render/effect_track.h"

#include "effect_track_angles.h"

namespace ro::render {
namespace {

// Combien de coins a le carré, et de combien de degrés ils s'écartent.
constexpr std::size_t kCorners = 4;
constexpr std::int32_t kQuarterTurnDegrees = kEffectTrackFullTurnDegrees / 4;

// L'IMAGE ENTIÈRE sur chaque face, retournée en hauteur : les deux premiers
// coins prennent le bas de l'image.
constexpr float kUOfFirstCorners = 0.0F;
constexpr float kUOfLastCorners = 1.0F;
constexpr float kVOfFirstCorners = 1.0F;
constexpr float kVOfLastCorners = 0.0F;

// Le mode de mélange zéro : l'addition.
constexpr std::int16_t kAddedBlend = 0;

[[nodiscard]] EffectWorldQuad Face(const std::array<glm::vec3, kCorners>& corners,
                                   const EffectTrack& effect_track, std::uint8_t blue,
                                   glm::vec3 node_origin) {
  EffectParticleQuadRequest request;
  request.corners = corners;
  request.alpha =
      static_cast<std::uint8_t>(std::clamp<std::int32_t>(effect_track.alpha, 0, UINT8_MAX));
  const std::uint8_t red_green = effect_track.mark[kSpinningBoxMarkOfRedGreen];
  request.color = {red_green, red_green, blue};
  request.blend_mode = kAddedBlend;
  request.u_at_a_and_d = kUOfFirstCorners;
  request.u_at_b_and_c = kUOfLastCorners;
  request.v_at_a_and_b = kVOfFirstCorners;
  request.v_at_c_and_d = kVOfLastCorners;
  return ComposeEffectParticleQuad(request, node_origin);
}

}  // namespace

std::vector<EffectWorldQuad> ComposeSpinningBoxQuads(const EffectTrack& track,
                                                     glm::vec3 node_origin) {
  std::vector<EffectWorldQuad> quads;
  if (!track.active) {
    return quads;
  }
  const float anchor_radius = track.radius_at_anchor;
  // LE RAYON RESPIRE avec l'inclinaison, entre huit et dix dixièmes.
  const float radius =
      (SinOfDegrees(track.tilt_degrees) + 1.0F) * (anchor_radius * kSpinningBoxRadiusWaveShare) +
      anchor_radius * kSpinningBoxRadiusFloorShare;
  // ⚠ LA HAUTEUR DU CLIENT COMPTE VERS LE BAS : sa clé et son balancement se
  // retournent ici, et le carré du haut est à la hauteur des coins PLUS le
  // rayon à l'ancre.
  const float bottom = -(kSpinningBoxBobAmplitude * SinOfDegrees(track.azimuth_degrees) +
                         track.value[kSpinningBoxSlotOfLift]);
  const float top = bottom + anchor_radius;

  std::array<glm::vec3, kCorners> dessous{};
  std::array<glm::vec3, kCorners> dessus{};
  for (std::size_t corner = 0; corner < kCorners; ++corner) {
    const std::int32_t angle = WithinFullTurn(
        track.azimuth_degrees + kQuarterTurnDegrees * static_cast<std::int32_t>(corner));
    // ⚠ LA PROFONDEUR DU CLIENT EST L'OPPOSÉE DE LA NÔTRE.
    const float x = CosOfDegrees(angle) * radius;
    const float z = -SinOfDegrees(angle) * radius;
    dessous[corner] = {x, bottom, z};
    dessus[corner] = {x, top, z};
  }

  quads.reserve(kSpinningBoxFaces);
  std::size_t face = 0;
  quads.push_back(Face(dessous, track, kSpinningBoxFaceBlues[face++], node_origin));
  for (std::size_t cote = 0; cote < kCorners; ++cote) {
    const std::size_t next = (cote + 1) % kCorners;
    quads.push_back(Face({dessous[cote], dessous[next], dessus[next], dessus[cote]}, track,
                         kSpinningBoxFaceBlues[face++], node_origin));
  }
  quads.push_back(Face(dessus, track, kSpinningBoxFaceBlues[face], node_origin));
  return quads;
}

void AdvanceSpinningBoxTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks) {
  for (EffectTrack& effect_track : tracks) {
    if (!effect_track.active) {
      continue;
    }
    // ⚠ LA MONTÉE S'ARRÊTE SUR LE PLAFOND, elle ne le dépasse pas : elle ne se
    // fait que d'en dessous.
    if (effect_track.alpha < kSpinningBoxAlphaCeiling) {
      effect_track.alpha = static_cast<std::int16_t>(effect_track.alpha + kSpinningBoxAlphaStep);
    }
    effect_track.tilt_degrees = static_cast<std::int16_t>(
        WithinFullTurn(effect_track.tilt_degrees + kSpinningBoxTurnStepDegrees));
    effect_track.azimuth_degrees = static_cast<std::int16_t>(
        WithinFullTurn(effect_track.azimuth_degrees + kSpinningBoxTurnStepDegrees));
  }
}

}  // namespace ro::render
