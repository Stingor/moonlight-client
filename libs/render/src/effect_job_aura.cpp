#include "ro/render/effect_job_aura.h"

#include <algorithm>
#include <cstddef>

#include "ro/render/effect_node.h"
#include "ro/render/effect_quad_rotation.h"
#include "ro/render/effect_track.h"

#include "effect_track_angles.h"

namespace ro::render {
namespace {

[[nodiscard]] std::int32_t DrawBelow(std::uint32_t& rng_state, std::int32_t exclusive_bound) {
  return static_cast<std::int32_t>(NextEffectNodeDraw(rng_state) %
                                   static_cast<std::uint32_t>(exclusive_bound));
}

[[nodiscard]] std::size_t ValueSlotOf(std::size_t particle, std::size_t rang) {
  return kJobAuraValuesPerParticle * particle + rang;
}

// LE CARRÉ DE L'AURA, à plat puis couché et tourné par les deux angles de
// caméra. Sa demi-diagonale est l'échelle de la couche fois le rayon à l'ancre.
[[nodiscard]] std::array<glm::vec3, 4> AuraSquare(float half_diagonal, std::int16_t tour,
                                                  std::int16_t tilt) {
  std::array<glm::vec3, 4> quad;
  for (std::size_t corner = 0; corner < quad.size(); ++corner) {
    const std::int32_t angle =
        kJobAuraFirstCornerDegrees + kJobAuraDegreesPerCorner * static_cast<std::int32_t>(corner);
    quad[corner] = {half_diagonal * CosOfDegrees(angle), 0.0F, half_diagonal * SinOfDegrees(angle)};
  }
  RotateEffectQuadCorners(quad, tour, tilt);
  return quad;
}

// OÙ UNE PARTICULE SE TROUVE : sur un cercle de rayon fixe, à la hauteur que le
// mouvement lui a donnée.
//
// ⚠ LE CLIENT RANGE CETTE POSITION DANS LA PISTE, et nous ne le faisons pas.
// Son dessinateur recalcule l'abscisse et la profondeur depuis l'angle à chaque
// image, puis les réécrit par-dessus les mêmes valeurs : l'écriture est
// idempotente et personne d'autre ne la relit — le mouvement ne touche que la
// hauteur. La garder ferait muter un état que la figure est seule à connaître.
[[nodiscard]] glm::vec3 ParticlePosition(const EffectTrack& effect_track, std::size_t particle) {
  const float angle = effect_track.value[ValueSlotOf(particle, kJobAuraValueOfAngle)];
  const float height = EffectFourParticleOffsetOf(effect_track, particle).y;
  return {CosOfDegrees(static_cast<std::int32_t>(angle)) * kJobAuraOrbitRadius, height,
          SinOfDegrees(static_cast<std::int32_t>(angle)) * kJobAuraOrbitRadius};
}

// UN CARRÉ DE L'AURA, posé sur sa particule.
[[nodiscard]] EffectWorldQuad ParticleQuad(const EffectTrack& effect_track, std::size_t particle,
                                           const std::array<glm::vec3, 4>& quad,
                                           std::array<std::uint8_t, 3> tint, std::int32_t alpha,
                                           std::int16_t tour, glm::vec3 anchor_point,
                                           const EffectOwnerTint& owner) {
  const glm::vec3 centre = ParticlePosition(effect_track, particle);
  const float angle = effect_track.value[ValueSlotOf(particle, kJobAuraValueOfAngle)];
  const auto image =
      static_cast<std::int32_t>(effect_track.value[ValueSlotOf(particle, kJobAuraValueOfFrame)]);
  const float aura_lift_height = effect_track.value[kJobAuraValueOfHeight];

  // L'IMAGE SE RETOURNE en échangeant les coins deux à deux : A avec B, C avec
  // D. C'est ce que fait le client, et non une symétrie de ses nombres de
  // texture.
  std::array<std::size_t, 4> order = {0, 1, 2, 3};
  if (EffectJobAuraFlips(angle, tour, image)) {
    order = {1, 0, 3, 2};
  }

  EffectParticleQuadRequest request;
  for (std::size_t corner = 0; corner < request.corners.size(); ++corner) {
    const glm::vec3& pose = quad[order[corner]];
    // ⚠ TOUT SE CALCULE DANS LE REPÈRE DU CLIENT — le carré tourné par les
    // angles de SA caméra, la particule sur son cercle, la hauteur de l'aura —
    // et revient au nôtre ENSEMBLE, par `S` entier, au dernier moment.
    // Retrancher la hauteur de l'aura ÉLÈVE donc le carré.
    request.corners[corner] = EffectPointFromClient(
        {pose.x + centre.x, pose.y + centre.y - aura_lift_height, pose.z + centre.z});
  }
  request.alpha = static_cast<std::uint8_t>(std::clamp<std::int32_t>(alpha, 0, 255));
  request.color = tint;
  request.texture = static_cast<std::size_t>(std::max(image, 0));
  // ⚠ L'IMAGE EST RETOURNÉE EN V, comme presque partout dans la voie A.
  request.v_at_a_and_b = 1.0F;
  request.v_at_c_and_d = 0.0F;
  // ⚠ L'AURA S'AVANCE D'UN SOUFFLE vers l'œil : voir `EffectQuadDepth::kNudged`.
  request.depth = EffectQuadDepth::kNudged;
  return ComposeEffectParticleQuad(request, anchor_point, owner);
}

}  // namespace

bool EffectJobAuraFlips(float particle_degrees, std::int16_t camera_turn_degrees,
                        std::int32_t frame) {
  const auto offset =
      static_cast<std::int32_t>(particle_degrees - static_cast<float>(camera_turn_degrees));
  // ⚠ LES DEUX SEUILS NE SONT PAS SYMÉTRIQUES — soixante-dix d'un côté,
  // quatre-vingts de l'autre — et l'écart doit en plus rester dans un tour et
  // demi. Les arrondir au même nombre décalerait le retournement de dix degrés.
  const bool far_point = (offset >= kJobAuraFlipAbove || offset <= kJobAuraFlipBelow) &&
                         offset >= -kJobAuraFlipHalfRange && offset <= kJobAuraFlipHalfRange;
  const bool inverse =
      frame == kJobAuraFrameThatInvertsFlipFirst || frame == kJobAuraFrameThatInvertsFlipSecond;
  return inverse ? far_point : !far_point;
}

const EffectJobAuraPalette& EffectJobAuraPaletteOf(std::uint8_t mark) {
  if (mark == kJobAuraBlueMark) {
    return kJobAuraBluePalette;
  }
  if (mark == kJobAuraPaleMark) {
    return kJobAuraPalePalette;
  }
  return kJobAuraRedPalette;
}

std::vector<EffectWorldQuad> ComposeLayeredAuraQuads(const EffectTrack& track,
                                                     std::int16_t turn_degrees,
                                                     std::int16_t tilt_degrees, glm::vec3 anchor,
                                                     const EffectOwnerTint& owner) {
  std::vector<EffectWorldQuad> quads;
  if (!track.active) {
    return quads;
  }
  const EffectJobAuraPalette& palette = EffectJobAuraPaletteOf(track.mark[kJobAuraMarkOfPalette]);
  for (std::size_t layer = 0; layer < kJobAuraLayers; ++layer) {
    const std::array<glm::vec3, 4> quad =
        AuraSquare(kJobAuraLayerScales[layer] * track.radius_at_anchor, turn_degrees, tilt_degrees);
    for (std::size_t particle = 0; particle < kJobAuraParticles; ++particle) {
      // ⚠ L'ALPHA EST PLAFONNÉ PAR LES TICS DE LA PISTE, et c'est ce qui fait
      // monter l'aura en fondu au lieu de la faire éclater. Le plafond ne mord
      // que sur les premiers tics, mais il mord sur TOUTES les particules.
      const auto carries =
          static_cast<std::int32_t>(track.value[ValueSlotOf(particle, kJobAuraValueOfAlpha)]);
      const std::int32_t alpha = std::min(carries, track.ticks);
      quads.push_back(
          ParticleQuad(track, particle, quad, palette[layer], alpha, turn_degrees, anchor, owner));
    }
  }
  return quads;
}

std::vector<EffectWorldQuad> ComposePlainAuraQuads(const EffectTrack& track,
                                                   std::int16_t turn_degrees,
                                                   std::int16_t tilt_degrees, glm::vec3 anchor,
                                                   std::array<std::uint8_t, 3> color,
                                                   const EffectOwnerTint& owner) {
  std::vector<EffectWorldQuad> quads;
  if (!track.active) {
    return quads;
  }
  const std::array<glm::vec3, 4> quad =
      AuraSquare(kPlainAuraScale * track.radius_at_anchor, turn_degrees, tilt_degrees);
  for (std::size_t particle = 0; particle < kJobAuraParticles; ++particle) {
    // ⚠ ET CELUI-CI NE PLAFONNE RIEN. Le sous-type 35 passe l'alpha de sa
    // particule tel quel : il paraît à plein dès son premier tic.
    const auto alpha =
        static_cast<std::int32_t>(track.value[ValueSlotOf(particle, kJobAuraValueOfAlpha)]);
    quads.push_back(ParticleQuad(track, particle, quad, color, alpha, turn_degrees, anchor, owner));
  }
  return quads;
}

std::vector<EffectWorldQuad> ComposePlainAuraNodeQuads(
    const std::array<EffectTrack, kEffectTracksPerNode>& tracks, std::int16_t turn_degrees,
    std::int16_t tilt_degrees, glm::vec3 anchor, const EffectOwnerTint& owner) {
  std::vector<EffectWorldQuad> quads;
  // ⚠ LA TEINTE DÉBORDE D'UNE PISTE SUR LES SUIVANTES. Le dessinateur du client
  // écrase ses propres PARAMÈTRES de teinte et ne les remet jamais : dès qu'une
  // piste porte la marque rouge, toutes celles qui viennent après sont rouges
  // aussi, quelle que soit LEUR marque. C'est pour cela que le nœud se compose
  // d'un coup et non piste par piste.
  std::array<std::uint8_t, 3> tint = kPlainAuraColor;
  for (const EffectTrack& effect_track : tracks) {
    if (!effect_track.active) {
      continue;
    }
    if (effect_track.mark[kJobAuraMarkOfPalette] == kPlainAuraRedMark) {
      tint = kPlainAuraRed;
    }
    const std::vector<EffectWorldQuad> carres =
        ComposePlainAuraQuads(effect_track, turn_degrees, tilt_degrees, anchor, tint, owner);
    quads.insert(quads.end(), carres.begin(), carres.end());
  }
  return quads;
}

void AdvanceJobAuraTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                          std::int16_t camera_turn_degrees, std::int16_t camera_tilt_degrees,
                          float aura_height, std::uint32_t& generator) {
  // ⚠ LES DEUX ANGLES DE CAMÉRA VIVENT DANS DES CASES DE STYLE, celles des
  // pistes zéro et un. Ce ne sont pas des styles, et aucune piste ne les lit
  // comme tels — c'est le dessinateur qui va les y relire. ADR 0017.
  tracks[0].style = camera_turn_degrees;
  tracks[1].style = camera_tilt_degrees;

  for (EffectTrack& effect_track : tracks) {
    // ⚠ LE COMPTEUR N'AVANCE QUE SUR UNE PISTE VIVE, au contraire des lucioles :
    // le client teste avant d'incrémenter. Et c'est ce compteur qui plafonne
    // l'alpha du sous-type 99.
    if (!effect_track.active) {
      continue;
    }
    ++effect_track.ticks;

    for (std::size_t particle = 0; particle < kJobAuraParticles; ++particle) {
      float& phase = effect_track.value[ValueSlotOf(particle, kJobAuraValueOfPhase)];
      float& alpha = effect_track.value[ValueSlotOf(particle, kJobAuraValueOfAlpha)];
      phase += 1.0F;
      if (phase < kJobAuraRisePhases) {
        alpha += kJobAuraAlphaRise;
        continue;
      }
      alpha -= kJobAuraAlphaFall;
      if (alpha >= kJobAuraRebirthBelow) {
        continue;
      }
      // LA RENAISSANCE : la particule repart d'en haut, sous un autre angle et
      // sur une autre image.
      phase = 0.0F;
      alpha = 0.0F;
      effect_track.value[ValueSlotOf(particle, kJobAuraValueOfAngle)] =
          static_cast<float>(DrawBelow(generator, kEffectTrackFullTurnDegrees));
      effect_track.value[ValueSlotOf(particle, kJobAuraValueOfFrame)] =
          static_cast<float>(DrawBelow(generator, kJobAuraFrameCount));
      glm::vec3 place = EffectFourParticleOffsetOf(effect_track, particle);
      place.y = static_cast<float>(kJobAuraRebirthHeightFloor -
                                   DrawBelow(generator, kJobAuraRebirthHeightSpread));
      SetEffectFourParticleOffset(effect_track, particle, place);
    }

    effect_track.value[kJobAuraValueOfHeight] = aura_height;
  }
}

}  // namespace ro::render
