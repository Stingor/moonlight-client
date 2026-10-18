#include "ro/render/effect_flying_shard_seed.h"

#include <string>

#include "ro/core/sorted_table.h"
#include "ro/render/effect_flying_shard.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_track.h"

#include "effect_draw_below.h"

namespace ro::render {

std::span<const EffectFlyingShardPattern> EffectFlyingShardPatternsFor(std::uint32_t effect_id) {
  return core::RowsWithKey(EffectFlyingShardPatternTable(), effect_id,
                           &EffectFlyingShardPattern::effect_id);
}

std::array<EffectTrack, kEffectTracksPerNode> SeedEffectFlyingShardTracks(
    const EffectFlyingShardPattern& pattern, std::uint32_t& generator) {
  const bool elance = pattern.builder == EffectFlyingShardBuilder::kSlender;
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  for (EffectTrack& track : tracks) {
    track.active = true;
    // ⚠ LE RETARD EST TIRÉ POUR CHAQUE PISTE : les quatre éclats ne partent
    // pas ensemble. Les semer au même tic en ferait un seul trait épais.
    track.ticks =
        elance
            ? -kFlyingShardSlenderDelayFloor - DrawBelow(generator, kFlyingShardSlenderDelaySpread)
            : -kFlyingShardStoutDelayFloor - DrawBelow(generator, kFlyingShardStoutDelaySpread);
    track.radius_at_anchor = kFlyingShardStartRadius;
    track.value[kFlyingShardSlotOfGrowth] =
        elance ? kFlyingShardSlenderGrowth : kFlyingShardStoutGrowth;
    // ⚠ DEUX ANGLES TIRÉS, ET DE NATURES DIFFÉRENTES : l'inclinaison donne
    // l'azimut du PLAN, les degrés l'orientation DANS ce plan. Les tirer
    // ensemble n'est qu'une coïncidence de l'écriture du client.
    track.tilt_degrees =
        static_cast<std::int16_t>(DrawBelow(generator, kEffectTrackFullTurnDegrees));
    track.degrees = static_cast<std::int16_t>(DrawBelow(generator, kEffectTrackFullTurnDegrees));
    track.value[kFlyingShardSlotOfHalfWidth] =
        elance ? kFlyingShardSlenderHalfWidth : kFlyingShardStoutHalfWidth;
    // ⚠ IL NAÎT INVISIBLE, et son mouvement commence par le faire TOMBER : il
    // ne monte qu'entre le onzième et le vingtième tic. Le semer plein le
    // ferait paraître d'un coup puis disparaître.
    track.alpha = 0;
    track.mark[kFlyingShardMarkOfColor] = pattern.mark;
    track.mark[kFlyingShardMarkOfLength] =
        elance ? kFlyingShardSlenderLength : kFlyingShardStoutLength;
  }
  return tracks;
}

PooledEffectNode SowEffectFlyingShardNode(const EffectFlyingShardPattern& pattern, glm::vec3 anchor,
                                          std::int32_t effect_life_ticks,
                                          std::uint32_t& generator) {
  // ⚠ LES DEUX BÂTISSEURS MONTENT LE NŒUD DE NEUF : c'est à hauteur de
  // poitrine que les éclats partent. Notre repère monte, le sien descend, et
  // il RETRANCHE neuf — nous ajoutons.
  const glm::vec3 upward{anchor.x, anchor.y + kFlyingShardBirthHeight, anchor.z};
  PooledEffectNode pooled = PooledNodeAt(PooledNodeShape::kFlyingShard, upward);
  pooled.node.life_ticks = effect_life_ticks;
  pooled.resource_paths = {std::string(pattern.texture)};
  pooled.tracks = SeedEffectFlyingShardTracks(pattern, generator);
  return pooled;
}

}  // namespace ro::render
