#include "ro/render/effect_aimed_spoke.h"

#include <algorithm>

#include "ro/render/effect_particle_quad.h"
#include "ro/render/effect_track.h"

#include "effect_track_angles.h"

namespace ro::render {
namespace {

// LES OCTETS QUE LA FIGURE DISTINGUE.
constexpr std::uint8_t kRoseTint = 1;
constexpr std::uint8_t kWhiteTint = 2;

constexpr std::array<std::uint8_t, 3> kWhite = {255, 255, 255};
constexpr std::array<std::uint8_t, 3> kRose = {255, 89, 182};
constexpr std::array<std::uint8_t, 3> kRed = {255, 25, 25};
constexpr std::array<std::uint8_t, 3> kBlue = {25, 25, 255};

// LES QUATRE NOMBRES DE TEXTURE `0, 1, 0, 1` : l'image est retournée, un aux
// deux premiers coins et zéro aux deux derniers.
constexpr float kImageVAtFirstCorners = 1.0F;
constexpr float kImageVAtLastCorners = 0.0F;

// UN POINT DU CLIENT DANS NOTRE REPÈRE : l'ordonnée et la profondeur se
// retournent.
[[nodiscard]] glm::vec3 FromClient(float x, float y, float z) { return {x, -y, -z}; }

// LES QUATRE COINS D'UN RAYON, dans notre repère, à l'angle du client donné.
//
// ⚠ LE PREMIER ET LE DERNIER SONT CONFONDUS AU CENTRE : le client en fait deux
// triangles, et le second n'a pas d'aire.
[[nodiscard]] std::array<glm::vec3, 4> SpokeCorners(const EffectTrack& track,
                                                    std::int32_t client_degrees) {
  const float length = track.value[kAimedSpokeSlotOfLength];
  const float width = track.lift_key;
  const float tip_x = length * CosOfDegrees(client_degrees);
  const float tip_z = length * SinOfDegrees(client_degrees);
  const std::int32_t en_arriere = WithinFullTurn(client_degrees - kAimedSpokeWidthBackDegrees);
  const glm::vec3 centre = FromClient(0.0F, kAimedSpokeClientDepthBelow, 0.0F);
  return {centre,
          FromClient(tip_x + (width * CosOfDegrees(en_arriere)), kAimedSpokeClientDepthBelow,
                     tip_z + (width * SinOfDegrees(en_arriere))),
          FromClient(tip_x, kAimedSpokeClientDepthBelow, tip_z), centre};
}

void EmitPass(const std::array<glm::vec3, 4>& corners, std::int16_t alpha,
              const std::array<std::uint8_t, 3>& tint, std::int16_t mode, glm::vec3 node_origin,
              std::vector<EffectWorldQuad>& quads) {
  EffectParticleQuadRequest request;
  request.corners = corners;
  request.absolute = false;
  request.alpha = static_cast<std::uint8_t>(alpha);
  request.color = tint;
  request.blend_mode = mode;
  request.texture = 0;
  request.v_at_a_and_b = kImageVAtFirstCorners;
  request.v_at_c_and_d = kImageVAtLastCorners;
  // ⚠ CETTE FIGURE PASSE TOUJOURS DEVANT TOUT : voir `EffectQuadDepth`.
  request.depth = EffectQuadDepth::kFrontmost;
  quads.push_back(ComposeEffectParticleQuad(request, node_origin));
}

}  // namespace

AimedSpokeColors AimedSpokeColorsOf(std::uint8_t tint_mark) {
  if (tint_mark == kRoseTint) {
    return {kRose, kRed};
  }
  if (tint_mark == kWhiteTint) {
    return {kWhite, kWhite};
  }
  return {kWhite, kBlue};
}

void AdvanceAimedSpokeTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks) {
  for (EffectTrack& effect_track : tracks) {
    if (!effect_track.active) {
      continue;
    }
    ++effect_track.ticks;
    if (effect_track.ticks <= kAimedSpokeRisesUntilTick) {
      effect_track.alpha = std::min<std::int16_t>(
          static_cast<std::int16_t>(effect_track.alpha + kAimedSpokeAlphaRise),
          kEffectTrackAlphaMax);
    } else if (effect_track.ticks > kAimedSpokeFadesAfterTick) {
      effect_track.alpha = static_cast<std::int16_t>(effect_track.alpha - kAimedSpokeAlphaFall);
      // ⚠ SOUS ZÉRO, LA PISTE MEURT : elle ne se rallume plus.
      if (effect_track.alpha < 0) {
        effect_track.alpha = 0;
        effect_track.active = false;
      }
    }
  }
}

void ComposeAimedSpokeQuads(const EffectTrack& track, glm::vec3 node_origin,
                            std::vector<EffectWorldQuad>& quads) {
  if (!track.active || track.alpha <= 0) {
    return;
  }
  const AimedSpokeColors teintes = AimedSpokeColorsOf(track.mark[kAimedSpokeMarkOfTint]);
  const auto pale = static_cast<std::int16_t>(track.alpha / kAimedSpokeDimPassDivisor);
  // LES DEUX RAYONS dans l'ordre du client : le cap moins vingt-cinq degrés,
  // puis le cap plus vingt-cinq.
  for (const std::int32_t offset : {-kAimedSpokeSpreadDegrees, kAimedSpokeSpreadDegrees}) {
    const std::array<glm::vec3, 4> corners =
        SpokeCorners(track, WithinFullTurn(track.tilt_degrees + offset));
    EmitPass(corners, pale, teintes.dim_pass, kAimedSpokeDimPassBlend, node_origin, quads);
    EmitPass(corners, track.alpha, teintes.full_pass, kAimedSpokeFullPassBlend, node_origin, quads);
  }
}

}  // namespace ro::render
