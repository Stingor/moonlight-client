#include "ro/render/effect_swirl_trail.h"

#include <algorithm>

#include "ro/render/effect_track.h"

#include "effect_track_angles.h"

namespace ro::render {
namespace {

// LES DEUX PISTES QUI PORTENT LES ANGLES DE CAMÉRA.
constexpr std::size_t kTrackThatTakesTheTurn = 0;
constexpr std::size_t kTrackThatTakesTheTilt = 1;

// LA MENEUSE, et la dernière piste de la traîne.
constexpr std::size_t kLeadTrack = 0;
constexpr std::size_t kFirstEchoTrack = 1;

// UN ÉCHO REPREND SA VOISINE : l'angle, le rayon, l'angle propre, l'alpha
// diminué, l'ancre et la marque d'image — la troisième piste d'abord, puis la
// deuxième, puis la première. Rien d'autre : ni l'allumage, ni le compteur, ni
// le cap, ni la vitesse — un écho ne bouge pas de lui-même.
void EchoFrom(const EffectTrack& voisine, EffectTrack& echo, std::int16_t perte_d_alpha) {
  echo.degrees = voisine.degrees;
  echo.radius_at_anchor = voisine.radius_at_anchor;
  echo.value[kSwirlTrailSlotOfSpinDegrees] = voisine.value[kSwirlTrailSlotOfSpinDegrees];
  echo.alpha = static_cast<std::int16_t>(std::max(voisine.alpha - perte_d_alpha, 0));
  echo.anchor = voisine.anchor;
  echo.mark[kSwirlTrailMarkOfTexture] = voisine.mark[kSwirlTrailMarkOfTexture];
}

}  // namespace

void AdvanceSwirlTrailTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                             std::int16_t camera_turn_degrees, std::int16_t camera_tilt_degrees) {
  tracks[kTrackThatTakesTheTurn].style = camera_turn_degrees;
  tracks[kTrackThatTakesTheTilt].style = camera_tilt_degrees;

  EffectTrack& leader = tracks[kLeadTrack];
  // ⚠ SEULE LA MENEUSE EST GARDÉE, par son allumage. Les échos n'ont pas de
  // garde : ils n'avancent pas, ils sont écrits.
  if (!leader.active) {
    return;
  }
  ++leader.ticks;
  if (leader.ticks <= 0) {
    return;
  }

  // LA TRAÎNE, tous les trois tics, EN PARTANT DU BOUT.
  //
  // ⚠ LES TROIS GARDES SONT `> 3`, `> 2` ET `> 1` sur un compteur multiple de
  // trois : au troisième tic, la dernière piste n'est pas encore écrite — elle
  // n'aurait rien à recevoir que du vide.
  if (leader.ticks % kSwirlTrailEchoPeriodTicks == 0) {
    for (std::size_t echo = tracks.size() - 1; echo >= kFirstEchoTrack; --echo) {
      if (leader.ticks <= static_cast<std::int32_t>(echo)) {
        continue;
      }
      EchoFrom(tracks[echo - 1], tracks[echo],
               echo == kFirstEchoTrack ? kSwirlTrailFirstEchoAlphaDrop : kSwirlTrailEchoAlphaDrop);
    }
  }

  // ⚠ LA ROTATION SE RATTRAPE D'UN SEUL TOUR, par une addition : quinze degrés
  // ne franchissent jamais plus d'un tour.
  leader.degrees = static_cast<std::int16_t>(leader.degrees - kSwirlTrailSpinPerTick);
  if (leader.degrees < 0) {
    leader.degrees = static_cast<std::int16_t>(leader.degrees + kEffectTrackFullTurnDegrees);
  }

  // ⚠ LA MONTÉE ET LA CHUTE NE SONT PAS UN `if … else` : deux gardes séparées,
  // qui ne se recouvrent pas ici parce que cinq tombe avant quinze.
  if (leader.ticks <= kSwirlTrailLastTickOfRise) {
    leader.alpha = std::min<std::int16_t>(
        static_cast<std::int16_t>(leader.alpha + kSwirlTrailAlphaRise), kEffectTrackAlphaMax);
  }
  if (leader.ticks >= kSwirlTrailFirstTickOfFall) {
    leader.alpha =
        std::max<std::int16_t>(static_cast<std::int16_t>(leader.alpha - kSwirlTrailAlphaFall), 0);
  }

  // LA MENEUSE FILE selon son cap, à plat : ni sa hauteur ni celle des échos ne
  // changent jamais.
  const float speed = leader.value[kSwirlTrailSlotOfSpeed];
  leader.anchor.x += speed * CosOfDegrees(leader.azimuth_degrees);
  leader.anchor.z += speed * SinOfDegrees(leader.azimuth_degrees);
}

}  // namespace ro::render
