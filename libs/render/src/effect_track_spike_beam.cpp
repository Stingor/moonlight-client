#include <cstddef>

#include "ro/render/effect_spike_beam.h"
#include "ro/render/effect_track.h"
#include "ro/render/effect_track_motion.h"

namespace ro::render {
namespace {

// DE COMBIEN L'INDICE D'IMAGE AVANCE, et de combien le rayon de départ recule :
// d'une unité par tic, tous les deux. Le client les écrit en flottants.
constexpr float kOneStepPerTick = 1.0F;

}  // namespace

void AdvanceFourLayerTimerTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                                 std::size_t texture_count) {
  for (EffectTrack& track : tracks) {
    if (!track.active) {
      continue;
    }
    ++track.ticks;
    // Une piste retardée ne bouge pas tant que son compte n'est pas remonté à un.
    if (track.ticks <= 0) {
      continue;
    }

    // ⚠ L'INDICE REVIENT À ZÉRO QUAND IL ATTEINT LE COMPTE, non quand il le
    // dépasse : le client compare par « supérieur ou égal ». Un nœud à une seule
    // image garde donc l'indice zéro, ce qui est exactement ce qu'il faut.
    track.value[kSlotOfSpikeBeamImage] += kOneStepPerTick;
    if (track.value[kSlotOfSpikeBeamImage] >= static_cast<float>(texture_count)) {
      track.value[kSlotOfSpikeBeamImage] = 0.0F;
    }

    // ⚠ ET LE RAYON DE DÉPART RECULE, il ne croît pas. C'est ce recul qui fait
    // GRANDIR le faisceau : sa portée maximale est fixe, si bien qu'un départ
    // plus proche laisse tenir un segment de plus. Le remonter d'un pas quand il
    // passe sous zéro fait recommencer le cycle sans jamais le laisser filer.
    track.radius_at_anchor -= kOneStepPerTick;
    if (track.radius_at_anchor < 0.0F) {
      track.radius_at_anchor += track.lift_key;
    }
  }
}

}  // namespace ro::render
