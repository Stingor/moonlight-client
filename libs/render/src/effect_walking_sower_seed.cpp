#include "ro/render/effect_walking_sower_seed.h"

#include <algorithm>

#include "ro/core/sorted_table.h"
#include "ro/render/effect_node.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_track.h"
#include "ro/render/effect_walking_sower.h"

#include "effect_track_angles.h"

namespace ro::render {

std::span<const EffectWalkingSowerPattern> EffectWalkingSowerPatternsFor(std::uint32_t effect_id) {
  return core::RowsWithKey(EffectWalkingSowerPatternTable(), effect_id,
                           &EffectWalkingSowerPattern::effect_id);
}

std::array<EffectTrack, kEffectTracksPerNode> SeedWalkingSowerTracks(std::uint8_t byte,
                                                                     glm::vec3 instance_position,
                                                                     std::uint32_t& generator) {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  std::int32_t delay = kWalkingSowerFirstTrackTicks;
  for (EffectTrack& track : tracks) {
    track.active = true;
    track.ticks = delay;
    ++delay;
    track.alpha = 0;
    track.degrees = 0;
    // ⚠ LA HAUTEUR TIRÉE SE CONVERTIT : le client l'AJOUTE dans un repère qui
    // descend, elle se RETRANCHE dans le nôtre.
    const auto draw = static_cast<float>(NextEffectNodeDraw(generator) %
                                         static_cast<std::uint32_t>(kWalkingSowerHeightDraws));
    track.anchor = {instance_position.x, instance_position.y - kWalkingSowerHeightBelowFloor - draw,
                    instance_position.z};
    track.value[kWalkingSowerSlotOfSpeed] = kWalkingSowerSpeed;
    track.mark[kWalkingSowerMarkOfByte] = byte;
  }
  return tracks;
}

std::int16_t WalkingSowerClientHeading(glm::vec3 node_position,
                                       const std::optional<glm::vec3>& target) {
  if (!target.has_value()) {
    return 0;
  }
  return static_cast<std::int16_t>(ClientHeadingToward(node_position, *target));
}

PooledEffectNode SowWalkingSowerNode(const EffectWalkingSowerPattern& pattern,
                                     glm::vec3 instance_position,
                                     const std::optional<glm::vec3>& target,
                                     std::int32_t effect_life_ticks, std::uint32_t& generator) {
  PooledEffectNode pooled = PooledNodeAt(PooledNodeShape::kWalkingSower, instance_position);
  pooled.target_position = target;
  pooled.node.life_ticks = std::max(effect_life_ticks, 1);
  pooled.tracks = SeedWalkingSowerTracks(pattern.byte, instance_position, generator);
  return pooled;
}

}  // namespace ro::render
