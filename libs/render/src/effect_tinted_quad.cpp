#include "ro/render/effect_tinted_quad.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "ro/render/effect_particle_quad.h"
#include "ro/render/effect_quad_rotation.h"
#include "ro/render/effect_track.h"

namespace ro::render {
namespace {

// COMMENT LE CARRÉ TEINTÉ LIT SA PISTE. Les cases n'ont pas de sens en
// elles-mêmes — voir `effect_track.h` — et c'est ici que cette figure donne le
// sien aux siennes. Ce sont les mêmes que celles de l'arc de ruban, à une près :
// la case 9 y remplace la case 10, et elle ne choisit pas une forme mais un
// décalage.
constexpr std::size_t kSlotOfSpinDegrees = 8;
constexpr std::size_t kSlotOfNudgeKey = 9;
constexpr std::size_t kMarkOfTexture = 0;

// DE COMBIEN LE RAYON RESPIRE, au rythme de l'angle propre de la piste. Le même
// chiffre que l'arc, et c'est ce qui dit que les deux figures sont sœurs.
constexpr float kRadiusBreathAmplitude = 0.050000001F;

// LE CARRÉ : quatre coins, chacun un quart de tour après le précédent.
constexpr std::size_t kCornerCount = 4;
constexpr std::int32_t kQuarterTurnDegrees = 90;

// LA CLÉ QUI POUSSE LE CARRÉ DE CÔTÉ, et de combien. Une seule valeur déclenche
// ce décalage ; toute autre le laisse au centre.
constexpr float kNudgeKeyThatShifts = 10.0F;
constexpr float kNudgeAside = 4.0F;

// DE COMBIEN LA SEULE MARQUE QUI S'ÉLÈVE monte au-dessus de son ancre. Le
// client l'écrit en retranchant ce nombre à l'ordonnée de ses quatre coins, et
// son repère DESCEND : la soustraction élève.
constexpr float kLiftOfRaisedMark = 110.0F;

// LE SEIZIÈME PARAMÈTRE DU TRACEUR, celui que chaque marque choisit. Le traceur
// le compare à zéro et prend deux — le facteur UN de Direct3D, une ADDITION —
// quand il est nul, six — le mélange ordinaire — sinon.
//
// ⚠ LE MÊME POUR TOUTES LES MARQUES serait faux dans les deux sens. `pok1.tga`
// est GRISE à cent soixante-dix par canal : mélangée au lieu d'ajoutée sous la
// marque onze du soin, elle rend un gris à halo blanc là où le client fait
// briller du vert ; ajoutée sous la marque seize, qui peint du NOIR, elle
// disparaîtrait.
constexpr std::int16_t kAddedBlend = 0;
constexpr std::int16_t kOrdinaryBlend = 1;

// L'image entière. ⚠ LE BAS DE L'IMAGE VA AUX DEUX PREMIERS COINS : le client
// pose v = 1 sur A et B et v = 0 sur C et D. Les inverser retournait toutes
// les images de cette figure — les pommes, les sacs, les notes des chansons.
constexpr float kTextureAtLeft = 0.0F;
constexpr float kTextureAtRight = 1.0F;
constexpr float kTextureAtFirstCorners = 1.0F;
constexpr float kTextureAtLastCorners = 0.0F;

// LA MARQUE QUI DÉLÈGUE À UNE SOUS-MARQUE. Elle seule lit `sub_mark`, et ses
// six branches sont six teintes de plus.
constexpr std::uint8_t kMarkThatDelegates = 9;

[[nodiscard]] float RadiansOfDegrees(std::int32_t degrees) {
  return static_cast<float>(degrees) * std::numbers::pi_v<float> / 180.0F;
}

[[nodiscard]] std::int32_t WithinFullTurn(std::int32_t degrees) {
  const std::int32_t remainder = degrees % kEffectTrackFullTurnDegrees;
  return remainder < 0 ? remainder + kEffectTrackFullTurnDegrees : remainder;
}

// Ce que la sous-marque de la marque neuf demande. Les six branches ne diffèrent
// que par leur teinte et, pour la dernière, par un mélange ADDITIF.
[[nodiscard]] EffectTintedQuadStyle StyleOfDelegatedMark(std::uint8_t sub_mark) {
  switch (sub_mark) {
    case 1:
      return {{255, 135, 135}, true, true, false, 0.0F, true};
    case 2:
      return {{255, 205, 135}, true, true, false, 0.0F, true};
    case 3:
      return {{255, 255, 135}, true, true, false, 0.0F, true};
    case 4:
      return {{255, 255, 255}, true, true, false, 0.0F, true};
    case 5:
      return {{135, 135, 255}, true, true, false, 0.0F, true};
    case 6:
      return {{255, 255, 255}, true, false, false, 0.0F, true};
    default:
      // ⚠ ZÉRO N'EST PAS UNE SOUS-MARQUE. Le client n'a pas de branche pour
      // elle, et un nœud dont personne n'a posé la sous-marque ne dessine donc
      // rien du tout — ce que sa marque neuf, seule, ne laisserait pas deviner.
      return {};
  }
}

}  // namespace

EffectTintedQuadStyle EffectTintedQuadStyleFor(std::uint8_t mark, std::uint8_t sub_mark) {
  switch (mark) {
    // ⚠ TROIS MARQUES GARDENT L'ORDRE DES COINS, et ce sont les trois seules. La
    // texture s'y pose donc à l'endroit quand toutes les autres la retournent.
    case 0:
      return {{255, 255, 255}, false, true, false, 0.0F, true};
    case 15:
      return {{255, 200, 255}, false, true, false, 0.0F, true};
    // ⚠ LA MARQUE UN AUSSI, bien que ses voisines deux et trois échangent. Son
    // appel empile les coins dans l'ordre des marques zéro et quinze — le
    // quatrième, le troisième, le deuxième puis le premier — là où la marque
    // deux commence par le DEUXIÈME coin.
    case 1:
      return {{150, 150, 250}, false, false, false, 0.0F, true};
    case 2:
      return {{130, 130, 250}, true, false, false, 0.0F, true};
    case 3:
      return {{200, 200, 100}, true, false, false, 0.0F, true};
    case 4:
      return {{255, 255, 255}, true, true, false, 0.0F, true};
    case 7:
    case 23:
      return {{255, 255, 255}, true, false, false, 0.0F, true};
    case 8:
      return {{135, 255, 135}, true, true, false, 0.0F, true};
    case kMarkThatDelegates:
      return StyleOfDelegatedMark(sub_mark);
    case 10:
      return {{250, 250, 150}, true, false, false, 0.0F, true};
    case 11:
      return {{220, 250, 220}, true, false, false, 0.0F, true};
    case 12:
      return {{252, 171, 143}, true, true, false, 0.0F, true};
    case 13:
      return {{250, 100, 100}, true, false, false, 0.0F, true};
    case 14:
      return {{94, 0, 0}, true, true, false, 0.0F, true};
    case 16:
      // ⚠ NOIR N'EST PAS ÉTEINT. Cette marque peint vraiment du noir, et sur un
      // mélange ordinaire cela se voit — la prendre pour une absence de teinte
      // rendrait son carré blanc.
      return {{0, 0, 0}, true, true, false, 0.0F, true};
    case 17:
      return {{255, 180, 180}, true, true, false, 0.0F, true};
    case 18:
      // La seule qui n'emploie que la moitié de l'alpha de sa piste.
      return {{255, 255, 255}, true, false, true, 0.0F, true};
    case 24:
      return {{63, 72, 204}, true, false, false, 0.0F, true};
    case 25:
      // La seule qui s'élève, de cent dix unités au-dessus de son ancre.
      return {{255, 255, 255}, true, false, false, kLiftOfRaisedMark, true};
    default:
      // SIX MARQUES SUR VINGT-SIX NE DESSINENT RIEN, et c'est une réponse : le
      // `switch` du client retombe sur un `default` muet.
      return {};
  }
}

std::optional<EffectWorldQuad> ComposeTintedRotatedQuad(const EffectTrack& track, std::uint8_t mark,
                                                        std::uint8_t sub_mark,
                                                        std::int16_t billboard_turn_degrees,
                                                        std::int16_t billboard_tilt_degrees) {
  if (!track.active || track.alpha <= 0) {
    return std::nullopt;
  }
  const EffectTintedQuadStyle style = EffectTintedQuadStyleFor(mark, sub_mark);
  if (!style.draws) {
    return std::nullopt;
  }

  // ⚠ UN ANGLE PROPRE HORS DU TOUR VAUT ZÉRO, il ne se ramène pas : le client
  // remet la case à zéro plutôt que d'en prendre le reste. Une piste qui a
  // débordé cesse de respirer au lieu de repartir au milieu du cycle.
  const float own = track.value[kSlotOfSpinDegrees];
  const bool respire = own >= 0.0F && own < static_cast<float>(kEffectTrackFullTurnDegrees);
  const float radius = respire
                           ? track.radius_at_anchor +
                                 track.radius_at_anchor *
                                     std::sin(RadiansOfDegrees(static_cast<std::int32_t>(own))) *
                                     kRadiusBreathAmplitude
                           : track.radius_at_anchor;

  const bool nudged = track.value[kSlotOfNudgeKey] == kNudgeKeyThatShifts;
  std::array<glm::vec3, kCornerCount> corners;
  std::int32_t angle = WithinFullTurn(track.degrees);
  for (std::size_t corner = 0; corner < kCornerCount; ++corner) {
    if (corner > 0) {
      angle = WithinFullTurn(angle + kQuarterTurnDegrees);
    }
    const float theta = RadiansOfDegrees(angle);
    // L'ordonnée porte l'élévation dans le repère DESCENDANT du client ; elle se
    // retourne plus bas, avec les trois autres.
    corners[corner] = {radius * std::cos(theta), -style.lift_above_anchor,
                       radius * std::sin(theta)};
    if (nudged) {
      corners[corner].x += kNudgeAside;
    }
  }
  RotateEffectQuadCorners(corners, billboard_turn_degrees, billboard_tilt_degrees);

  const std::array<std::size_t, kCornerCount> order =
      style.swaps_corners ? std::array<std::size_t, kCornerCount>{1, 0, 3, 2}
                          : std::array<std::size_t, kCornerCount>{0, 1, 2, 3};

  EffectParticleQuadRequest request;
  for (std::size_t corner = 0; corner < kCornerCount; ++corner) {
    const glm::vec3 local = corners[order[corner]];
    // ⚠ LE REPÈRE SE RETOURNE ICI, et seulement ici, par `S` entier : les coins
    // se calculent dans celui du client, tournés par les angles de SA caméra —
    // élévation comprise —, et l'ancre est déjà dans le nôtre.
    request.corners[corner] = track.anchor + EffectPointFromClient(local);
  }
  // Le dix-huitième paramètre, à UN pour toutes les marques : l'origine du nœud
  // n'entre pas, l'ancre de piste est déjà dans le monde.
  request.absolute = true;
  const std::int16_t alpha =
      style.halves_alpha ? static_cast<std::int16_t>(track.alpha / 2) : track.alpha;
  request.alpha = static_cast<std::uint8_t>(std::clamp<std::int16_t>(alpha, 0, 255));
  request.color = style.color;
  request.blend_mode = style.ordinary_blend ? kOrdinaryBlend : kAddedBlend;
  request.texture = track.mark[kMarkOfTexture];
  request.u_at_a_and_d = kTextureAtLeft;
  request.u_at_b_and_c = kTextureAtRight;
  request.v_at_a_and_b = kTextureAtFirstCorners;
  request.v_at_c_and_d = kTextureAtLastCorners;
  return ComposeEffectParticleQuad(request, glm::vec3{0.0F, 0.0F, 0.0F});
}

bool DrawnAsTintedQuad(PooledNodeShape shape) noexcept {
  switch (shape) {
    case PooledNodeShape::kScatterField:
    case PooledNodeShape::kHomingTrail:
    case PooledNodeShape::kThunderScatter:
    case PooledNodeShape::kTintedQuads:
    case PooledNodeShape::kRisingQuads:
    case PooledNodeShape::kThrownItem:
    case PooledNodeShape::kHealSparkle:
    case PooledNodeShape::kRisingSparks:
    case PooledNodeShape::kFallingItem:
    case PooledNodeShape::kSwirlTrail:
    case PooledNodeShape::kDriftingIcons:
      return true;
    default:
      return false;
  }
}

void CapTintedQuadAlpha(std::array<EffectTrack, kEffectTracksPerNode>& tracks) noexcept {
  for (EffectTrack& track : tracks) {
    if (track.active && track.alpha > kTintedQuadAlphaCapOnThatMap) {
      track.alpha = kTintedQuadAlphaCapOnThatMap;
    }
  }
}

}  // namespace ro::render
