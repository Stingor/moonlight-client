#include <cstddef>

#include "ro/render/effect_track.h"

#include "effect_track_angles.h"

namespace ro::render {
namespace {

// CE QUE LE SOUS-TYPE 36 POSE EN DUR, et comment il lit sa piste.
//
// ⚠ CE MOUVEMENT VIT DANS SON PROPRE FICHIER, comme les autres : deux
// mouvements donnent des valeurs DIFFÉRENTES aux mêmes idées, et les laisser se
// partager une portée obligerait à inventer des noms pour les distinguer.

// De combien le rayon croît, et le seuil qui retourne le sens de l'alpha.
constexpr float kRadiusPerTick = 0.04F;
constexpr float kRadiusThatStartsTheFade = 2.5F;

// L'INCLINAISON DESCEND D'UN DEGRÉ PAR TIC et s'arrête à dix — et toucher ce
// plancher ÉTEINT la piste. Une seule garde, deux effets.
constexpr std::int16_t kTiltPerTick = 1;
constexpr std::int16_t kLowestTilt = 10;

// L'ALPHA EST UN CYCLE, et c'est le RAYON qui en tient l'horloge : il monte
// tant que le rayon est petit, descend ensuite, et sa chute à zéro fait
// renaître le rayon et l'inclinaison. Une centaine de tics en tout — neuf de
// montée, quatre-vingt-dix de descente. L'anneau respire.
constexpr std::int16_t kAlphaRisePerTick = 20;
constexpr std::int16_t kAlphaFallPerTick = 2;
constexpr float kRadiusAtRebirth = 2.14F;
constexpr std::int16_t kTiltAtRebirth = 74;

// L'ONDE. Le profil parcourt une DEMI-période de sinus sur les vingt et une
// cases — neuf degrés par case, zéro à cent quatre-vingts —, si bien qu'elle
// est nulle aux deux bouts du ruban et maximale au milieu. Un tour entier
// l'aurait fait battre deux fois.
constexpr std::int32_t kWaveDegreesPerSlot = 9;
constexpr float kWaveAmplitude = 0.3F;

// DE COMBIEN LA PHASE AVANCE D'UNE PISTE À L'AUTRE, et le rang à partir duquel
// elle change de règle. Les deux premières prennent quatre-vingt-dix degrés par
// rang ; les suivantes en prennent quarante-cinq puis DOUBLENT le tout, si bien
// que leur onde court deux fois plus vite.
constexpr std::int32_t kSlowPhasePerTrack = 90;
constexpr std::int32_t kFastPhasePerTrack = 45;
constexpr std::int32_t kPhaseThatDoubles = 90;

// La phase de cette piste-ci, repliée sur le tour.
[[nodiscard]] std::int32_t TrackPhase(std::int32_t ticks, std::size_t rang) {
  const auto fast = static_cast<std::int32_t>(rang) * kFastPhasePerTrack;
  // ⚠ LA GARDE PORTE SUR LE DÉCALAGE RAPIDE, et non sur le rang : c'est lui que
  // le client compare à quatre-vingt-dix. Les deux disent la même chose pour
  // quatre pistes, et le dire comme lui reste vrai s'il en vient d'autres.
  const std::int32_t cumul = fast >= kPhaseThatDoubles
                                 ? (ticks + fast) * 2
                                 : ticks + static_cast<std::int32_t>(rang) * kSlowPhasePerTrack;
  return cumul % kEffectTrackFullTurnDegrees;
}

}  // namespace

void ApplyRibbonWaveLengths(EffectTrack& track, std::size_t rank) {
  const std::int32_t phase = TrackPhase(track.ticks, rank);
  const float wave = SinOfDegrees(phase) * kWaveAmplitude;
  for (std::size_t creneau = 0; creneau < track.value.size(); ++creneau) {
    const auto le_long = static_cast<std::int32_t>(creneau) * kWaveDegreesPerSlot;
    track.value[creneau] = track.lift_key * (1.0F + SinOfDegrees(le_long) * wave);
  }
}

void AdvanceRibbonWaveTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks) {
  for (std::size_t rang = 0; rang < tracks.size(); ++rang) {
    EffectTrack& effect_track = tracks[rang];
    if (!effect_track.active) {
      continue;
    }
    ++effect_track.ticks;
    effect_track.radius_at_anchor += kRadiusPerTick;
    effect_track.tilt_degrees = static_cast<std::int16_t>(effect_track.tilt_degrees - kTiltPerTick);
    if (effect_track.tilt_degrees < kLowestTilt) {
      effect_track.tilt_degrees = kLowestTilt;
      effect_track.alpha = 0;
    }

    // ⚠ LA COMPARAISON PORTE SUR LE RAYON DÉJÀ AVANCÉ. Le juger avant
    // l'incrément décalerait tout le cycle d'un tic, et la respiration avec.
    if (effect_track.radius_at_anchor < kRadiusThatStartsTheFade) {
      effect_track.alpha = static_cast<std::int16_t>(effect_track.alpha + kAlphaRisePerTick);
    } else {
      effect_track.alpha = static_cast<std::int16_t>(effect_track.alpha - kAlphaFallPerTick);
      if (effect_track.alpha <= 0) {
        effect_track.alpha = 0;
        effect_track.radius_at_anchor = kRadiusAtRebirth;
        effect_track.tilt_degrees = kTiltAtRebirth;
      }
    }

    ApplyRibbonWaveLengths(effect_track, rang);
  }
}

// CE QUE LE SOUS-TYPE 50 POSE EN DUR, et qui n'est PAS celui du 36.
namespace {

// 🔴 L'ONDE DU 50 N'EST PAS CELLE DU 36, et c'est ce qui dessine sa figure :
// `v = clé · sin(9k)`, puis
// `v += clé · sin(9k) · 0,3 · sin(phase)` — soit `clé · sin(9k) · (1 + 0,3 ·
// sin(phase))`. Le sinus du long multiplie TOUT : le ruban s'effile à zéro aux
// deux bouts et respire au milieu, là où celui du 36 garde sa clé pour base et
// n'ondule qu'autour d'elle. Lui prêter la loi de son frère en ferait une bande
// de largeur constante.
void SpinningRibbonLengths(EffectTrack& track, std::size_t rank) {
  const std::int32_t phase = TrackPhase(track.ticks, rank);
  const float breath = 1.0F + SinOfDegrees(phase) * kWaveAmplitude;
  for (std::size_t creneau = 0; creneau < track.value.size(); ++creneau) {
    const auto le_long = static_cast<std::int32_t>(creneau) * kWaveDegreesPerSlot;
    track.value[creneau] = track.lift_key * SinOfDegrees(le_long) * breath;
  }
}

// LE RAYON PULSE au lieu de croître : il est le SINUS d'un compteur à part,
// décalé de trois. L'anneau va donc de DEUX à QUATRE et revient.
constexpr float kPulsingRadiusOffset = 3.0F;
// DE COMBIEN L'AZIMUT TOURNE par tic : un tour en cent vingt tics.
constexpr std::int16_t kPulsingAzimuthPerTick = 3;
// De combien le compteur du pouls avance. Un degré, et il reboucle sur le tour.
constexpr std::int16_t kPulsingStepPerTick = 1;

}  // namespace

void AdvanceTurningRingTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks) {
  for (std::size_t rang = 0; rang < tracks.size(); ++rang) {
    EffectTrack& effect_track = tracks[rang];
    if (!effect_track.active) {
      continue;
    }
    ++effect_track.ticks;

    // ⚠ LE POULS A SON PROPRE COMPTEUR, et ce n'est pas celui des tics : le
    // client le tient dans le champ de STYLE de la piste. Le semis l'y TIRE, si
    // bien que deux anneaux ne pulsent pas d'accord — prendre le compteur de
    // tics les ferait battre d'un seul cœur. C'est l'ADR 0017 : le champ n'a de
    // sens que pour qui le lit.
    TurnOnward(effect_track.style, kPulsingStepPerTick);
    effect_track.radius_at_anchor = SinOfDegrees(effect_track.style) + kPulsingRadiusOffset;

    TurnOnward(effect_track.azimuth_degrees, kPulsingAzimuthPerTick);

    // ⚠ ET IL NE TOUCHE NI À L'ALPHA NI À L'INCLINAISON, là où son frère fait
    // respirer l'un et descendre l'autre. Deux mouvements de la même figure, et
    // pas même la même onde : voir `LongueursDuRubanQuiTourne`.
    SpinningRibbonLengths(effect_track, rang);
  }
}

}  // namespace ro::render
