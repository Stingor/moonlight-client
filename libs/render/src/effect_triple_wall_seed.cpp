#include "ro/render/effect_triple_wall_seed.h"

#include <string>

#include "ro/core/sorted_table.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_track.h"

namespace ro::render {
namespace {

// CE QUE LES QUATRE BÂTISSEURS PARTAGENT, champ pour champ.
//
// ⚠ ILS NAISSENT INVISIBLES ET DÉJÀ ÉPAIS : alpha zéro, rayon à l'ancre deux,
// rayon intérieur UN. C'est le mouvement qui les allume et les éloigne ; les
// semer visibles les ferait paraître d'un coup, collés au centre.
constexpr std::int16_t kStartsInvisible = 0;
constexpr float kThicknessAtBirth = 2.0F;
constexpr float kInnerRadiusAtBirth = 1.0F;
constexpr std::size_t kSlotOfInnerRadius = 0;

}  // namespace

std::span<const EffectTripleWallPattern> EffectTripleWallPatternsFor(std::uint32_t effect_id) {
  return core::RowsWithKey(EffectTripleWallPatternTable(), effect_id,
                           &EffectTripleWallPattern::effect_id);
}

std::array<EffectTrack, kEffectTracksPerNode> SeedEffectTripleWallTracks(
    const EffectTripleWallPattern& pattern, std::int16_t node_azimuth_degrees) {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  for (std::size_t rang = 0; rang < tracks.size(); ++rang) {
    EffectTrack& track = tracks[rang];
    track.active = true;
    track.ticks = 0;
    track.alpha = kStartsInvisible;
    // Les quatre pistes d'un nœud se partagent le tour, et le nœud entier part
    // de l'azimut que son site lui donne.
    track.azimuth_degrees = static_cast<std::int16_t>(
        node_azimuth_degrees + kTripleWallDegreesPerTrack * static_cast<std::int16_t>(rang));
    track.lift_key = pattern.lift_key;
    track.radius_at_anchor = kThicknessAtBirth;
    track.value[kSlotOfInnerRadius] = kInnerRadiusAtBirth;
    track.mark[0] = pattern.mark;
  }
  return tracks;
}

PooledEffectNode SowEffectTripleWallNode(const EffectTripleWallPattern& pattern,
                                         std::int16_t node_azimuth_degrees, glm::vec3 anchor,
                                         std::int32_t effect_life_ticks) {
  PooledEffectNode pooled = PooledNodeAt(PooledNodeShape::kTripleWall, anchor);
  pooled.node.life_ticks = effect_life_ticks;
  pooled.resource_paths = {std::string(pattern.texture)};
  pooled.trail_variant = pattern.trail_variant;
  pooled.tracks = SeedEffectTripleWallTracks(pattern, node_azimuth_degrees);
  return pooled;
}

}  // namespace ro::render
