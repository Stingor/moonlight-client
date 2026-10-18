#include <array>

#include "ro/render/effect_track.h"
#include "ro/render/effect_track_motion.h"

#include "effect_track_angles.h"

namespace ro::render {
namespace {

// LA FENÊTRE D'ALLUMAGE, et celle d'extinction. Entre les deux — du dixième au
// quarantième tic — l'alpha ne bouge PAS : le flocon vole à pleine lumière.
constexpr std::int32_t kLastTickOfRise = 10;
constexpr std::int32_t kFirstTickOfFall = 40;
constexpr std::int16_t kAlphaRise = 25;
constexpr std::int16_t kAlphaFall = 5;
constexpr std::int16_t kFullAlpha = 255;

}  // namespace

void AdvanceFadingStreakTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
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

    // ⚠ L'ANGLE DU QUATRE-COINS TOURNE D'UN DEGRÉ PAR TIC : le flocon tourne sur
    // lui-même pendant qu'il vole. C'est la seule chose qui bouge en dehors de
    // l'alpha ; l'avance, elle, se RECALCULE depuis le compteur par la figure.
    TurnOnward(track.degrees, 1);

    if (track.ticks < kLastTickOfRise) {
      track.alpha = static_cast<std::int16_t>(track.alpha + kAlphaRise);
    } else if (track.ticks > kFirstTickOfFall) {
      track.alpha = static_cast<std::int16_t>(track.alpha - kAlphaFall);
    }

    if (track.alpha >= kFullAlpha) {
      track.alpha = kFullAlpha;
    } else if (track.alpha < 0) {
      track.alpha = 0;
    }
  }
}

}  // namespace ro::render
