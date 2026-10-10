#include "ro/render/effect_fading_sprite.h"

#include <algorithm>

#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_track.h"
#include "ro/render/effect_trail_history.h"

namespace ro::render {
namespace {

// LA MONTÉE D'ALPHA de ce style, à ce tic : le pas et le plafond.
//
// ⚠ UN NŒUD RESEMÉ A SA PROPRE RAMPE, et elle passe avant celle de son style :
// le client teste d'abord la marque du tic de naissance. C'est ce qui pâlit
// chaque battement d'ailes un peu plus que le précédent.
struct RiseSchedule {
  float step = 0.0F;
  float ceiling = 0.0F;
};

[[nodiscard]] RiseSchedule RiseScheduleOf(std::uint8_t style, std::uint8_t birth,
                                          std::int32_t ticks) {
  if (birth != 0) {
    return {kFadingSpriteRebornStep, kFadingSpriteRebornCeiling - static_cast<float>(birth)};
  }
  switch (style) {
    case kFadingSpriteWings:
      return {kFadingSpriteRiseStep, kFadingSpriteWingsCeiling};
    case kFadingSpriteHanbokBody:
      return {kFadingSpriteHanbokRiseStep, kFadingSpriteHanbokCeiling};
    case kFadingSpriteHanbokWings:
      // ⚠ CELUI-CI NE MONTE PAS, IL S'ÉTEINT : le client remet son alpha à zéro
      // à chaque tic. Ce sont les ailes du hanbok, que seule la traînée montre.
      return {0.0F, 0.0F};
    case kFadingSpriteBody:
      return {kFadingSpriteRiseStep, kFadingSpriteBodyCeiling};
    default:
      // Les plumes : elles montent moins haut, et cessent de monter au
      // trentième tic même si leur plafond n'est pas atteint.
      return ticks < kFadingSpriteFeatherRiseLastTick
                 ? RiseSchedule{kFadingSpriteRiseStep, kFadingSpriteFeatherCeiling}
                 : RiseSchedule{0.0F, 0.0F};
  }
}

// À PARTIR DE QUEL TIC ce style s'efface, et de combien par tic.
struct FadeSchedule {
  std::int32_t first_tick = 0;
  float step = 0.0F;
};

[[nodiscard]] FadeSchedule FadeScheduleOf(std::uint8_t style, std::uint8_t birth) {
  switch (style) {
    case kFadingSpriteBody:
      return {kFadingSpriteBodyFallTick, kFadingSpriteBodyFallStep};
    case kFadingSpriteFirstFeather:
      return {kFadingSpriteFirstFeatherFallTick, kFadingSpriteSlowFallStep};
    case kFadingSpriteSecondFeather:
      return {kFadingSpriteSecondFeatherFallTick, kFadingSpriteSlowFallStep};
    case kFadingSpriteThirdFeather:
      return {kFadingSpriteThirdFeatherFallTick, kFadingSpriteSlowFallStep};
    case kFadingSpriteHanbokBody:
      return {kFadingSpriteBodyFallTick, kFadingSpriteFastFallStep};
    default:
      // ⚠ ET LE RESEMÉ S'EFFACE PLUS DOUCEMENT : le client regarde encore la
      // marque du tic de naissance pour choisir entre trois et six.
      return {kFadingSpriteOtherFallTick,
              birth != 0 ? kFadingSpriteSlowFallStep : kFadingSpriteFastFallStep};
  }
}

// DE COMBIEN DE TICS l'index d'image avance d'un cran, et où il s'arrête.
[[nodiscard]] std::int32_t FramePeriodTicks(std::uint8_t style) {
  return style == kFadingSpriteHanbokBody || style == kFadingSpriteHanbokWings
             ? kFadingSpriteHanbokFrameEveryTicks
             : kFadingSpriteFrameEveryTicks;
}

[[nodiscard]] std::int32_t LastFrame(std::uint8_t style) {
  return style == kFadingSpriteHanbokBody || style == kFadingSpriteHanbokWings
             ? kFadingSpriteHanbokLastFrame
             : kFadingSpriteLastFrame;
}

}  // namespace

void AdvanceFadingSpriteNode(PooledEffectNode& pooled) {
  // ⚠ UNE SEULE PISTE SERT ICI, et le client ne regarde que celle-là : elle
  // porte l'ancre, le style et le tic de naissance. Les trois autres restent
  // éteintes.
  EffectTrack& track = pooled.tracks.front();
  if (track.active) {
    ++track.ticks;
    // ⚠ IL S'ÉLÈVE EN RETRANCHANT : le repère du client descend.
    track.anchor.y -= kFadingSpriteRisePerTick;
    // LE NŒUD SUIT SON ANCRE, et c'est ce qui le fait monter à l'écran.
    pooled.ground_position = track.anchor;

    const std::uint8_t style = track.mark[kFadingSpriteMarkOfStyle];
    const std::uint8_t birth = track.mark[kFadingSpriteMarkOfBirthTick];
    const RiseSchedule rise = RiseScheduleOf(style, birth, track.ticks);
    if (style == kFadingSpriteHanbokWings && birth == 0) {
      pooled.node.alpha = 0.0F;
    } else if (rise.step > 0.0F && pooled.node.alpha < rise.ceiling) {
      // ⚠ LE CLIENT TESTE AVANT D'AJOUTER, ET NE BORNE PAS. L'alpha dépasse donc
      // son plafond du dernier pas, et c'est ce qui empêche le CORPS de
      // s'éteindre : passé le centième tic, sa montée de cinq et sa chute de
      // quatre se répondent, et il oscille autour de son plafond jusqu'à ce que
      // le nœud meure. Borner à `plafond` ferait retomber l'ange à zéro — et il
      // ne le fait pas.
      pooled.node.alpha += rise.step;
    }

    // L'IMAGE N'AVANCE QU'APRÈS LE TRENTIÈME TIC, et le client la borne.
    if (track.ticks > kFadingSpriteFrameFirstTick) {
      if (track.ticks % FramePeriodTicks(style) == 0) {
        ++pooled.node.frame;
      }
      pooled.node.frame = std::min(pooled.node.frame, LastFrame(style));
    }

    const FadeSchedule descent = FadeScheduleOf(style, birth);
    if (track.ticks > descent.first_tick) {
      pooled.node.alpha = std::max(pooled.node.alpha - descent.step, 0.0F);
    }
  }

  // L'HISTORIQUE RECULE D'UN CRAN, que la piste vive ou non : le client le fait
  // reculer en sortant, hors de toute garde.
  AdvanceEffectTrailHistory(pooled.trail, pooled.node, pooled.ground_position);
}

}  // namespace ro::render
