// LES ANNEAUX QUI SUIVENT LEUR PORTEUR : trois formes de la même jupe, dont le
// mouvement recopie d'abord la place du porteur, puis fait vivre les pistes —
// une ouverture en sinus pour le quatuor, une onde pour l'anneau tournant, un
// cercle qui grandit pour la gerbe.
//
// ⚠ SORTI DU CATALOGUE DES MOUVEMENTS, qui était à huit cents lignes : voir
// `effect_node_motion.h`.

#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_ring_quartet_seed.h"
#include "ro/render/effect_ring_starburst_seed.h"
#include "ro/render/effect_turning_ring_seed.h"

#include "effect_node_motion.h"

namespace ro::render {

void AdvanceFollowingRingTracksOf(PooledEffectNode& pooled) {
  // ⚠ CES MOUVEMENTS SUIVENT LEUR PORTEUR : le client recopie sa position dans
  // le nœud avant d'avancer les pistes.
  pooled.ground_position = pooled.owner_position;
  switch (pooled.shape) {
    case PooledNodeShape::kRingQuartet:
      // La jupe s'OUVRE en sinus, au lieu d'onduler. Voir `AdvanceRingQuartetTracks`.
      AdvanceRingQuartetTracks(pooled.tracks);
      return;
    case PooledNodeShape::kRingStarburst:
      // Le cercle GRANDIT pendant que la jupe se couche. Voir
      // `AdvanceRingStarburstTracks`.
      AdvanceRingStarburstTracks(pooled.tracks);
      return;
    case PooledNodeShape::kTurningRing:
      // L'onde de son frère, autour de sa clé. Voir `AdvanceTurningRingTracks`.
      AdvanceTurningRingTracks(pooled.tracks);
      return;
    default:
      return;
  }
}

}  // namespace ro::render
