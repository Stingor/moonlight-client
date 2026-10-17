#include "ro/render/effect_spiral_stack.h"

#include <algorithm>
#include <cmath>

#include "ro/render/effect_track.h"

#include "effect_track_angles.h"

namespace ro::render {
namespace {

constexpr std::int16_t kAddedBlend = 0;
constexpr std::int16_t kOrdinaryBlend = 1;

// CE QUE CHAQUE STYLE DEMANDE, relevé sur les dix branches du `switch`.
//
// ⚠ DEUX STYLES ADDITIONNENT ET LES AUTRES SE MÉLANGENT, sans que rien dans leur
// numéro ne le dise : zéro et dix d'un côté, un à huit de l'autre. Les traiter
// pareil rendrait les deux premiers gris.
struct StyleRow {
  std::uint8_t style;
  std::uint8_t red;
  std::uint8_t green;
  std::uint8_t blue;
  std::int16_t blend;
  std::int16_t divisor;
};

constexpr std::array<StyleRow, 9> kStyleRows = {{
    {0, 50, 50, 255, kAddedBlend, 1},
    {1, 164, 136, 55, kOrdinaryBlend, 1},
    {2, 155, 255, 155, kOrdinaryBlend, 1},
    {3, 130, 130, 255, kOrdinaryBlend, 1},
    {4, 255, 50, 50, kOrdinaryBlend, 1},
    {5, 5, 5, 5, kOrdinaryBlend, 1},
    {6, 255, 255, 255, kOrdinaryBlend, 1},
    {7, 255, 125, 255, kOrdinaryBlend, 1},
    // ⚠ CELUI-CI DIVISE SON ALPHA PAR DIX : c'est un voile, pas un trait.
    {8, 225, 225, 255, kOrdinaryBlend, 10},
}};

// ⚠ LE STYLE ONZE PEINT DEUX COUCHES, et lui seul : un blanc au tiers de
// l'alpha, puis un rouge à pleine force par-dessus. N'en garder qu'une le
// priverait de son halo.
constexpr std::uint8_t kStyleWithTwoLayers = 11;
constexpr std::int16_t kTwoLayerDivisor = 3;
constexpr std::uint8_t kStyleWithPaleBlue = 10;

}  // namespace

std::vector<EffectSpiralStackStyle> EffectSpiralStackStylesFor(std::uint8_t style) {
  if (style == kStyleWithTwoLayers) {
    return {EffectSpiralStackStyle{{255, 255, 255}, kOrdinaryBlend, kTwoLayerDivisor, true},
            EffectSpiralStackStyle{{255, 55, 55}, kAddedBlend, 1, true}};
  }
  if (style == kStyleWithPaleBlue) {
    return {EffectSpiralStackStyle{{125, 125, 255}, kAddedBlend, 1, true}};
  }
  const auto found = std::ranges::find(kStyleRows, style, &StyleRow::style);
  if (found == kStyleRows.end()) {
    // ⚠ LE `default` DU CLIENT NE DESSINE RIEN. C'est une réponse, pas un oubli.
    return {EffectSpiralStackStyle{{255, 255, 255}, kOrdinaryBlend, 1, false}};
  }
  return {EffectSpiralStackStyle{
      {found->red, found->green, found->blue}, found->blend, found->divisor, true}};
}

std::vector<EffectWorldQuad> ComposeSpiralStackQuads(const EffectTrack& track,
                                                     glm::vec3 node_origin,
                                                     const EffectOwnerTint& owner) {
  std::vector<EffectWorldQuad> quads;
  const auto etages = static_cast<std::int32_t>(track.mark[kMarkOfSpiralSteps]);
  if (!track.active || track.alpha <= 0 || etages <= 0) {
    return quads;
  }
  const std::vector<EffectSpiralStackStyle> layers =
      EffectSpiralStackStylesFor(track.mark[kMarkOfSpiralStyle]);
  if (layers.empty() || !layers.front().draws) {
    return quads;
  }

  // LES ORDONNÉES SE CALCULENT DANS LE REPÈRE DU CLIENT, qui descend : le
  // sommet y est l'opposé de la case, chaque étage le rabaisse vers le sol.
  // Elles changent de signe à la pose des coins, et la pile s'élève en
  // entonnoir au-dessus de son ancre.
  float height = -track.value[kSlotOfSpiralTop];
  float radius = track.lift_key;

  quads.reserve(static_cast<std::size_t>(etages) * layers.size());
  for (std::int32_t etage = 0; etage < etages; ++etage) {
    if (etage > 0) {
      height += track.value[kSlotOfSpiralRise];
      radius -= (kSpiralStackShrinkBase - static_cast<float>(etage) * kSpiralStackShrinkDecay) *
                track.radius_at_anchor;
    }
    const std::int32_t start =
        (track.azimuth_degrees + kSpiralStackTurnPerStep * etage) % kEffectTrackFullTurnDegrees;

    // LES QUATRE COINS, à quatre-vingt-dix degrés l'un de l'autre. Les deux
    // premiers sont posés un cran plus haut : c'est le gauchissement.
    std::array<glm::vec3, 4> corners{};
    for (std::size_t corner = 0; corner < corners.size(); ++corner) {
      const float theta = DegreesToRadians(static_cast<float>(
          (start + kSpiralStackCornerDegrees * static_cast<std::int32_t>(corner)) %
          kEffectTrackFullTurnDegrees));
      const float y_client = corner < 2 ? height - kSpiralStackWarp : height;
      // LE COIN ENTIER PASSE DANS NOTRE REPÈRE, profondeur comprise : ne
      // retourner que l'ordonnée ferait de la pile le miroir de la sienne.
      corners[corner] =
          EffectPointFromClient({radius * std::cos(theta), y_client, radius * std::sin(theta)});
    }

    for (const EffectSpiralStackStyle& layer : layers) {
      EffectParticleQuadRequest request;
      request.corners = corners;
      request.alpha = static_cast<std::uint8_t>(
          std::clamp<std::int16_t>(track.alpha / layer.alpha_divisor, 0, 255));
      request.color = layer.color;
      request.blend_mode = layer.blend_mode;
      quads.push_back(ComposeEffectParticleQuad(request, node_origin, owner));
    }
  }
  return quads;
}

}  // namespace ro::render
