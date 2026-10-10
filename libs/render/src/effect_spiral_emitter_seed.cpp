#include "ro/render/effect_spiral_emitter_seed.h"

#include <algorithm>
#include <string>

#include "ro/render/effect_node.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_spawn_request.h"
#include "ro/render/effect_spiral_emitter.h"
#include "ro/render/effect_track.h"

namespace ro::render {
namespace {

constexpr std::size_t kLeadingTrack = 0;

}  // namespace

std::span<const EffectSpiralEmitterPattern> EffectSpiralEmitterPatternsFor(
    std::uint32_t effect_id) {
  const std::span<const EffectSpiralEmitterPattern> table = EffectSpiralEmitterPatternTable();
  const auto [first, last] =
      std::ranges::equal_range(table, effect_id, {}, &EffectSpiralEmitterPattern::effect_id);
  return {first, last};
}

std::array<EffectTrack, kEffectTracksPerNode> SeedSpiralEmitterTracks(
    std::uint8_t byte, std::uint32_t designated_actor_id, std::uint32_t& generator) {
  // Les quatre pistes éteintes, puis la première allumée.
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  EffectTrack& leader = tracks[kLeadingTrack];
  leader.active = true;
  leader.ticks = 0;
  leader.radius_at_anchor = kSpiralEmitterSquareRadius;
  leader.value[kSpiralEmitterSlotOfDesignatedActor] = StoredDesignatedActor(designated_actor_id);
  leader.value[kSpiralEmitterSlotOfAlpha] = 0.0F;
  leader.value[kSpiralEmitterSlotOfUnreadFirst] = 0.0F;
  leader.value[kSpiralEmitterSlotOfUnreadSecond] = 0.0F;
  // LES QUATRE TIRAGES que personne ne relit, rangés dans les quatre premières
  // cases : ils font avancer le générateur comme chez le client.
  for (std::size_t rang = 0; rang < kSpiralEmitterUnreadAngles; ++rang) {
    leader.value[rang] = static_cast<float>(NextEffectNodeDraw(generator) %
                                            static_cast<std::uint32_t>(kSpiralEmitterAngleDraws));
  }
  leader.tail[kSpiralEmitterSlotOfSpiralRadius] = kSpiralEmitterFirstRadius;
  leader.tail[kSpiralEmitterSlotOfOffsetX] = kSpiralEmitterBirthOffsetX;
  leader.tail[kSpiralEmitterSlotOfOffsetDown] = kSpiralEmitterBirthOffsetDown;
  leader.tail[kSpiralEmitterSlotOfOffsetZ] = 0.0F;
  leader.mark[kSpiralEmitterMarkOfByte] = byte;
  return tracks;
}

PooledEffectNode SowSpiralEmitterNode(const EffectSpiralEmitterPattern& pattern,
                                      glm::vec3 owner_position, std::int32_t effect_life_ticks,
                                      std::uint32_t designated_actor_id, std::uint32_t& generator) {
  PooledEffectNode pooled;
  pooled.shape = PooledNodeShape::kSpiralEmitter;
  pooled.ground_position = owner_position;
  pooled.owner_position = owner_position;
  pooled.node.life_ticks = std::max(effect_life_ticks, 1);
  pooled.node.render_word = kSpiralEmitterRenderWord;
  // ⚠ L'IMAGE EST CHARGÉE ET JAMAIS PEINTE : elle reste nommée pour dire ce que
  // le bâtisseur lit, et la composition ne la cherche pas.
  pooled.resource_paths = {std::string(pattern.texture)};
  pooled.tracks = SeedSpiralEmitterTracks(pattern.byte, designated_actor_id, generator);
  return pooled;
}

}  // namespace ro::render
