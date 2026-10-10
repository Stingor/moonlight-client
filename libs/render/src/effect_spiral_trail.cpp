#include "ro/render/effect_spiral_trail.h"

#include <algorithm>

#include "ro/render/effect_quad_rotation.h"
#include "ro/render/effect_track.h"

#include "effect_track_angles.h"

namespace ro::render {
namespace {

// DE COMBIEN L'AZIMUT AVANCE d'un coin au suivant : le carré est un carré.
constexpr std::int32_t kCornerStepDegrees = 90;

// LE MÉLANGE : zéro est l'ADDITION, dans la numérotation de la voie A.
constexpr std::int16_t kAddedBlend = 0;

constexpr std::uint8_t kFullChannel = 255;
// CE QU'UNE COMPOSANTE VAUT QUAND ELLE N'EST PAS PLEINE : soixante-dix pour
// quatre teintes sur cinq, quatre-vingt-dix pour la jaune.
constexpr std::uint8_t kDimChannel = 70;
constexpr std::uint8_t kYellowBlue = 90;

// L'ÉTINCELLE PEINT EN BLANC, quelle que soit la teinte de la piste.
constexpr std::array<std::uint8_t, 3> kSparkTint = {kFullChannel, kFullChannel, kFullChannel};

}  // namespace

std::array<std::uint8_t, 3> EffectSpiralTrailTintFor(std::uint8_t mark) {
  switch (mark) {
    case 0:
      return {kDimChannel, kDimChannel, kFullChannel};
    case 1:
      return {kFullChannel, kDimChannel, kDimChannel};
    case 2:
      return {kDimChannel, kFullChannel, kDimChannel};
    case 3:
      return {kFullChannel, kFullChannel, kYellowBlue};
    default:
      // ⚠ LE CINQUIÈME CAS EST CELUI QUI N'A PAS DE TEST, et le semis y tombe une
      // fois sur cinq : son tirage va de zéro à quatre.
      return {kFullChannel, kDimChannel, kFullChannel};
  }
}

std::vector<EffectWorldQuad> ComposeSpiralTrailQuads(const EffectTrack& track,
                                                     std::int16_t turn_degrees,
                                                     std::int16_t tilt_degrees,
                                                     const EffectOwnerTint& owner) {
  std::vector<EffectWorldQuad> quads;
  if (!track.active || track.alpha <= 0) {
    return quads;
  }
  const std::uint8_t spark = track.mark[kMarkOfSpiralTrailSpark];
  const std::array<std::uint8_t, 3> tint =
      EffectSpiralTrailTintFor(track.mark[kMarkOfSpiralTrailTint]);
  const glm::vec3 trace{track.tail[kTailOfSpiralTrailRecent],
                        track.tail[kTailOfSpiralTrailRecent + 1],
                        track.tail[kTailOfSpiralTrailRecent + 2]};

  for (std::size_t rang = 0; rang < kSpiralTrailImagesPerTrack; ++rang) {
    // ⚠ LE SECOND GRAIN EST PLUS PETIT, PAS PLUS LOIN : le client retranche un
    // demi à la demi-taille avant de poser ses coins. C'est ce rétrécissement,
    // et non un écart, qui donne à la traînée sa forme de goutte.
    const float half = track.lift_key - static_cast<float>(rang) * kSpiralTrailTrailShrink;
    const glm::vec3 centre = rang == 0 ? track.anchor : trace;

    EffectParticleQuadRequest request;
    for (std::size_t corner = 0; corner < request.corners.size(); ++corner) {
      const std::int32_t angle = WithinFullTurn(
          track.azimuth_degrees + static_cast<std::int32_t>(corner) * kCornerStepDegrees);
      request.corners[corner] = {half * CosOfDegrees(angle), 0.0F, half * SinOfDegrees(angle)};
    }
    // Le grain se tourne dans le repère du client, par les angles de SA caméra,
    // et revient au nôtre par `S` entier avant de rejoindre son centre, qui y
    // est déjà.
    RotateEffectQuadCorners(request.corners, turn_degrees, tilt_degrees);
    for (glm::vec3& corner : request.corners) {
      corner = EffectPointFromClient(corner) + centre;
    }

    // ⚠ L'ALPHA SE DIVISE PAR LE RANG, ET LE RANG COMMENCE À UN : la trace n'en
    // garde que la moitié. Diviser par le rang compté depuis zéro ferait une
    // division par zéro au premier grain.
    std::int16_t alpha =
        static_cast<std::int16_t>(track.alpha / static_cast<std::int16_t>(rang + 1));
    if (spark != 0) {
      alpha = static_cast<std::int16_t>(alpha / kSpiralTrailSparkAlphaDivider);
    }
    request.alpha = static_cast<std::uint8_t>(std::clamp<std::int16_t>(alpha, 0, 255));
    request.color = spark != 0 ? kSparkTint : tint;
    request.texture = spark;
    request.blend_mode = kAddedBlend;
    // ⚠ ABSOLUS : l'ancre porte déjà un point du monde. Voir l'en-tête.
    request.absolute = true;
    quads.push_back(ComposeEffectParticleQuad(request, glm::vec3{0.0F, 0.0F, 0.0F}, owner));
  }
  return quads;
}

}  // namespace ro::render
