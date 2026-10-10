#include "ro/render/effect_aimed_spoke_seed.h"

#include <algorithm>
#include <string>

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
  const std::span<const EffectAimedSpokePattern> table = EffectAimedSpokePatternTable();
  const auto [first, last] =
      std::ranges::equal_range(table, effect_id, {}, &EffectAimedSpokePattern::effect_id);
  return {first, last};
}

std::int16_t AimedSpokeClientHeading(glm::vec3 node_position,
                                     const std::optional<glm::vec3>& target) {
  if (!target.has_value()) {
    return 0;
  }
  // ⚠ LA PROFONDEUR SE RETOURNE : le client prend `cible − instance` dans son
  // repère, où notre z est opposé.
  return static_cast<std::int16_t>(
      HeadingOfVector(target->x - node_position.x, node_position.z - target->z));
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
  PooledEffectNode pooled;
  pooled.shape = PooledNodeShape::kAimedSpokes;
  pooled.ground_position = instance_position;
  pooled.owner_position = instance_position;
  pooled.target_position = target;
  pooled.node.life_ticks = std::max(effect_life_ticks, 1);
  pooled.node.render_word = kAimedSpokeRenderWord;
  pooled.resource_paths = {std::string(pattern.texture)};
  pooled.tracks =
      SeedAimedSpokeTracks(pattern.octet, AimedSpokeClientHeading(instance_position, target));
  return pooled;
}

}  // namespace ro::render
