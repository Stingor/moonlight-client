// LE MOUVEMENT DES FORMES QUI NE TIRENT RIEN ET NE LISENT PAS LE MONDE, un cas
// par forme : les panneaux qui rangent leurs angles de caméra, et les deux
// formes qui ne peignent rien et font naître d'autres effets.
//
// ⚠ SORTI DE `effect_node_motion.cpp`, arrivé au plafond de huit cents lignes.
// La coupure suit ce que ces formes ont en commun : leur mouvement ne tire rien
// au sort et ne lit pas le monde. Voir `effect_node_motion.h`.

#include "ro/render/effect_arcing_light.h"
#include "ro/render/effect_bobbing_note.h"
#include "ro/render/effect_break_ring.h"
#include "ro/render/effect_colored_veil.h"
#include "ro/render/effect_floor_spinner.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_radial_petal.h"
#include "ro/render/effect_shockwave_ray.h"
#include "ro/render/effect_spinning_box.h"
#include "ro/render/effect_spiral_emitter.h"
#include "ro/render/effect_tapered_sheet.h"
#include "ro/render/effect_track_motion.h"
#include "ro/render/effect_walking_sower.h"
#include "ro/render/effect_walking_sower_seed.h"

#include "effect_node_motion.h"

namespace ro::render {

void AdvancePanelTracksOf(PooledEffectNode& pooled) {
  switch (pooled.shape) {
    case PooledNodeShape::kRadialPetal:  // ne recopie PAS la place du porteur
      AdvanceRadialPetalTracks(pooled.tracks, pooled.billboard_turn_degrees,
                               pooled.billboard_tilt_degrees);
      return;
    case PooledNodeShape::kArcingLight:  // recopie la place du porteur à chaque tic
      pooled.ground_position = pooled.owner_position;
      AdvanceArcingLightTracks(pooled.tracks, pooled.billboard_turn_degrees,
                               pooled.billboard_tilt_degrees);
      return;
    case PooledNodeShape::kShockwaveRays:  // ne recopie PAS la place du porteur
      AdvanceShockwaveRayTracks(pooled.tracks, pooled.billboard_turn_degrees,
                                pooled.billboard_tilt_degrees);
      return;
    case PooledNodeShape::kTaperedSheets:  // recopie la place du porteur à chaque tic
      pooled.ground_position = pooled.owner_position;
      AdvanceTaperedSheetTracks(pooled.tracks, pooled.ground_position,
                                pooled.billboard_turn_degrees, pooled.billboard_tilt_degrees);
      return;
    case PooledNodeShape::kSpiralEmitter:  // recopie la place du porteur à chaque tic
      pooled.ground_position = pooled.owner_position;
      AdvanceSpiralEmitterTracks(pooled.tracks, pooled.billboard_turn_degrees,
                                 pooled.billboard_tilt_degrees);
      return;
    case PooledNodeShape::kWalkingSower:  // le cap se prend depuis la place du nœud
      AdvanceWalkingSowerTracks(
          pooled.tracks, WalkingSowerClientHeading(pooled.ground_position, pooled.target_position));
      return;
    case PooledNodeShape::kFloorSpinner:  // recopie la place du porteur à chaque tic
      pooled.ground_position = pooled.owner_position;
      AdvanceFloorSpinnerTracks(pooled.tracks, pooled.resource_paths.size());
      // L'IMAGE EST CELLE DE LA PREMIÈRE PISTE : la composition la lit dans le nœud.
      pooled.node.frame = FloorSpinnerImageOf(pooled.tracks.front());
      return;
    case PooledNodeShape::kBobbingNote:  // recopie la place du porteur à chaque tic
      pooled.ground_position = pooled.owner_position;
      AdvanceBobbingNoteTracks(pooled.tracks, pooled.ground_position, pooled.billboard_turn_degrees,
                               pooled.billboard_tilt_degrees);
      return;
    case PooledNodeShape::kColoredVeil:  // le mouvement de la nappe au sol
      pooled.ground_position = pooled.owner_position;
      AdvanceParticleFrameTracks(pooled.tracks, pooled.billboard_turn_degrees,
                                 pooled.billboard_tilt_degrees);
      return;
    case PooledNodeShape::kBreakRings:  // recopie la place du porteur à chaque tic
      pooled.ground_position = pooled.owner_position;
      AdvanceBreakRingTracks(pooled.tracks);
      return;
    case PooledNodeShape::kSpinningBox:  // recopie la place du porteur à chaque tic
      pooled.ground_position = pooled.owner_position;
      AdvanceSpinningBoxTracks(pooled.tracks);
      return;
    default:
      // ⚠ AUCUNE AUTRE FORME N'ARRIVE ICI : `AdvanceTracksOf` n'y envoie que les
      // onze ci-dessus, et c'est SON `switch` sans `default` qui garde
      // l'exhaustivité.
      return;
  }
}

}  // namespace ro::render
