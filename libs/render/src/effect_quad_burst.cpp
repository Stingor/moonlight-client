#include "ro/render/effect_quad_burst.h"

#include <algorithm>

#include "ro/render/effect_particle_quad.h"
#include "ro/render/effect_quad_rotation.h"
#include "ro/render/effect_track.h"

#include "effect_track_angles.h"

namespace ro::render {
namespace {

// OÙ CHAQUE PARTICULE RANGE SA POSITION, dans l'ordre du client. La deuxième
// est dans l'ANCRE et non dans la queue ; les autres sont aux cases trois,
// six et ZÉRO.
constexpr std::size_t kSlotOfFirstTail = 3;
constexpr std::size_t kSlotOfThirdTail = 6;
constexpr std::size_t kSlotOfFourthTail = 0;
constexpr std::size_t kParticleInTheAnchor = 1;

[[nodiscard]] std::size_t FirstTailSlot(std::size_t particle_index) {
  switch (particle_index) {
    case 0:
      return kSlotOfFirstTail;
    case 2:
      return kSlotOfThirdTail;
    default:
      return kSlotOfFourthTail;
  }
}

// LES DEUX SEUILS DE LA RAMPE ORDINAIRE, et le pas qui la remonte.
constexpr float kOrdinaryFloor = -20.0F;
constexpr float kGreyFloorOnFirst = -40.0F;
constexpr std::int16_t kOrdinaryCeiling = 250;
constexpr float kOrdinaryStep = 25.0F;

// LA RAMPE DES DEUX MARQUES QUI MONTENT : elle tombe en haut et s'éteint en bas.
constexpr std::int16_t kRisingPlateau = 100;
constexpr float kRisingTop = -10.0F;
constexpr float kRisingBottom = -40.0F;
constexpr float kRisingCeiling = 150.0F;
constexpr float kRisingStep = 15.0F;

// LA RAMPE PAR DÉFAUT : un plateau bas, une montée sous le seuil, et une
// extinction franche au ras du nœud.
constexpr std::int16_t kDefaultPlateau = 50;
constexpr float kDefaultFloor = -4.0F;
constexpr float kDefaultStep = 15.0F;
constexpr float kDefaultEdge = -0.5F;
constexpr float kDefaultEdgeStep = -100.0F;

// DE COMBIEN LA MARQUE QUINZE S'EFFACE par cran de sa propre marque.
constexpr std::int16_t kFadingFull = 255;
constexpr std::int16_t kFadingPerStep = 2;

[[nodiscard]] std::int16_t Plancher(float value) {
  const auto integer = static_cast<std::int32_t>(value);
  return static_cast<std::int16_t>(integer < 0 ? 0 : integer);
}

}  // namespace

EffectQuadBurstOffset EffectQuadBurstOffsetOf(const EffectTrack& track, std::size_t particle) {
  if (particle == kParticleInTheAnchor) {
    return {track.anchor.x, track.anchor.y, track.anchor.z};
  }
  const std::size_t base = FirstTailSlot(particle);
  return {track.tail[base], track.tail[base + 1], track.tail[base + 2]};
}

void SetEffectQuadBurstOffset(EffectTrack& track, std::size_t particle,
                              const EffectQuadBurstOffset& offset) {
  if (particle == kParticleInTheAnchor) {
    track.anchor = {offset.sideways, offset.depth_below, offset.forward};
    return;
  }
  const std::size_t base = FirstTailSlot(particle);
  track.tail[base] = offset.sideways;
  track.tail[base + 1] = offset.depth_below;
  track.tail[base + 2] = offset.forward;
}

std::int16_t EffectQuadBurstAlpha(std::uint8_t mark, float depth_below, std::uint8_t fade_mark,
                                  bool first_particle) {
  if (mark == kQuadBurstMarkFading) {
    // ⚠ CELLE-CI NE REGARDE PAS SA HAUTEUR : son alpha ne dépend que de sa
    // propre marque, que le mouvement fait monter par crans de cinq.
    return std::max<std::int16_t>(
        0, static_cast<std::int16_t>(kFadingFull - kFadingPerStep * fade_mark));
  }
  if (mark == kQuadBurstMarkRising || mark == kQuadBurstMarkScattering) {
    if (depth_below > kRisingTop) {
      return Plancher(kRisingCeiling - (depth_below - kRisingTop) * kRisingStep);
    }
    return depth_below < kRisingBottom ? 0 : kRisingPlateau;
  }
  // ⚠ LA MARQUE ONZE N'A PAS LE MÊME SEUIL SUR LA PREMIÈRE PARTICULE que sur
  // les trois autres : moins quarante chez elle, moins vingt chez elles. C'est
  // une asymétrie du client, et la lisser ferait apparaître la première vingt
  // unités trop tôt.
  if (mark <= kQuadBurstMarkBlue || mark == kQuadBurstMarkGrey) {
    const float seuil =
        first_particle && mark == kQuadBurstMarkGrey ? kGreyFloorOnFirst : kOrdinaryFloor;
    if (depth_below >= seuil) {
      return kOrdinaryCeiling;
    }
    return Plancher((depth_below - seuil) * kOrdinaryStep + static_cast<float>(kOrdinaryCeiling));
  }
  if (depth_below > kDefaultEdge) {
    return Plancher(depth_below * kDefaultEdgeStep);
  }
  if (depth_below < kDefaultFloor) {
    return Plancher((depth_below - kDefaultFloor) * kDefaultStep +
                    static_cast<float>(kDefaultPlateau));
  }
  return kDefaultPlateau;
}

std::array<std::uint8_t, 3> EffectQuadBurstColor(std::uint8_t mark, std::uint8_t fade_mark) {
  switch (mark) {
    case kQuadBurstMarkWhite:
    case kQuadBurstMarkPlainWhite:
    case kQuadBurstMarkRising:
      return kQuadBurstWhite;
    case kQuadBurstMarkBlue:
      return kQuadBurstBlue;
    case kQuadBurstMarkYellow:
      return kQuadBurstYellow;
    case kQuadBurstMarkGrey:
      return kQuadBurstGrey;
    case kQuadBurstMarkFading:
      if (fade_mark == kQuadBurstShadeOrange) {
        return kQuadBurstOrange;
      }
      if (fade_mark == kQuadBurstShadeYellow) {
        return kQuadBurstLemon;
      }
      return kQuadBurstGreen;
    default:
      return kQuadBurstGreen;
  }
}

std::size_t EffectQuadBurstPasses(std::uint8_t mark) {
  return mark == kQuadBurstMarkWhite || mark == kQuadBurstMarkBlue ? 2 : 1;
}

std::vector<EffectWorldQuad> ComposeQuadBurstQuads(const EffectTrack& track,
                                                   std::int16_t turn_degrees,
                                                   std::int16_t tilt_degrees, glm::vec3 anchor) {
  std::vector<EffectWorldQuad> quads;
  if (!track.active) {
    return quads;
  }

  // LE CARRÉ DE DÉPART : à plat, tourné d'un huitième de tour, de la taille du
  // rayon à l'ancre — puis couché et tourné par les deux angles de caméra.
  std::array<glm::vec3, 4> world_quad;
  for (std::size_t corner = 0; corner < world_quad.size(); ++corner) {
    const std::int32_t angle = kQuadBurstFirstCornerDegrees +
                               kQuadBurstDegreesPerCorner * static_cast<std::int32_t>(corner);
    world_quad[corner] = {track.radius_at_anchor * CosOfDegrees(angle), 0.0F,
                          track.radius_at_anchor * SinOfDegrees(angle)};
  }
  RotateEffectQuadCorners(world_quad, turn_degrees, tilt_degrees);

  const std::uint8_t style_mark = track.mark[0];
  for (std::size_t particle_index = 0; particle_index < kQuadBurstParticles; ++particle_index) {
    const EffectQuadBurstOffset shift = EffectQuadBurstOffsetOf(track, particle_index);
    // ⚠ LA HAUTEUR QUI DÉCIDE DE TOUT EST CELLE DU PREMIER COIN, pas celle du
    // centre : le client compare le coin A translaté, et rien d'autre.
    const float height = world_quad[0].y + shift.depth_below;
    const std::int16_t alpha = EffectQuadBurstAlpha(
        style_mark, height, track.mark[particle_index + 1], particle_index == 0);
    // ⚠ LA MARQUE QUINZE SE PEINT QUELLE QUE SOIT SA HAUTEUR ; toutes les
    // autres attendent d'être au-dessus du nœud. Le client la traite avant sa
    // garde, et l'y soumettre la ferait disparaître la moitié du temps.
    if (style_mark != kQuadBurstMarkFading && height > 0.0F) {
      continue;
    }
    // ⚠ ET LA MARQUE ONZE NE PEINT QUE SA PREMIÈRE PARTICULE. Les trois autres
    // sont calculées et jetées ; les peindre quadruplerait le gris.
    if (style_mark == kQuadBurstMarkGrey && particle_index != 0) {
      continue;
    }

    EffectParticleQuadRequest request;
    for (std::size_t corner = 0; corner < world_quad.size(); ++corner) {
      // ⚠ LE COIN ET SON DÉCALAGE SONT DANS LE REPÈRE DU CLIENT, tournés par
      // les angles de SA caméra : ils reviennent au nôtre ENSEMBLE, par `S`
      // entier, au dernier moment.
      request.corners[corner] = EffectPointFromClient({world_quad[corner].x + shift.sideways,
                                                       world_quad[corner].y + shift.depth_below,
                                                       world_quad[corner].z + shift.forward});
    }
    request.alpha = static_cast<std::uint8_t>(alpha);
    request.color = EffectQuadBurstColor(style_mark, track.mark[particle_index + 1]);
    request.v_at_a_and_b = 1.0F;
    request.v_at_c_and_d = 0.0F;
    const EffectWorldQuad quad = ComposeEffectParticleQuad(request, anchor);
    for (std::size_t passe = 0; passe < EffectQuadBurstPasses(style_mark); ++passe) {
      quads.push_back(quad);
    }
  }
  return quads;
}

}  // namespace ro::render
