#include "ro/render/effect_scrolling_band.h"

#include <cstddef>

#include "ro/render/effect_track.h"

namespace ro::render {
namespace {

// LES DEUX STYLES QUI TEINTENT, relevés sur les trois branches de couleur du
// dessinateur. Le troisième canal vaut toujours deux cent cinquante-cinq : le
// client le pousse en dur, et seuls les deux premiers changent.
constexpr std::uint8_t kStyleDeepWater = 1;
constexpr std::uint8_t kStyleFlare = 10;

// Le mode de mélange que la voie A entend comme une ADDITION.
constexpr std::int16_t kAddedBlend = 0;
// Celui qu'elle entend comme le mélange ordinaire par l'alpha de la source.
constexpr std::int16_t kOrdinaryBlend = 1;

// LE CANAL ASSOMBRI que les deux styles teintés partagent.
constexpr std::uint8_t kDimmedChannel = 55;
constexpr std::uint8_t kFullChannel = 255;

// L'INDEX D'IMAGE, ramené dans le jeu de trois quel que soit le sens.
[[nodiscard]] std::size_t WrapTextureIndex(std::int32_t index) {
  const std::int32_t size = static_cast<std::int32_t>(kScrollingBandTextures);
  return static_cast<std::size_t>(((index % size) + size) % size);
}

}  // namespace

EffectScrollingBandStyle EffectScrollingBandStyleFor(std::uint8_t style) {
  EffectScrollingBandStyle rendered;
  if (style == kStyleDeepWater) {
    rendered.color = {kDimmedChannel, kDimmedChannel, kFullChannel};
    rendered.blend_mode = kAddedBlend;
    rendered.reversed = true;
    return rendered;
  }
  if (style == kStyleFlare) {
    rendered.color = {kFullChannel, kDimmedChannel, kFullChannel};
    rendered.blend_mode = kAddedBlend;
    rendered.reversed = true;
    return rendered;
  }
  rendered.color = {kFullChannel, kFullChannel, kFullChannel};
  rendered.blend_mode = kOrdinaryBlend;
  rendered.reversed = false;
  return rendered;
}

std::vector<EffectWorldQuad> ComposeScrollingBandQuads(const EffectTrack& track,
                                                       glm::vec3 node_origin,
                                                       const EffectOwnerTint& owner) {
  std::vector<EffectWorldQuad> quads;
  if (!track.active || track.alpha <= 0) {
    return quads;
  }

  // ⚠ LA DURÉE DE CYCLE SE LIT EN FLOTTANT PUIS SE TRONQUE. Elle vit dans la
  // case que l'anneau prend pour une clé de décalage : le sens d'un champ
  // appartient à sa figure (ADR 0017).
  std::int32_t cycle_ticks = static_cast<std::int32_t>(track.lift_key);
  std::int32_t cycles_parcourus = 0;
  float phase = 0.0F;
  if (cycle_ticks != 0) {
    const std::int32_t tour = static_cast<std::int32_t>(kScrollingBandTextures) * cycle_ticks;
    cycles_parcourus = track.ticks % tour;
    phase = static_cast<float>(track.ticks % cycle_ticks) * kScrollingBandPhaseSpan /
            static_cast<float>(cycle_ticks);
  } else {
    // La bande se fige au bout de sa course, sur un seul quad.
    phase = kScrollingBandPhaseSpan;
    cycle_ticks = 1;
  }

  const EffectScrollingBandStyle style = EffectScrollingBandStyleFor(track.mark[kMarkOfBandStyle]);
  if (style.reversed) {
    phase = kScrollingBandPhaseSpan - phase;
  }
  const float fraction = phase * kScrollingBandPhaseToFraction;
  const std::int32_t notch = cycles_parcourus / cycle_ticks;

  const bool bascule = track.mark[kMarkOfBandPlane] == kScrollingBandPlaneTurned;
  const std::uint8_t tassement = track.mark[kMarkOfBandSqueeze];
  const float half_width = track.radius_at_anchor * 0.5F;
  // ⚠ SEULE LA PROFONDEUR DE L'ANCRE ENTRE, et le client ne lit que celle-là :
  // ses deux bâtisseurs n'écrivent que cette case. Lire les trois poserait les
  // rideaux là où personne ne les a mis.
  const float plan = track.anchor.z;

  const std::size_t segments = cycle_ticks == 1 ? 1 : kScrollingBandSegments;
  for (std::size_t rang = 0; rang < segments; ++rang) {
    float top = phase - track.value[rang];
    float bottom = phase - track.value[rang + 1];
    if (tassement > kScrollingBandSqueezeFloor) {
      const float facteur =
          static_cast<float>(kScrollingBandSqueezeFloor) / static_cast<float>(tassement);
      top *= facteur;
      bottom *= facteur;
    }

    // Le rognage des deux bouts, géométrie et image du même pas.
    float v_bottom = 1.0F;
    float v_top = 0.0F;
    if (rang == 0) {
      top -= (top - bottom) * fraction;
      v_top = fraction;
    } else if (rang + 1 == kScrollingBandSegments) {
      bottom += (top - bottom) * (1.0F - fraction);
      v_bottom = fraction;
    }

    // ⚠ LES COINS SONT CEUX DU CLIENT, dans son repère qui descend : la bande y
    // pend de cent soixante unités AU-DESSUS de son ancre jusqu'au sol. Ils
    // passent au nôtre avant d'être demandés ; recopiés, ils l'enterraient.
    EffectParticleQuadRequest request;
    if (bascule) {
      request.corners = {glm::vec3{plan, bottom, -half_width}, glm::vec3{plan, bottom, half_width},
                         glm::vec3{plan, top, half_width}, glm::vec3{plan, top, -half_width}};
    } else {
      request.corners = {glm::vec3{-half_width, bottom, plan}, glm::vec3{half_width, bottom, plan},
                         glm::vec3{half_width, top, plan}, glm::vec3{-half_width, top, plan}};
    }
    for (glm::vec3& corner : request.corners) {
      corner = EffectPointFromClient(corner);
    }
    request.alpha = static_cast<std::uint8_t>(track.alpha);
    request.color = style.color;
    request.blend_mode = style.blend_mode;
    request.v_at_a_and_b = v_bottom;
    request.v_at_c_and_d = v_top;
    request.u_at_a_and_d = 0.0F;
    request.u_at_b_and_c = 1.0F;
    // ⚠ L'INDEX TOURNE DANS LE SENS DE LA BANDE : le cran se retranche quand la
    // phase remonte et s'ajoute sinon. L'ajouter toujours ferait remonter la
    // bande sur des images qui, elles, descendraient.
    const std::int32_t signed_rank = static_cast<std::int32_t>(rang % kScrollingBandTextures);
    request.texture = WrapTextureIndex(style.reversed ? signed_rank - notch : signed_rank + notch);

    quads.push_back(ComposeEffectParticleQuad(request, node_origin, owner));
  }
  return quads;
}

}  // namespace ro::render
