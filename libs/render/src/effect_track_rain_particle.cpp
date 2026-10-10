#include <array>

#include "ro/render/effect_rain_particle.h"
#include "ro/render/effect_track.h"
#include "ro/render/effect_track_motion.h"

#include "effect_track_angles.h"

namespace ro::render {
namespace {

// LA FENÊTRE OÙ LE GRAIN S'ALLUME, et de combien son alpha y monte.
constexpr std::int32_t kLastTickOfRise = 10;
constexpr std::int16_t kAlphaRise = 25;
// CE QU'IL PERD À CHAQUE TIC UNE FOIS REDESCENDU À SON NŒUD.
constexpr std::int16_t kAlphaFall = 15;

constexpr std::int16_t kFullAlpha = 255;

}  // namespace

void AdvanceCameraFacingRadialTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
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
    if (track.ticks < kLastTickOfRise) {
      track.alpha = static_cast<std::int16_t>(track.alpha + kAlphaRise);
    }
    if (track.alpha >= kFullAlpha) {
      track.alpha = kFullAlpha;
    }

    // ⚠ LA DESCENTE ET L'EXTINCTION SONT LA MÊME BRANCHE. L'ancre est dans le
    // repère du client, qui descend : tant que son ordonnée est négative, le
    // grain est AU-DESSUS de son nœud, il descend de sa clé et garde son alpha ;
    // dès qu'il l'a rejoint il s'arrête et commence à perdre quinze par tic. Les
    // séparer ferait un grain qui tombe indéfiniment ou qui s'éteint en route.
    if (track.anchor.y < 0.0F) {
      track.anchor.y += track.lift_key;
    } else {
      track.alpha = static_cast<std::int16_t>(track.alpha - kAlphaFall);
    }

    // ⚠ L'AZIMUT NE TOURNE PAS. Le client recalcule l'abscisse et la profondeur
    // à chaque tic depuis un azimut et un rayon que le semis a tirés une fois
    // pour toutes : le grain tombe DROIT, sur une verticale posée au hasard dans
    // un disque. Le faire tourner en ferait une spirale que le client n'a pas.
    const float radius = track.value[kSlotOfRainParticleOrbitRadius];
    track.anchor.x = radius * CosOfDegrees(track.degrees);
    track.anchor.z = radius * SinOfDegrees(track.degrees);

    // ⚠ ET LA PISTE MEURT PAR SON ALPHA, jamais par son âge : le nœud n'a pas de
    // compte de tics à vivre, c'est l'extinction qui l'éteint.
    if (track.alpha < 0) {
      track.alpha = 0;
      track.active = false;
    }
    if (track.degrees >= kEffectTrackFullTurnDegrees) {
      track.degrees = static_cast<std::int16_t>(track.degrees - kEffectTrackFullTurnDegrees);
    }
  }
}

}  // namespace ro::render
