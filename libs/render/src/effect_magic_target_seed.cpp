#include "ro/render/effect_magic_target_seed.h"

#include <string>

#include "ro/core/sorted_table.h"
#include "ro/render/effect_magic_target.h"
#include "ro/render/effect_node.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_track.h"

namespace ro::render {

std::span<const EffectMagicTargetPattern> EffectMagicTargetPatternsFor(std::uint32_t effect_id) {
  return core::RowsWithKey(EffectMagicTargetPatternTable(), effect_id,
                           &EffectMagicTargetPattern::effect_id);
}

std::array<EffectTrack, kEffectTracksPerNode> SeedEffectMagicTargetTracks() {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  EffectTrack& track = tracks.front();
  track.active = true;
  track.azimuth_degrees = 0;
  // ⚠ L'ÉCHELLE EST POSÉE ICI, ET LE BÂTISSEUR NE L'ÉCRIT PAS. Ce n'est pas une
  // valeur inventée : c'est celle que le mouvement donne à l'azimut zéro, que le
  // bâtisseur pose juste au-dessus — le sinus de zéro est nul, il reste le
  // socle. L'état semé est donc un point de la courbe du mouvement, et non un
  // état que le client n'atteindrait jamais.
  //
  // La laisser nulle coûterait une image : toutes les coordonnées de texture
  // vaudraient alors un demi, et les quarante-neuf cases montreraient le même
  // texel — un aplat lavande à la place du cercle.
  track.lift_key = kMagicTargetScaleFloor;
  return tracks;
}

PooledEffectNode SowEffectMagicTargetNode(const EffectMagicTargetPattern& pattern, glm::vec3 anchor,
                                          std::int32_t effect_life_ticks) {
  PooledEffectNode pooled = PooledNodeAt(PooledNodeShape::kMagicTargetGrid, anchor);
  pooled.node.life_ticks = effect_life_ticks;
  pooled.resource_paths = {std::string(pattern.texture)};
  pooled.tracks = SeedEffectMagicTargetTracks();
  return pooled;
}

}  // namespace ro::render
