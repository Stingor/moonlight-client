#include "ro/render/effect_track.h"
#include "ro/render/effect_track_motion.h"

namespace ro::render {

void AdvanceEmitterFrameTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks) {
  // ⚠ CHAQUE PISTE VIVE, ET CELLES-LÀ SEULEMENT : le client teste l'allumage de
  // chacune des quatre avant d'avancer son compteur. Une piste éteinte ne
  // vieillit pas.
  for (EffectTrack& track : tracks) {
    if (track.active) {
      ++track.ticks;
    }
  }
}

}  // namespace ro::render
