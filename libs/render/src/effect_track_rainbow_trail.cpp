#include <array>

#include "ro/render/effect_track.h"
#include "ro/render/effect_track_motion.h"

#include "effect_track_angles.h"

namespace ro::render {
namespace {

// LES DEUX ÂGES QUI DÉCOUPENT LA VIE DE L'ARC : il monte jusqu'au premier, il
// descend jusqu'au second, et ne fait plus rien après.
constexpr std::int32_t kLastTickOfRise = 180;
constexpr std::int32_t kLastTickOfFall = 360;

// LE PLAFOND D'ALPHA de cette famille, et il n'est jamais atteint : l'alpha monte
// d'un par tic pendant cent quatre-vingts tics. L'écrire quand même est ce qui
// permettra de s'en apercevoir le jour où un bâtisseur sèmerait un alpha plein.
constexpr std::int16_t kAlphaCeiling = 250;

}  // namespace

void AdvanceCameraAlignedArcTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks) {
  for (EffectTrack& track : tracks) {
    if (!track.active) {
      continue;
    }
    ++track.ticks;

    // ⚠ LE BALAYAGE ET LA MONTÉE D'ALPHA SONT LE MÊME GESTE, et c'est ce qui
    // fait l'arc : tant que l'alpha bouge, l'azimut avance d'un degré. Passé le
    // trois cent soixantième tic, les DEUX s'arrêtent, et l'arc reste où il est.
    if (track.ticks <= kLastTickOfRise) {
      ++track.alpha;
      ++track.azimuth_degrees;
    } else if (track.ticks <= kLastTickOfFall) {
      --track.alpha;
      ++track.azimuth_degrees;
    }

    if (track.alpha < 0) {
      track.alpha = 0;
    } else if (track.alpha >= kAlphaCeiling) {
      track.alpha = kAlphaCeiling;
    }

    // ⚠ L'INCLINAISON NE TOURNE PAS, ELLE SE REPLIE. Le client la borne sans
    // jamais l'avancer : c'est l'angle de caméra que le bâtisseur a figé à la
    // naissance, et l'arc garde donc son plan quoi que le joueur fasse ensuite.
    DropOneTurn(track.tilt_degrees);
  }
}

}  // namespace ro::render
