#include "ro/render/effect_layered_ring_seed.h"

#include <algorithm>
#include <span>
#include <string>

#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_track.h"

namespace ro::render {

std::array<EffectTrack, kEffectTracksPerNode> SeedEffectLayeredRingTracks(
    std::span<const EffectLayeredRingTrackSeed> seeds) {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  // ⚠ LES RANGS AU-DELÀ DE LA TRANCHE RESTENT ÉTEINTS, et c'est une mesure : le
  // second bâtisseur éteint sa quatrième piste explicitement. Les allumer
  // toutes, fût-ce sans valeurs, donnerait une couronne de rayon nul de plus.
  for (std::size_t rang = 0; rang < tracks.size() && rang < seeds.size(); ++rang) {
    const EffectLayeredRingTrackSeed& seed = seeds[rang];
    EffectTrack& track = tracks[rang];
    track.active = true;
    track.ticks = 0;
    track.degrees = seed.degrees;
    track.azimuth_degrees = seed.azimuth_degrees;
    track.tilt_degrees = seed.tilt_degrees;
    track.alpha = seed.alpha;
    track.radius_at_anchor = seed.radius_at_anchor;
    track.lift_key = seed.lift_key;
    // Les longueurs de segment naissent nulles : c'est le mouvement qui donne
    // au ruban son aire.
  }
  return tracks;
}

PooledEffectNode SowEffectLayeredRingNode(const EffectLayeredRingNodePattern& pattern,
                                          glm::vec3 ground_position) {
  PooledEffectNode pooled;
  // ⚠ MÊME FIGURE QUE L'ANNEAU DU SOUS-TYPE 30, MOUVEMENT DIFFÉRENT. Le
  // dessinateur est le même — mesuré dans la table des sous-types —, mais le
  // répartiteur de mouvement donne au vingt-trois sa propre cible, qui délègue au
  // mouvement du cône de particules. La forme porte les deux, et c'est pourquoi
  // il en faut une à lui.
  pooled.shape = PooledNodeShape::kParticleConeRings;
  pooled.ground_position = ground_position;
  pooled.resource_paths = {std::string(pattern.texture)};
  pooled.trail_variant = pattern.trail_variant;
  pooled.waves_last_track = pattern.waves_last_track;
  // ⚠ LA VIE DU NŒUD DÉCIDE DE LA SORTIE : le mouvement fait redescendre l'alpha
  // quarante tics avant la fin. Une vie plus courte que quarante ferait un nœud
  // qui s'éteint avant d'avoir paru. Elle vient du PATRON, les deux bâtisseurs
  // ne la posant pas de la même façon — et l'un ne la posant pas du tout.
  pooled.node.life_ticks = pattern.life_ticks;
  // Le nœud naît là où il est semé ; c'est `app` qui dira où son porteur s'est
  // rendu, et le mouvement l'y suivra.
  pooled.owner_position = ground_position;
  pooled.tracks = SeedEffectLayeredRingTracks(EffectLayeredRingTracksFor(pattern));
  return pooled;
}

}  // namespace ro::render
