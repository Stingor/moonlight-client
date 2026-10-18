#include "ro/render/effect_arcing_light_seed.h"

#include <algorithm>
#include <string>

#include "ro/core/sorted_table.h"
#include "ro/render/effect_arcing_light.h"
#include "ro/render/effect_node.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_spawn_request.h"
#include "ro/render/effect_track.h"

namespace ro::render {
namespace {

constexpr std::size_t kLeadingTrack = 0;

}  // namespace

std::span<const EffectArcingLightPattern> EffectArcingLightPatternsFor(std::uint32_t effect_id) {
  return core::RowsWithKey(EffectArcingLightPatternTable(), effect_id,
                           &EffectArcingLightPattern::effect_id);
}

std::array<EffectTrack, kEffectTracksPerNode> SeedArcingLightTracks(
    std::uint32_t& generator, std::uint32_t designated_actor_id) {
  // Les quatre pistes éteintes, puis la première allumée.
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  EffectTrack& leader = tracks[kLeadingTrack];
  leader.active = true;
  leader.ticks = 0;
  leader.radius_at_anchor = kArcingLightRadius;
  // L'ACTEUR DÉSIGNÉ, rangé en flottant — voir `StoredDesignatedActor`.
  leader.value[kArcingLightSlotOfDesignatedActor] = StoredDesignatedActor(designated_actor_id);
  leader.value[kArcingLightSlotOfAlpha] = 0.0F;
  leader.value[kArcingLightSlotOfUnreadFirst] = 0.0F;
  leader.value[kArcingLightSlotOfUnreadSecond] = 0.0F;
  // LES QUATRE TIRAGES que personne ne relit, rangés dans les quatre premières
  // cases : ils font avancer le générateur comme chez le client.
  for (std::size_t rang = 0; rang < kArcingLightUnreadAngles; ++rang) {
    leader.value[rang] = static_cast<float>(NextEffectNodeDraw(generator) %
                                            static_cast<std::uint32_t>(kArcingLightAngleDraws));
  }
  leader.tail[kArcingLightSlotOfSweepDegrees] = 0.0F;
  leader.tail[kArcingLightSlotOfSideways] = kArcingLightBirthSideways;
  leader.tail[kArcingLightSlotOfDepthBelow] = kArcingLightBirthDepthBelow;
  leader.tail[kArcingLightSlotOfForward] = 0.0F;
  return tracks;
}

PooledEffectNode SowArcingLightNode(const EffectArcingLightPattern& pattern,
                                    glm::vec3 owner_position, std::int32_t effect_life_ticks,
                                    std::uint32_t& generator, std::uint32_t designated_actor_id) {
  PooledEffectNode pooled = PooledNodeAt(PooledNodeShape::kArcingLight, owner_position);
  pooled.node.life_ticks = std::max(effect_life_ticks, 1);
  pooled.node.render_word = kArcingLightRenderWord;
  pooled.resource_paths = {std::string(pattern.texture)};
  pooled.tracks = SeedArcingLightTracks(generator, designated_actor_id);
  return pooled;
}

}  // namespace ro::render
