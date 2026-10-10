#include <cstdint>

#include "ro/render/effect_grid_plane.h"
#include "ro/render/effect_track.h"
#include "ro/render/effect_track_motion.h"

namespace ro::render {
namespace {

// OÙ LA NAPPE SE REBOUCLE : un carreau. Passé là, son origine revient en deçà,
// si bien que la nappe ne s'éloigne jamais de plus d'un carreau.
constexpr float kGridPlaneWrapAt = 30.0F;

// LA MARQUE QUI MÈNE LE RYTHME, et c'est la MÊME que celle où la figure lit sa
// teinte (ADR 0017) : un seul octet, deux lecteurs, deux sens.
constexpr std::uint8_t kMarkThatPulses = 1;

// LES DEUX RYTHMES D'ALPHA. Celui qui pulse monte de dix pendant dix tics et
// retombe de dix passé soixante-cinq ; les autres montent de cinq et RESTENT.
constexpr std::int32_t kLastTickOfFastRise = 10;
constexpr std::int32_t kFirstTickOfFall = 65;
constexpr std::int16_t kFastRise = 10;
constexpr std::int16_t kSlowRise = 5;
constexpr std::int16_t kFall = 10;
// ⚠ LE PLAFOND EST UNE BORNE DU TEST, NON DE LA VALEUR. Le client monte tant que
// l'alpha ne DÉPASSE pas quarante-cinq : il finit donc à cinquante ou
// cinquante-cinq selon le pas, jamais à quarante-cinq. L'y ramener éteindrait la
// nappe d'un dixième.
constexpr std::int16_t kRiseWhileAtMost = 45;
constexpr std::int16_t kFallWhileAtLeast = 10;

}  // namespace

void AdvanceParticleFrameTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                                std::int16_t billboard_turn_degrees,
                                std::int16_t billboard_tilt_degrees) {
  // ⚠ CES DEUX-LÀ SE POSENT MÊME QUAND LE NŒUD DORT.
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
    // ⚠ LA CLÉ NE FAIT QUE SE REBOUCLER, elle n'avance pas. Rien ici ne la fait
    // croître : le client se contente de la ramener en deçà d'un carreau si elle
    // l'a dépassé. Lui ajouter un pas ferait glisser la nappe, ce qu'il ne fait
    // pas.
    if (track.lift_key >= kGridPlaneWrapAt) {
      track.lift_key -= kGridPlaneWrapAt;
    }

    if (track.mark[kMarkOfGridPlaneTint] != kMarkThatPulses) {
      if (track.alpha <= kRiseWhileAtMost) {
        track.alpha = static_cast<std::int16_t>(track.alpha + kSlowRise);
      }
      continue;
    }
    if (track.ticks < kLastTickOfFastRise) {
      if (track.alpha <= kRiseWhileAtMost) {
        track.alpha = static_cast<std::int16_t>(track.alpha + kFastRise);
      }
    } else if (track.ticks > kFirstTickOfFall && track.alpha >= kFallWhileAtLeast) {
      track.alpha = static_cast<std::int16_t>(track.alpha - kFall);
    }
  }
}

}  // namespace ro::render
