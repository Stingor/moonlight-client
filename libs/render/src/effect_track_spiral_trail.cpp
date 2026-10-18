#include <array>
#include <cstdint>

#include "ro/render/effect_node.h"
#include "ro/render/effect_spiral_trail.h"
#include "ro/render/effect_track.h"
#include "ro/render/effect_track_motion.h"

#include "effect_track_angles.h"

namespace ro::render {
namespace {

// TOUS LES COMBIEN LA TRACE SE DÉCALE. Trois tics : la trace est donc à trois
// tics derrière le grain, et le tampon en garde deux de plus.
constexpr std::int32_t kTrailShiftPeriodTicks = 3;

// LA FENÊTRE OÙ L'ÉTINCELLE PEUT S'ALLUMER, comptée depuis la marque de retard
// de la piste, et le rythme qui l'allume dans cette fenêtre.
constexpr std::int32_t kSparkWindowOpensAfter = 160;
constexpr std::int32_t kSparkWindowClosesAfter = 180;
constexpr std::int32_t kSparkBeatTicks = 20;
constexpr std::int32_t kSparkFirstImageAt = 15;
constexpr std::int32_t kSparkFirstImageAgainAt = 19;
constexpr std::int32_t kSparkSecondImageAt = 17;

// LA MARQUE DE RETARD, qui décale la fenêtre d'étincelle d'une piste à l'autre.
constexpr std::size_t kMarkOfSparkDelay = 2;
// LA MARQUE D'ÉTAT : zéro tant que le grain monte invisible, un dès qu'il file.
constexpr std::size_t kMarkOfFlightState = 1;
constexpr std::uint8_t kStateClimbing = 0;
constexpr std::uint8_t kStateFlying = 1;

// DE COMBIEN L'AZIMUT TOURNE par tic : c'est le carré qui tourne sur lui-même.
constexpr std::int16_t kSpinPerTick = 5;

// LA MONTÉE : de combien l'ordonnée s'élève par tic. Le tic où elle cesse est
// PUBLIC — c'est lui qui dit combien de temps la gerbe reste invisible, donc
// quelle vie de nœud lui suffit. Voir effect_spiral_trail.h.
constexpr float kClimbPerTick = 0.25F;
constexpr std::int16_t kAlphaAtTakeOff = 250;

// LE VOL : le rayon rétrécit d'un cinquantième par tic, et l'ordonnée retombe
// de deux centièmes par tic — la pesanteur qui courbe la gerbe.
constexpr float kRadiusShrink = 0.98F;
constexpr float kSinkPerTick = 0.02F;

// LE TIC OÙ LA PISTE RENAÎT, et ce qu'elle y retire au sort.
constexpr std::int32_t kTickOfRebirth = 2000;
constexpr float kRebirthShiftX = -15.0F;
constexpr float kRebirthRadiusFloor = 0.4F;
constexpr float kRebirthRadiusStep = 0.004F;
constexpr std::uint32_t kRebirthRadiusSpread = 100;

// LA MOITIÉ DES TICS COÛTENT UN ALPHA DE PLUS. Le client tire à pile ou face.
constexpr std::uint32_t kHeadsOrTailsModulus = 2;

void DecalerLaTrace(EffectTrack& track) {
  // ⚠ DANS CET ORDRE, ET IL COMPTE : la plus vieille case reçoit la deuxième, la
  // deuxième reçoit la première, la première reçoit l'ancre. L'inverse écraserait
  // tout avec l'ancre du tic courant.
  for (std::size_t axe = 0; axe < 3; ++axe) {
    track.tail[6 + axe] = track.tail[axe];
    track.tail[axe] = track.tail[kTailOfSpiralTrailRecent + axe];
  }
  track.tail[kTailOfSpiralTrailRecent] = track.anchor.x;
  track.tail[kTailOfSpiralTrailRecent + 1] = track.anchor.y;
  track.tail[kTailOfSpiralTrailRecent + 2] = track.anchor.z;
}

void PlaceSpark(EffectTrack& track) {
  const auto delay = static_cast<std::int32_t>(track.mark[kMarkOfSparkDelay]);
  if (track.ticks > delay + kSparkWindowClosesAfter ||
      track.ticks < delay + kSparkWindowOpensAfter) {
    track.mark[kMarkOfSpiralTrailSpark] = 0;
    return;
  }
  const std::int32_t battement = track.ticks % kSparkBeatTicks;
  if (battement == kSparkFirstImageAt || battement == kSparkFirstImageAgainAt) {
    track.mark[kMarkOfSpiralTrailSpark] = 1;
  } else {
    track.mark[kMarkOfSpiralTrailSpark] = battement == kSparkSecondImageAt ? 2 : 0;
  }
}

}  // namespace

void AdvanceSwirlOrbitTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                             std::int16_t billboard_turn_degrees,
                             std::int16_t billboard_tilt_degrees, glm::vec3 node_position,
                             std::uint32_t& generator) {
  // LES ANGLES DE PANNEAU, RANGÉS TELS QUELS : la troupe passe déjà le
  // complément du client (`EffectBillboardTurnDegrees`, `…TiltDegrees`) ; le
  // recalculer ici rendrait à la figure l'angle brut de la caméra.
  tracks[0].style = billboard_turn_degrees;
  tracks[1].style = billboard_tilt_degrees;

  for (EffectTrack& track : tracks) {
    if (!track.active) {
      continue;
    }
    ++track.ticks;
    // ⚠ LE COMPTEUR PART TRÈS NÉGATIF, et c'est tout l'échelonnement : le semis
    // le pose entre moins trois cents et moins trois cent cinquante, si bien que
    // les quatre grains ne partent pas ensemble et que la gerbe dure.
    if (track.ticks <= 0) {
      continue;
    }

    if (track.ticks % kTrailShiftPeriodTicks == 0) {
      DecalerLaTrace(track);
    }
    PlaceSpark(track);

    TurnOnward(track.azimuth_degrees, kSpinPerTick);

    if (track.mark[kMarkOfFlightState] == kStateClimbing) {
      // LA MONTÉE : rien d'autre que l'ordonnée. Le client la retranche dans son
      // repère qui descend ; elle s'ajoute dans le nôtre, qui monte.
      track.anchor.y += kClimbPerTick;
      if (track.ticks > kLastTickOfSpiralTrailClimb) {
        track.mark[kMarkOfFlightState] = kStateFlying;
        // ⚠ L'ALPHA NAÎT AU DÉCOLLAGE, pas au semis : le grain monte INVISIBLE
        // pendant cent cinquante-quatre tics, puis s'allume d'un coup. C'est la
        // mèche, et la poser au semis montrerait une braise qui s'élève.
        track.alpha = kAlphaAtTakeOff;
      }
    } else if (track.mark[kMarkOfFlightState] == kStateFlying) {
      // LE VOL : une direction FIXE, tirée au semis, et un rayon qui fond.
      const float step = track.radius_at_anchor;
      const float de_cote = step * CosOfDegrees(track.degrees);
      track.anchor.y -= step * SinOfDegrees(track.degrees);
      --track.alpha;
      track.anchor.x += de_cote * CosOfDegrees(track.tilt_degrees);
      // ⚠ L'ORDONNÉE SE CORRIGE UNE SECONDE FOIS, après le pas : deux centièmes
      // de pesanteur, qui courbent la gerbe vers le sol. Comme le pas, le client
      // les écrit dans son repère qui descend, et le signe s'inverse ici.
      track.anchor.y -= kSinkPerTick;
      // ⚠ LA DIRECTION TIRÉE EST UN CAP DU CLIENT, et l'ancre est dans notre
      // repère : la profondeur se nie. Le tirage couvre le tour entier, si bien
      // que le miroir ne se voit pas en moyenne ; à graine égale, si.
      track.anchor.z -= de_cote * SinOfDegrees(track.tilt_degrees);
      track.radius_at_anchor = step * kRadiusShrink;
      // ⚠ UN TIC SUR DEUX COÛTE UN ALPHA DE PLUS, et c'est un tirage, pas un
      // rythme : deux grains voisins ne s'éteignent pas ensemble.
      if (NextEffectNodeDraw(generator) % kHeadsOrTailsModulus == 0) {
        --track.alpha;
      }
      if (track.alpha < 0) {
        track.alpha = 0;
      }
    }

    if (track.ticks > kTickOfRebirth) {
      // ⚠ LA PISTE NE MEURT PAS, ELLE RENAÎT. Rien ne l'éteint jamais : c'est le
      // nœud qui finit par mourir de sa vie. Le compte revient à zéro, le grain
      // reprend sa montée et retire sa direction au sort.
      track.ticks = 0;
      track.mark[kMarkOfFlightState] = kStateClimbing;
      track.tilt_degrees = static_cast<std::int16_t>(
          NextEffectNodeDraw(generator) % static_cast<std::uint32_t>(kEffectTrackFullTurnDegrees));
      track.anchor = node_position;
      track.anchor.x += kRebirthShiftX;
      track.radius_at_anchor =
          kRebirthRadiusFloor +
          static_cast<float>(NextEffectNodeDraw(generator) % kRebirthRadiusSpread) *
              kRebirthRadiusStep;
    }
  }
}

}  // namespace ro::render
