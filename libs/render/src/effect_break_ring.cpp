#include "ro/render/effect_break_ring.h"

#include <algorithm>

#include "ro/render/effect_scene.h"
#include "ro/render/effect_track.h"
#include "ro/render/effect_trail_palette.h"

#include "effect_track_angles.h"

namespace ro::render {
namespace {

// Combien de bandes de texture la couronne compte : une de plus que ses
// segments, comme la jupe.
constexpr std::int32_t kBands = kBreakRingSegments + 1;

// La coordonnée de texture au travers du ruban : le bord intérieur porte le
// haut de l'image, l'extérieur le bas.
constexpr float kTextureAtInnerEdge = 1.0F;
constexpr float kTextureAtOuterEdge = 0.0F;

}  // namespace

void AdvanceBreakRingTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks) {
  for (EffectTrack& effect_track : tracks) {
    if (!effect_track.active) {
      continue;
    }
    ++effect_track.ticks;
    if (effect_track.tilt_degrees < 0) {
      effect_track.tilt_degrees = 0;
    }
    if (effect_track.azimuth_degrees >= kEffectTrackFullTurnDegrees) {
      effect_track.azimuth_degrees =
          static_cast<std::int16_t>(effect_track.azimuth_degrees - kEffectTrackFullTurnDegrees);
    }
    if (effect_track.ticks < kBreakRingRisesBeforeTick) {
      const bool living = effect_track.style == kBreakRingBrightStyle;
      effect_track.alpha = std::min<std::int16_t>(
          static_cast<std::int16_t>(effect_track.alpha +
                                    (living ? kBreakRingBrightRise : kBreakRingDimRise)),
          living ? kBreakRingBrightCeiling : kBreakRingDimCeiling);
    } else {
      effect_track.alpha = static_cast<std::int16_t>(effect_track.alpha - kBreakRingAlphaFall);
      // ⚠ SOUS ZÉRO, LA PISTE MEURT.
      if (effect_track.alpha < 0) {
        effect_track.alpha = 0;
        effect_track.active = false;
      }
    }
  }
}

void ComposeBreakRingQuads(const EffectTrack& track, std::uint8_t variant, glm::vec3 anchor,
                           std::vector<EffectWorldQuad>& quads) {
  if (!track.active || track.alpha <= 0) {
    return;
  }
  const std::int32_t sweep_degrees = track.degrees;
  const std::int32_t step = sweep_degrees / kBreakRingSegments;
  // ⚠ UN PAS NUL FIGERAIT LA BOUCLE DU CLIENT ; le bâtisseur pose trois cent
  // soixante, et nous rendons rien plutôt que de tourner sans fin.
  if (step <= 0) {
    return;
  }
  const EffectTrailVariant tint = EffectTrailVariantFor(variant);
  const std::int16_t alpha = tint.forced_alpha != 0 ? tint.forced_alpha : track.alpha;
  const float offset = track.lift_key * CosOfDegrees(track.tilt_degrees);
  const float leve = (track.lift_key * SinOfDegrees(track.tilt_degrees)) + kBreakRingLift;

  glm::vec3 previous_inner{0.0F, 0.0F, 0.0F};
  glm::vec3 previous_outer{0.0F, 0.0F, 0.0F};
  std::int32_t segment = 0;
  for (std::int32_t travelled = 0; travelled <= sweep_degrees; travelled += step) {
    // LE DERNIER SEGMENT REPREND L'AZIMUT DE DÉPART quand le tour est entier.
    const bool referme =
        sweep_degrees == kEffectTrackFullTurnDegrees && segment == kBreakRingSegments;
    std::int32_t angle = track.azimuth_degrees + travelled;
    if (angle >= kEffectTrackFullTurnDegrees) {
      angle -= kEffectTrackFullTurnDegrees;
    }
    if (referme) {
      angle = track.azimuth_degrees;
    }
    const float vers_x = CosOfDegrees(angle);
    // LA PROFONDEUR SE RETOURNE AVEC L'ORDONNÉE : le passage au nôtre est un
    // demi-tour autour de X, et l'anneau tournerait sinon à l'envers.
    const float vers_z = -SinOfDegrees(angle);
    const glm::vec3 inner = anchor + glm::vec3{track.radius_at_anchor * vers_x, kBreakRingLift,
                                               track.radius_at_anchor * vers_z};
    const glm::vec3 outer =
        inner + glm::vec3{offset * vers_x, leve - kBreakRingLift, offset * vers_z};
    if (segment > 0) {
      const float u_before = static_cast<float>(segment - 1) / static_cast<float>(kBands);
      const float u_after = static_cast<float>(segment) / static_cast<float>(kBands);
      EffectWorldQuad quad;
      quad.corners = {previous_inner, inner, outer, previous_outer};
      quad.texture_uv = {
          glm::vec2{u_before, kTextureAtInnerEdge}, glm::vec2{u_after, kTextureAtInnerEdge},
          glm::vec2{u_after, kTextureAtOuterEdge}, glm::vec2{u_before, kTextureAtOuterEdge}};
      quad.tint = {tint.color[0], tint.color[1], tint.color[2], static_cast<std::uint8_t>(alpha)};
      quad.blend_source = kEffectNodeBlendSource;
      quad.blend_destination = tint.blend_destination;
      quads.push_back(quad);
    }
    previous_inner = inner;
    previous_outer = outer;
    // ⚠ L'INDEX DE SEGMENT SE BLOQUE AU VINGTIÈME : un balayage qui n'est pas
    // multiple de vingt donne des échantillons de plus, qui repeignent le dernier.
    segment = std::min(segment + 1, kBreakRingSegments);
  }
}

}  // namespace ro::render
