#include "ro/render/effect_sheathed_bolt.h"

#include <algorithm>
#include <cstddef>

#include "ro/render/effect_track.h"

#include "effect_track_angles.h"

namespace ro::render {
namespace {

// LES TROIS RAMPES D'ALPHA, et le tic où chacune bascule de la montée à la
// chute, au cas 69 du répartiteur.
constexpr std::int32_t kDimRiseTicks = 20;
constexpr std::int16_t kDimRise = 8;
constexpr std::int16_t kDimFall = 2;
constexpr std::int32_t kOtherRiseTicks = 5;
constexpr std::int16_t kWhiteRise = 2;
constexpr std::int16_t kWhiteFall = 1;
constexpr std::int16_t kPlainRise = 8;
constexpr std::int16_t kPlainFall = 4;

// UN SOMMET du pentagone, sur l'un des deux cercles.
//
// `ring_degrees` est le cap du plan du cercle — l'azimut plus un quart de tour.
// `lift` est de combien le cercle entier MONTE au-dessus de son centre.
[[nodiscard]] glm::vec3 Vertex(const glm::vec3& centre, std::int32_t rang, float radius,
                               std::int32_t ring_degrees, float lift) {
  const std::int32_t angle = kSheathedBoltStepDegrees * rang;
  const float radial = radius * CosOfDegrees(angle);
  // ⚠ NOTRE REPÈRE MONTE, CELUI DU CLIENT DESCEND : là où il retranche la
  // hauteur pour élever le trait, nous l'ajoutons, et le pentagone se retourne
  // avec elle. Recopier le signe enterrerait le trait sous le sol.
  const float top = lift - radius * SinOfDegrees(angle);
  return {centre.x + radial * CosOfDegrees(ring_degrees), centre.y + top,
          centre.z + radial * SinOfDegrees(ring_degrees)};
}

// L'ALPHA D'UNE PISTE AU TIC SUIVANT, selon son style.
[[nodiscard]] std::int16_t NextTickAlpha(float style, std::int16_t alpha, std::int32_t ticks) {
  if (style == kSheathedBoltDimStyle) {
    return ticks <= kDimRiseTicks ? static_cast<std::int16_t>(alpha + kDimRise)
                                  : static_cast<std::int16_t>(alpha - kDimFall);
  }
  if (ticks <= kOtherRiseTicks) {
    return style == kSheathedBoltWhiteStyle ? static_cast<std::int16_t>(alpha + kWhiteRise)
                                            : static_cast<std::int16_t>(alpha + kPlainRise);
  }
  return style == kSheathedBoltWhiteStyle ? static_cast<std::int16_t>(alpha - kWhiteFall)
                                          : static_cast<std::int16_t>(alpha - kPlainFall);
}

}  // namespace

std::array<std::uint8_t, 3> EffectSheathedBoltColor(float style, bool sheath) {
  if (style == kSheathedBoltWhiteStyle) {
    return kSheathedBoltWhite;
  }
  if (style == kSheathedBoltPinkStyle) {
    return kSheathedBoltPink;
  }
  if (style == kSheathedBoltBlueStyle) {
    return kSheathedBoltBlue;
  }
  if (style == kSheathedBoltDimStyle) {
    return kSheathedBoltDim;
  }
  return sheath ? kSheathedBoltSheathDefault : kSheathedBoltCoreDefault;
}

float EffectSheathedBoltRadius(float style, float core_radius, bool sheath) {
  if (!sheath) {
    return core_radius;
  }
  return style == kSheathedBoltDoubledSheathStyle ? core_radius + core_radius
                                                  : core_radius + kSheathedBoltSheathGrowth;
}

std::vector<EffectWorldQuad> ComposeSheathedBoltQuads(const EffectTrack& track,
                                                      const EffectOwnerTint& owner) {
  std::vector<EffectWorldQuad> quads;
  if (!track.active || track.alpha <= 0) {
    return quads;
  }

  const float style = track.value[kSheathedBoltSlotOfStyle];
  const float lift = track.value[kSheathedBoltSlotOfLift];
  // LE CAP DU TRAIT EST DANS NOTRE REPÈRE, où les angles tournent à l'opposé
  // de ceux du client : le quart de tour de la coupe s'y RETRANCHE. L'ajouter
  // retournait le pentagone, pointe de l'autre côté du trait.
  const std::int32_t ring_heading =
      WithinFullTurn(track.azimuth_degrees - kSheathedBoltRingQuarterTurn);
  const glm::vec3 pres = track.anchor;
  const glm::vec3 far_point{track.tail[kSheathedBoltSlotOfFarX],
                            track.tail[kSheathedBoltSlotOfFarY],
                            track.tail[kSheathedBoltSlotOfFarZ]};

  quads.reserve(kSheathedBoltPasses * kSheathedBoltSides);
  for (std::size_t passe = 0; passe < kSheathedBoltPasses; ++passe) {
    const bool gaine = passe == 1;
    const float radius = EffectSheathedBoltRadius(style, track.lift_key, gaine);
    for (std::int32_t cote = 1; cote <= static_cast<std::int32_t>(kSheathedBoltSides); ++cote) {
      EffectParticleQuadRequest request;
      request.corners = {Vertex(pres, cote - 1, radius, ring_heading, lift),
                         Vertex(pres, cote, radius, ring_heading, lift),
                         Vertex(far_point, cote, radius, ring_heading, lift),
                         Vertex(far_point, cote - 1, radius, ring_heading, lift)};
      // ⚠ ABSOLUS : les deux centres sont déjà dans le monde.
      request.absolute = true;
      request.alpha = static_cast<std::uint8_t>(track.alpha);
      request.color = EffectSheathedBoltColor(style, gaine);
      request.blend_mode = gaine ? kSheathedBoltSheathBlend : kSheathedBoltCoreBlend;
      // ⚠ L'IMAGE EST RETOURNÉE : le client donne un aux deux premiers coins et
      // zéro aux deux derniers, là où le défaut fait l'inverse.
      request.v_at_a_and_b = 1.0F;
      request.v_at_c_and_d = 0.0F;
      // ⚠ LE BLEU ET LE SOMBRE PASSENT DEVANT TOUT, aux deux passes ; le blanc,
      // le rose et le style inconnu gardent leur profondeur.
      request.depth = style == kSheathedBoltBlueStyle || style == kSheathedBoltDimStyle
                          ? EffectQuadDepth::kFrontmost
                          : EffectQuadDepth::kProjected;
      quads.push_back(ComposeEffectParticleQuad(request, glm::vec3{0.0F, 0.0F, 0.0F}, owner));
    }
  }
  return quads;
}

void AdvanceSheathedBoltTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks) {
  for (EffectTrack& effect_track : tracks) {
    if (!effect_track.active) {
      continue;
    }
    ++effect_track.ticks;
    // Tant que le compteur est négatif, la piste attend : c'est le retard que
    // le bâtisseur a tiré au sort.
    if (effect_track.ticks <= 0) {
      continue;
    }
    // ⚠ LE CLIENT NE BORNE QUE PAR LE BAS, et jamais par le haut : ses montées
    // sont trop courtes pour déborder — cent soixante au plus. Ajouter un
    // plafond serait inventer une règle.
    effect_track.alpha =
        std::max<std::int16_t>(NextTickAlpha(effect_track.value[kSheathedBoltSlotOfStyle],
                                             effect_track.alpha, effect_track.ticks),
                               0);
    // LE BOUT LOINTAIN, recalculé à chaque tic et toujours égal à lui-même. La
    // hauteur n'est pas écrite : elle reste celle que le semis a posée.
    effect_track.tail[kSheathedBoltSlotOfFarX] =
        effect_track.anchor.x +
        effect_track.radius_at_anchor * CosOfDegrees(effect_track.azimuth_degrees);
    effect_track.tail[kSheathedBoltSlotOfFarZ] =
        effect_track.anchor.z +
        effect_track.radius_at_anchor * SinOfDegrees(effect_track.azimuth_degrees);
  }
}

}  // namespace ro::render
