#include "ro/render/effect_grand_cross.h"

#include <array>
#include <cmath>

#include "ro/render/effect_particle_quad.h"
#include "ro/render/effect_scene.h"
#include "ro/render/effect_track.h"
#include "ro/render/effect_trail_palette.h"

#include "effect_track_angles.h"

namespace ro::render {
namespace {

[[nodiscard]] float Cosinus(std::int32_t angle_degrees) {
  return std::cos(DegreesToRadians(static_cast<float>(WithinFullTurn(angle_degrees))));
}

[[nodiscard]] float Sinus(std::int32_t angle_degrees) {
  return std::sin(DegreesToRadians(static_cast<float>(WithinFullTurn(angle_degrees))));
}

// L'ALPHA DES DEUX NŒUDS, ET LA MORT DE LA PISTE.
//
// ⚠ DEUX CONDITIONS SÉPARÉES, ET LES CONFONDRE CHANGE CE QU'ON VOIT. L'alpha
// tombe à zéro dès qu'il a fini de descendre ; la piste ne s'éteint que si le
// tic a AUSSI dépassé le trentième. Entre les deux, la piste vit sans se voir —
// et pour le ruban, c'est là que son rayon repart.
//
// Rend VRAI quand l'alpha vient de s'annuler. Le ruban s'en sert pour repartir ;
// le cadre ne s'en sert pas, et la réponse ne lui est donc pas imposée.
bool AdvanceAlpha(EffectTrack& effect_track, std::int16_t rise) {
  ++effect_track.ticks;
  if (effect_track.ticks < kGrandCrossRiseTicks) {
    effect_track.alpha = static_cast<std::int16_t>(effect_track.alpha + rise);
    return false;
  }
  effect_track.alpha = static_cast<std::int16_t>(effect_track.alpha - 1);
  if (effect_track.alpha > 0) {
    return false;
  }
  effect_track.alpha = 0;
  if (effect_track.ticks > kGrandCrossLastTick) {
    effect_track.active = false;
  }
  return true;
}

// LE BRAS D'UNE PISTE, EN DEGRÉS : le client le choisit par une chaîne de
// comparaisons sur le RANG, et les quatre valeurs ne se suivent pas dans
// l'ordre des rangs. Les ranger par une formule les mélangerait.
[[nodiscard]] std::int32_t ArmAngle(std::size_t rang, std::int32_t angle) {
  switch (rang) {
    case 2:
      return WithinFullTurn(90 - angle);
    case 1:
      return WithinFullTurn(270 - angle);
    case 0:
      return WithinFullTurn(450 - angle);
    default:
      return WithinFullTurn(630 - angle);
  }
}

// LE DÉCALAGE D'UN BRAS, lu dans les deux premières marques de la piste.
//
// ⚠ CES DEUX OCTETS SONT SIGNÉS DANS LE CLIENT, et la piste les range non
// signés. Vingt-trois et moins vingt-trois placent les quatre bras aux quatre
// coins ; les lire non signés en mettrait deux à deux cent trente-trois, très
// loin de la croix.
//
// ⚠ ILS S'ÉCRIVENT DANS LE REPÈRE DU CLIENT, et la profondeur se retourne en
// passant au nôtre, comme celle de l'arc et du bras.
[[nodiscard]] glm::vec3 ArmOffset(const EffectTrack& effect_track) {
  return {static_cast<float>(static_cast<std::int8_t>(effect_track.mark[0])), 0.0F,
          -static_cast<float>(static_cast<std::int8_t>(effect_track.mark[1]))};
}

}  // namespace

void AdvanceGrandCrossRibbonTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks) {
  for (EffectTrack& effect_track : tracks) {
    if (!effect_track.active) {
      continue;
    }
    effect_track.radius_at_anchor -= kGrandCrossRibbonRadiusDecay;
    if (AdvanceAlpha(effect_track, kGrandCrossRibbonAlphaRise)) {
      // ⚠ IL REPART, IL NE S'ARRÊTE PAS. Le rayon et l'inclinaison sont remis à
      // leurs valeurs de départ chaque fois que l'alpha s'annule, que la piste
      // survive ou non. C'est ainsi que le ruban pulse sans qu'aucune boucle ne
      // soit écrite.
      effect_track.radius_at_anchor = kGrandCrossRibbonRadiusReset;
      effect_track.tilt_degrees = kGrandCrossRibbonTiltReset;
    }
    // LA LONGUEUR DU BRAS, la même dans les vingt et une cases : le sinus du
    // tic pris pour un angle en degrés, multiplié par la clé.
    const float length = Sinus(effect_track.ticks) * effect_track.lift_key;
    for (float& track_slot : effect_track.value) {
      track_slot = length;
    }
  }
}

void AdvanceGrandCrossFrameTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks) {
  for (EffectTrack& effect_track : tracks) {
    if (!effect_track.active) {
      continue;
    }
    AdvanceAlpha(effect_track, kGrandCrossFrameAlphaRise);
    // LES DEUX DEMI-EXTENSIONS s'écartent du même pas : le cadre grandit sans
    // changer de proportions.
    effect_track.value[0] += kGrandCrossFrameCornerGrowth;
    effect_track.value[1] += kGrandCrossFrameCornerGrowth;
    effect_track.value[2] = Sinus(effect_track.ticks) * effect_track.lift_key;
  }
}

std::vector<EffectWorldQuad> ComposeGrandCrossRibbonQuads(const EffectTrack& track,
                                                          std::size_t rank, std::uint8_t variant,
                                                          glm::vec3 anchor) {
  std::vector<EffectWorldQuad> quads;
  if (!track.active || track.degrees <= 0) {
    return quads;
  }
  // LA TEINTE ET LE MÉLANGE VIENNENT DE LA PALETTE DU RUBAN, comme pour toute
  // primitive de ruban : voir `effect_trail_palette.h`.
  const EffectTrailVariant tint = EffectTrailVariantFor(variant);
  const std::int32_t step = track.degrees / kGrandCrossRibbonStepDivisor;
  if (step <= 0) {
    return quads;
  }
  const glm::vec3 offset = ArmOffset(track);
  const float layer = std::sin(DegreesToRadians(static_cast<float>(track.tilt_degrees)));
  const float flat = std::cos(DegreesToRadians(static_cast<float>(track.tilt_degrees)));

  glm::vec3 previous_inner{};
  glm::vec3 previous_outer{};
  std::size_t slot = 0;
  for (std::int32_t travelled = 0; travelled < track.degrees; travelled += step) {
    std::int32_t angle = WithinFullTurn(travelled + track.azimuth_degrees);
    // ⚠ UN BALAYAGE COMPLET SE REFERME SUR SON AZIMUT, et c'est le seul cas où
    // l'angle ne suit pas le pas : sans cela le dernier segment laisserait une
    // fente d'un pas dans un anneau censé être clos.
    if (track.degrees == kEffectTrackFullTurnDegrees && slot == kEffectTrackSlots - 1) {
      angle = WithinFullTurn(track.azimuth_degrees);
    }
    // ⚠ LA PROFONDEUR SE RETOURNE AVEC L'ORDONNÉE, pour l'arc comme pour le
    // bras : n'en retourner qu'une ferait de la croix le miroir de la sienne.
    const glm::vec3 inner{track.radius_at_anchor * Cosinus(angle), 0.0F,
                          -track.radius_at_anchor * Sinus(angle)};
    const float arm = track.value[slot];
    const std::int32_t cap = ArmAngle(rank, angle);
    // ⚠ LE BRAS S'ÉLÈVE : le client écrit son bout à une ordonnée négative, dans
    // son repère qui descend. Dans le nôtre, qui monte, le signe s'inverse.
    const glm::vec3 outer{inner.x + (arm * flat * Cosinus(cap)), arm * layer,
                          inner.z - (arm * flat * Sinus(cap))};
    if (slot > 0) {
      EffectWorldQuad quad;
      quad.corners = {anchor + offset + previous_inner, anchor + offset + inner,
                      anchor + offset + outer, anchor + offset + previous_outer};
      // LA BANDE DU SEGMENT qui s'achève ici — celui qui part de la case
      // précédente —, dans l'ordre des coins : l'arc d'abord, le bout du bras
      // ensuite. Voir `kGrandCrossRibbonTextureBands`.
      const float band_start = static_cast<float>(slot - 1) / kGrandCrossRibbonTextureBands;
      const float band_end = static_cast<float>(slot) / kGrandCrossRibbonTextureBands;
      quad.texture_uv = {glm::vec2{band_start, kGrandCrossRibbonTextureAtArc},
                         glm::vec2{band_end, kGrandCrossRibbonTextureAtArc},
                         glm::vec2{band_end, kGrandCrossRibbonTextureAtTip},
                         glm::vec2{band_start, kGrandCrossRibbonTextureAtTip}};
      const std::int16_t track_alpha =
          tint.forced_alpha != 0 ? static_cast<std::int16_t>(tint.forced_alpha) : track.alpha;
      const std::uint8_t alpha = static_cast<std::uint8_t>(
          track_alpha > kEffectTrackAlphaMax ? kEffectTrackAlphaMax : track_alpha);
      quad.tint = {tint.color[0], tint.color[1], tint.color[2], alpha};
      quad.blend_source = kEffectNodeBlendSource;
      quad.blend_destination = tint.blend_destination;
      quads.push_back(quad);
    }
    previous_inner = inner;
    previous_outer = outer;
    if (slot + 1 < kEffectTrackSlots) {
      ++slot;
    }
  }
  return quads;
}

std::vector<EffectWorldQuad> ComposeGrandCrossFrameQuads(const EffectTrack& track,
                                                         glm::vec3 anchor) {
  std::vector<EffectWorldQuad> quads;
  if (!track.active || track.alpha <= 0) {
    return quads;
  }
  const float half_x = track.value[0];
  const float half_z = track.value[1];
  const float height = track.value[2];
  const float cosinus = Cosinus(track.azimuth_degrees);
  const float sinus = Sinus(track.azimuth_degrees);
  const auto rotated = [cosinus, sinus](float x, float z) {
    return glm::vec2{(x * cosinus) - (z * sinus), (z * cosinus) + (x * sinus)};
  };
  // LES QUATRE COINS, dans l'ordre du client : (+,+), (+,-), (-,-), (-,+).
  const std::array<glm::vec2, 4> corners = {rotated(half_x, half_z), rotated(half_x, -half_z),
                                            rotated(-half_x, -half_z), rotated(-half_x, half_z)};
  // ⚠ LES DEUX HAUTEURS SONT CELLES DU CLIENT, dans son repère qui descend : le
  // cadre y part un cran au-dessus du sol et monte. Elles changent de signe
  // dans le nôtre ; recopiées, elles enterraient le cadre.
  const float bottom = -kGrandCrossFrameBaseHeight;
  const float top = height - kGrandCrossFrameBaseHeight;
  // Les coins restent RELATIFS au nœud : le quad projeté y ajoute sa place.
  // ⚠ LA PROFONDEUR DU PLAN SE RETOURNE AVEC LES HAUTEURS : le rectangle tourné
  // s'écrit dans le repère du client, et le garder tel quel le tournerait à
  // l'envers.
  const auto point = [](glm::vec2 plan, float y) { return glm::vec3{plan.x, y, -plan.y}; };
  const std::uint8_t alpha = static_cast<std::uint8_t>(
      track.alpha > kEffectTrackAlphaMax ? kEffectTrackAlphaMax : track.alpha);
  // UN PANNEAU BLANC, l'image entière et RETOURNÉE : voir la déclaration.
  const auto panel = [anchor](const std::array<glm::vec3, 4>& panel_corners,
                              std::uint8_t panel_alpha, std::int16_t blend_mode) {
    EffectParticleQuadRequest request;
    request.corners = panel_corners;
    request.alpha = panel_alpha;
    request.blend_mode = blend_mode;
    request.v_at_a_and_b = 1.0F;
    request.v_at_c_and_d = 0.0F;
    return ComposeEffectParticleQuad(request, anchor);
  };

  quads.reserve(kGrandCrossFrameQuadsPerTrack);
  for (std::size_t side = 0; side < kGrandCrossFrameSideQuads; ++side) {
    const glm::vec2 here = corners[side];
    const glm::vec2 next = corners[(side + 1) % kGrandCrossFrameSideQuads];
    quads.push_back(
        panel({point(here, bottom), point(next, bottom), point(next, top), point(here, top)}, alpha,
              kGrandCrossFrameSideBlendMode));
  }
  // ⚠ LES DEUX DIAGONALES SONT À LA MOITIÉ DE L'ALPHA, et elles sont PLATES :
  // le client les émet avec les quatre coins du bas, sans hauteur, et en mode
  // zéro — l'ADDITION — là où les quatre côtés se mélangent à l'ordinaire. Les
  // peindre à plein alpha ferait du cadre une boîte pleine.
  const auto half_alpha = static_cast<std::uint8_t>(alpha / 2);
  quads.push_back(panel({point(corners[0], bottom), point(corners[1], bottom),
                         point(corners[2], bottom), point(corners[3], bottom)},
                        half_alpha, kGrandCrossFrameFloorBlendMode));
  quads.push_back(panel({point(corners[3], bottom), point(corners[2], bottom),
                         point(corners[1], bottom), point(corners[0], bottom)},
                        half_alpha, kGrandCrossFrameFloorBlendMode));
  return quads;
}

}  // namespace ro::render
