#include "ro/render/effect_particle_figures.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "ro/render/effect_particle_quad.h"
#include "ro/render/effect_quad_rotation.h"
#include "ro/render/effect_track.h"

namespace ro::render {
namespace {

// COMMENT LA PARTICULE MONTANTE LIT SA PISTE. Les cases d'une piste n'ont pas de
// sens en elles-mêmes — voir `effect_track.h` — et c'est ici que cette figure
// donne le sien aux siennes.
constexpr std::size_t kSlotOfHalfDiagonal = 1;
constexpr std::size_t kSlotOfSpinDegrees = 2;
constexpr std::size_t kMarkOfColorCase = 0;

// LES DIX CAS DE MARQUE du dessinateur. Une marque hors de cette table ne dessine RIEN : le cas par
// défaut du client saute l'émission entière.
constexpr std::size_t kMarkCaseCount = 10;
constexpr std::array<EffectParticleAppearance, kMarkCaseCount> kMarkCases = {{
    {{100, 150, 255}, false, false},
    {{250, 250, 155}, false, false},
    {{250, 100, 100}, true, false},
    {{50, 50, 255}, false, false},
    {{255, 50, 50}, false, false},
    {{50, 255, 50}, false, false},
    {{250, 100, 100}, false, true},
    {{5, 5, 255}, false, true},
    {{50, 50, 255}, true, false},
    {{70, 70, 70}, true, false},
}};

// LES DEUX COMPLÉMENTS QUE LE CLIENT POSE : trois cent soixante moins son
// lacet, quatre cent cinquante moins son altitude.
constexpr std::int32_t kTurnComplementDegrees = 360;
constexpr std::int32_t kTiltComplementDegrees = 450;

// Le quart de tour qui sépare deux coins du carré.
constexpr std::int32_t kQuarterTurnDegrees = 90;
constexpr std::size_t kCornerCount = 4;

// La texture que le dessinateur prend dans la liste du nœud, et le mode de
// mélange qu'il passe : le premier, et l'addition.
constexpr std::size_t kFirstTexture = 0;
constexpr std::int16_t kAdditiveMode = 0;

// L'image entière, dans l'ordre où le client range ses quatre nombres.
constexpr float kTextureAtLeft = 0.0F;
constexpr float kTextureAtRight = 1.0F;
constexpr float kTextureAtNear = 1.0F;
constexpr float kTextureAtFar = 0.0F;

[[nodiscard]] float RadiansOfDegrees(std::int32_t degrees) {
  return static_cast<float>(degrees) * std::numbers::pi_v<float> / 180.0F;
}

[[nodiscard]] std::int16_t WithinFullTurn(std::int32_t degrees) {
  const std::int32_t remainder = degrees % kEffectTrackFullTurnDegrees;
  return static_cast<std::int16_t>(remainder < 0 ? remainder + kEffectTrackFullTurnDegrees
                                                 : remainder);
}

// LE CENTRE DU CARRÉ : un point de la sphère que le rayon de la piste fait
// grandir. L'inclinaison y tient la longitude et l'angle la latitude — deux
// noms que seule cette figure justifie.
[[nodiscard]] glm::vec3 CentreOfParticle(const EffectTrack& track) {
  const float latitude = RadiansOfDegrees(track.degrees);
  const float longitude = RadiansOfDegrees(track.tilt_degrees);
  const float plat = track.radius_at_anchor * std::cos(latitude);
  return {plat * std::cos(longitude), track.radius_at_anchor * std::sin(latitude),
          plat * std::sin(longitude)};
}

}  // namespace

bool EffectRisingParticleIsDrawn(std::uint8_t mark) { return mark < kMarkCaseCount; }

EffectParticleAppearance EffectRisingParticleAppearance(std::uint8_t mark) {
  if (!EffectRisingParticleIsDrawn(mark)) {
    return {};
  }
  return kMarkCases[mark];
}

// Le lacet du client est l'opposé du nôtre, son altitude l'opposé de notre
// élévation : `360 − (−lacet)` et `450 − (−élévation)`.
std::int16_t EffectBillboardTurnDegrees(const Camera& camera) {
  return WithinFullTurn(kTurnComplementDegrees + static_cast<std::int32_t>(camera.yaw_degrees));
}

std::int16_t EffectBillboardTiltDegrees(const Camera& camera) {
  return WithinFullTurn(kTiltComplementDegrees + static_cast<std::int32_t>(camera.pitch_degrees));
}

std::optional<EffectWorldQuad> ComposeRisingParticleQuad(const EffectTrack& track,
                                                         std::int16_t billboard_turn_degrees,
                                                         std::int16_t billboard_tilt_degrees) {
  if (!track.active || track.alpha <= 0) {
    return std::nullopt;
  }
  const std::uint8_t style_mark = track.mark[kMarkOfColorCase];
  if (!EffectRisingParticleIsDrawn(style_mark)) {
    return std::nullopt;
  }
  const EffectParticleAppearance style = EffectRisingParticleAppearance(style_mark);

  const glm::vec3 centre = CentreOfParticle(track);
  const float half = track.value[kSlotOfHalfDiagonal];
  const float rotation = RadiansOfDegrees(static_cast<std::int32_t>(
      WithinFullTurn(static_cast<std::int32_t>(track.value[kSlotOfSpinDegrees]))));

  std::array<glm::vec3, kCornerCount> corners;
  for (std::size_t corner = 0; corner < kCornerCount; ++corner) {
    const std::int32_t angle = WithinFullTurn(
        track.azimuth_degrees + static_cast<std::int32_t>(corner) * kQuarterTurnDegrees);
    const float le_long = half * std::cos(RadiansOfDegrees(angle));
    const float au_travers = half * std::sin(RadiansOfDegrees(angle));
    // DEUX CARRÉS POUR UN SEUL NOM. Couché, il attend d'être redressé vers
    // l'œil ; debout, il tourne sur place autour de la verticale, et c'est son
    // propre angle qui le fait tourner.
    corners[corner] = style.faces_camera ? glm::vec3{le_long, 0.0F, au_travers}
                                         : glm::vec3{le_long * std::cos(rotation), au_travers,
                                                     le_long * std::sin(rotation)};
  }
  if (style.faces_camera) {
    RotateEffectQuadCorners(corners, billboard_turn_degrees, billboard_tilt_degrees);
  }

  EffectParticleQuadRequest request;
  // ⚠ LE REPÈRE SE RETOURNE ICI, et seulement ici, par `S` entier : tout ce qui
  // précède se calcule dans le repère du client, tourné par les angles de SA
  // caméra, et l'ancre est déjà dans le nôtre.
  for (std::size_t corner = 0; corner < kCornerCount; ++corner) {
    const glm::vec3 local = centre + corners[corner];
    request.corners[corner] = track.anchor + EffectPointFromClient(local);
  }
  // Les coins sont DÉJÀ des points du monde : le nœud n'a plus rien à y ajouter.
  request.absolute = true;
  request.alpha = static_cast<std::uint8_t>(std::min<std::int16_t>(track.alpha, 255));
  request.color = style.color;
  request.blend_mode = kAdditiveMode;
  request.texture = kFirstTexture;
  request.u_at_a_and_d = kTextureAtLeft;
  request.u_at_b_and_c = kTextureAtRight;
  request.v_at_a_and_b = kTextureAtNear;
  request.v_at_c_and_d = kTextureAtFar;
  request.depth = style.flattened_depth ? EffectQuadDepth::kFrontmost : EffectQuadDepth::kProjected;
  return ComposeEffectParticleQuad(request, {});
}

}  // namespace ro::render
