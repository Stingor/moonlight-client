#include <algorithm>

#include "ro/render/effect_track.h"

namespace ro::render {
namespace {

// CE QUE LE MOUVEMENT DU BILLBOARD À STYLES POSE EN DUR, et comment il lit les
// cases de sa piste. Un fichier par mouvement : voir
// `effect_track_particle.cpp` pour la raison.

// La case qui porte l'angle dont la figure tourne SUR ELLE-MÊME, et la marque
// qui porte le STYLE — d'où tout le reste dépend.
constexpr std::size_t kSlotOfSpinDegrees = 8;
constexpr std::size_t kMarkOfStyle = 4;

// Passé ce tic, la courbe d'alpha du style ne joue plus et seul le déclin final
// reste. Le client saute alors le choix entier.
constexpr std::int32_t kTicksWhenStyleStopsDriving = 300;
// Pour les deux styles à courbe, le tic où la montée devient descente.
constexpr std::int32_t kTicksWhenRiseTurnsToFall = 200;

// LES PAS ET LES PLAFONDS D'ALPHA, par famille de styles.
constexpr std::int16_t kAlphaRisePerTick = 10;
constexpr std::int16_t kAlphaCeilingForRisingStyles = 220;
constexpr std::int16_t kAlphaFallPerTick = 5;
constexpr std::int16_t kAlphaRiseForStyleTwenty = 5;
constexpr std::int16_t kAlphaCeilingForStyleTwenty = 200;
constexpr std::int16_t kAlphaCreepPerTick = 1;

// LES ALPHAS QUE LES AUTRES STYLES POSENT D'EMBLÉE, sans montée. Le client les
// ÉCRIT et ne les ajoute pas : les cumuler blanchirait la figure en quelques
// tics.
constexpr std::int16_t kAlphaForStandingStyles = 100;
constexpr std::int16_t kAlphaForTurningStyles = 150;
constexpr std::int16_t kAlphaForStyleFourteen = 50;
constexpr std::int16_t kAlphaForLateStyles = 250;
// Sous ce style, aucun alpha n'est posé d'emblée — et c'est aussi la famille
// que le déclin final éteint.
constexpr std::uint8_t kFirstStyleWithStandingAlpha = 10;

// Le style qui PULSE : après un premier temps, son alpha monte puis redescend
// par pas de huit, et son rayon croît tant qu'il reste visible.
constexpr std::uint8_t kStyleThatPulses = 2;
constexpr std::int32_t kTicksBeforeThePulse = 40;
constexpr std::int32_t kTicksWhenThePulseFalls = 62;
constexpr std::int16_t kPulseStepPerTick = 8;
constexpr float kRadiusGrowthWhilePulsing = 0.69999999F;

// Le premier style qui ne tourne plus sur lui-même, et le pas de cette rotation.
constexpr std::uint8_t kFirstStyleWithoutOwnSpin = 30;
constexpr float kSpinStepPerTick = 2.0F;

// De combien l'angle avance ou recule — un tic sur deux, et c'est ce qui donne
// à la figure sa rotation lente.
constexpr std::int16_t kAngleStepPerTick = 1;
constexpr std::int32_t kTicksBetweenAngleSteps = 2;

constexpr std::int16_t kAlphaMax = 255;

// CE QUE LE STYLE FAIT DE L'ANGLE. Trois familles, et le reste n'y touche pas.
//
// ⚠ LE STYLE DIX-HUIT NE TOURNE PAS MAIS SE REPLIE quand même : le client le
// fait sauter par-dessus le pas, droit au repli. Le ranger avec ceux qui ne
// touchent à rien laisserait passer un angle négatif hérité du bâtisseur.
[[nodiscard]] bool StyleTurnsForward(std::uint8_t style) {
  return style == 10 || style == 13 || style == 14 || style == 15 || style == 19;
}

[[nodiscard]] bool StyleTurnsBackward(std::uint8_t style) {
  return style == 11 || style == 12 || style == 16 || style == 17;
}

constexpr std::uint8_t kStyleThatOnlyFolds = 18;

// L'alpha que le style pose d'emblée, ou rien quand il n'en pose aucun.
//
// ⚠ UN STYLE SOUS DIX NE POSE RIEN, ET CE N'EST PAS ZÉRO : le client saute, et
// l'alpha garde ce que le tic d'avant lui a laissé.
[[nodiscard]] bool StandingAlphaOf(std::uint8_t style, std::int16_t& alpha) {
  if (style == 16 || style == 17 || style == 18) {
    alpha = kAlphaForStandingStyles;
    return true;
  }
  if (style == 10 || style == 11 || style == 12) {
    alpha = kAlphaForTurningStyles;
    return true;
  }
  if (style == 14) {
    alpha = kAlphaForStyleFourteen;
    return true;
  }
  if (style < kFirstStyleWithStandingAlpha) {
    return false;
  }
  alpha = kAlphaForLateStyles;
  return true;
}

// La courbe d'alpha que le style suit tant qu'il pilote.
void StepAlphaByStyle(EffectTrack& track, std::uint8_t style) {
  if (style == 0) {
    track.alpha = static_cast<std::int16_t>(track.alpha + kAlphaCreepPerTick);
    return;
  }
  if (style == 1 || style == 3) {
    if (track.ticks < kTicksWhenRiseTurnsToFall) {
      track.alpha = static_cast<std::int16_t>(track.alpha + kAlphaRisePerTick);
      track.alpha = std::min(track.alpha, kAlphaCeilingForRisingStyles);
      return;
    }
    track.alpha = static_cast<std::int16_t>(track.alpha - kAlphaFallPerTick);
    if (track.alpha < 0) {
      track.alpha = 0;
    }
    return;
  }
  if (style == 20) {
    track.alpha = static_cast<std::int16_t>(track.alpha + kAlphaRiseForStyleTwenty);
    track.alpha = std::min(track.alpha, kAlphaCeilingForStyleTwenty);
    return;
  }
  if (style == 22) {
    track.alpha = static_cast<std::int16_t>(track.alpha + kAlphaRisePerTick);
    track.alpha = std::min(track.alpha, kAlphaCeilingForRisingStyles);
    return;
  }
  std::int16_t pose = 0;
  if (StandingAlphaOf(style, pose)) {
    track.alpha = pose;
  }
}

// L'angle propre à la piste, avancé et replié selon le style.
void StepAngleByStyle(EffectTrack& track, std::uint8_t style) {
  const bool advance = StyleTurnsForward(style);
  const bool recule = StyleTurnsBackward(style);
  if (!advance && !recule && style != kStyleThatOnlyFolds) {
    return;
  }
  if (style != kStyleThatOnlyFolds && track.ticks % kTicksBetweenAngleSteps == 0) {
    const std::int16_t step = advance ? kAngleStepPerTick : -kAngleStepPerTick;
    track.degrees = static_cast<std::int16_t>(track.degrees + step);
  }
  // ⚠ CHAQUE SENS NE REPLIE QUE SON CÔTÉ, comme le client : celui qui avance ne
  // teste que le tour entier, celui qui recule que le négatif.
  if (advance) {
    if (track.degrees >= kEffectTrackFullTurnDegrees) {
      track.degrees = static_cast<std::int16_t>(track.degrees - kEffectTrackFullTurnDegrees);
    }
    return;
  }
  if (track.degrees < 0) {
    track.degrees = static_cast<std::int16_t>(track.degrees + kEffectTrackFullTurnDegrees);
  }
}

}  // namespace

void AdvanceStyledBillboardTrack(EffectTrack& track) {
  if (!track.active) {
    return;
  }
  ++track.ticks;
  const std::uint8_t style = track.mark[kMarkOfStyle];

  // ⚠ PASSÉ CE TIC, LE STYLE NE PILOTE PLUS. Le client saute le choix entier et
  // ne garde que le déclin final ; continuer à jouer la courbe ferait remonter
  // l'alpha d'une figure qui devrait s'éteindre.
  if (track.ticks < kTicksWhenStyleStopsDriving) {
    StepAlphaByStyle(track, style);
  }

  if (style == kStyleThatPulses && track.ticks > kTicksBeforeThePulse) {
    const std::int16_t step =
        track.ticks >= kTicksWhenThePulseFalls ? -kPulseStepPerTick : kPulseStepPerTick;
    track.alpha = static_cast<std::int16_t>(track.alpha + step);
    // LE RAYON NE CROÎT QUE TANT QUE LA FIGURE SE VOIT : une fois l'alpha tombé,
    // elle cesse de s'ouvrir au lieu de grandir dans le vide.
    if (track.alpha > 0) {
      track.radius_at_anchor += kRadiusGrowthWhilePulsing;
    }
  }
  StepAngleByStyle(track, style);

  // LE DÉCLIN FINAL, et il ne vaut que pour les styles bas. Le client envoie les
  // autres droit au bornage, si bien qu'une figure de style élevé ne s'éteint
  // pas d'elle-même — c'est la mort du nœud qui la retire.
  if (style < kFirstStyleWithStandingAlpha && track.ticks > kTicksWhenStyleStopsDriving &&
      track.alpha > 0) {
    track.alpha = static_cast<std::int16_t>(track.alpha - kAlphaCreepPerTick);
  }
  track.alpha = std::clamp<std::int16_t>(track.alpha, 0, kAlphaMax);

  // LA FIGURE TOURNE AUSSI SUR ELLE-MÊME, et c'est un autre angle que le
  // précédent : celui-ci avance à chaque tic, pour tous les styles sous trente.
  if (style < kFirstStyleWithoutOwnSpin) {
    track.value[kSlotOfSpinDegrees] += kSpinStepPerTick;
  }
  if (track.value[kSlotOfSpinDegrees] >= static_cast<float>(kEffectTrackFullTurnDegrees)) {
    track.value[kSlotOfSpinDegrees] -= static_cast<float>(kEffectTrackFullTurnDegrees);
  }
}

}  // namespace ro::render
