#include "ro/render/effect_aimed_spoke_seed.h"

#include <algorithm>
#include <string>

#include "ro/core/sorted_table.h"
#include "ro/render/effect_aimed_spoke.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_track.h"

#include "effect_track_angles.h"

namespace ro::render {
namespace {

constexpr std::size_t kLeadingTrack = 0;

}  // namespace

std::span<const EffectAimedSpokePattern> EffectAimedSpokePatternsFor(std::uint32_t effect_id) {
  return core::RowsWithKey(EffectAimedSpokePatternTable(), effect_id,
                           &EffectAimedSpokePattern::effect_id);
}

std::int16_t AimedSpokeClientHeading(glm::vec3 node_position,
                                     const std::optional<glm::vec3>& target) {
  if (!target.has_value()) {
    return 0;
  }
  return static_cast<std::int16_t>(ClientHeadingToward(node_position, *target));
}

std::array<EffectTrack, kEffectTracksPerNode> SeedAimedSpokeTracks(
    std::uint8_t octet, std::int16_t client_heading_degrees) {
  // Les quatre pistes éteintes, puis la première allumée.
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  EffectTrack& leader = tracks[kLeadingTrack];
  leader.active = true;
  leader.ticks = 0;
  leader.alpha = 0;
  leader.radius_at_anchor = kAimedSpokeUnreadRadius;
  leader.value[kAimedSpokeSlotOfLength] = kAimedSpokeLength;
  leader.value[kAimedSpokeSlotOfUnread] = kAimedSpokeUnreadValue;
  leader.lift_key = kAimedSpokeWidth;
  leader.tilt_degrees = client_heading_degrees;
  leader.mark[kAimedSpokeMarkOfTint] = octet;
  return tracks;
}

PooledEffectNode SowAimedSpokeNode(const EffectAimedSpokePattern& pattern,
                                   glm::vec3 instance_position,
                                   const std::optional<glm::vec3>& target,
                                   std::int32_t effect_life_ticks) {
  PooledEffectNode pooled = PooledNodeAt(PooledNodeShape::kAimedSpokes, instance_position);
  pooled.target_position = target;
  pooled.node.life_ticks = std::max(effect_life_ticks, 1);
  pooled.node.render_word = kAimedSpokeRenderWord;
  pooled.resource_paths = {std::string(pattern.texture)};
  pooled.tracks =
      SeedAimedSpokeTracks(pattern.octet, AimedSpokeClientHeading(instance_position, target));
  return pooled;
}

}  // namespace ro::render
