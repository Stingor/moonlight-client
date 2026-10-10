#include <cstddef>

#include "ro/render/effect_heal_sparkle.h"
#include "ro/render/effect_track.h"

namespace ro::render {
namespace {

// L'ANGLE PROPRE de la piste, celui que la figure lit pour faire respirer son
// rayon. Même case que l'arc de ruban et le carré teinté — c'est la case 8.
constexpr std::size_t kSlotOfSpinDegrees = 8;

// Le tour complet, en degrés, tel que le mouvement le replie.
constexpr float kFullTurnDegrees = static_cast<float>(kEffectTrackFullTurnDegrees);

}  // namespace

void AdvanceHealSparkleTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                              std::int16_t camera_turn_degrees, std::int16_t camera_tilt_degrees) {
  tracks[0].style = camera_turn_degrees;
  tracks[1].style = camera_tilt_degrees;

  EffectTrack& track = tracks.front();
  if (!track.active) {
    return;
  }
  ++track.ticks;

  // ⚠ L'ANGLE SE REPLIE PAR UNE SOUSTRACTION, PAS PAR UN RESTE. Le client
  // retranche le tour une seule fois : un pas qui dépasserait le tour laisserait
  // l'angle hors bornes, et la figure cesse alors de respirer au lieu de
  // repartir au milieu du cycle. Trois degrés ne dépassent jamais, mais recopier
  // le geste plutôt que son intention garde les deux d'accord.
  float& own = track.value[kSlotOfSpinDegrees];
  own += kHealSparkleSpinPerTick;
  if (own >= kFullTurnDegrees) {
    own -= kFullTurnDegrees;
  }

  if (track.ticks <= kHealSparkleLastTickOfRise) {
    track.alpha = static_cast<std::int16_t>(track.alpha + kHealSparkleAlphaRise);
  }
  // ⚠ LE PALIER N'EST PAS UN `else` : la montée et le palier se recouvrent au
  // vingtième tic, et le client monte l'alpha AVANT de tester la fin du palier.
  // En faire deux branches exclusives retirerait un pas de montée.
  if (track.ticks <= kHealSparkleLastTickOfHold) {
    return;
  }
  track.alpha = static_cast<std::int16_t>(track.alpha - kHealSparkleAlphaFall);
  if (track.alpha < 0) {
    track.alpha = 0;
  }
}

}  // namespace ro::render
