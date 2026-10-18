#include "ro/render/effect_thunder_freeze_seed.h"

#include <algorithm>
#include <string>
#include <utility>

#include "ro/core/sorted_table.h"
#include "ro/render/effect_node.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_thunder_freeze.h"
#include "ro/render/effect_track.h"

namespace ro::render {
namespace {

constexpr std::size_t kLeadingTrack = 0;
// Le plus petit nombre demandé qui pose un nœud : la troncature de tout ce qui
// est en deçà vaut zéro, et zéro ne pose rien.
constexpr float kFewestLaunchedNodes = 1.0F;

[[nodiscard]] float DrawnAngle(std::uint32_t& generator) {
  return static_cast<float>(NextEffectNodeDraw(generator) %
                            static_cast<std::uint32_t>(kThunderFreezeAngleDraws));
}

}  // namespace

std::span<const EffectThunderFreezePattern> EffectThunderFreezePatternsFor(
    std::uint32_t effect_id) {
  return core::RowsWithKey(EffectThunderFreezePatternTable(), effect_id,
                           &EffectThunderFreezePattern::effect_id);
}

std::int32_t ThunderFreezeNodeCount(float launched_count) {
  // ⚠ LE CLIENT TRONQUE, puis juge le plafond sur l'entier. Juger le flottant
  // d'abord évite de convertir ce qu'aucun
  // entier ne tient, et rend la même réponse pour tout nombre fini.
  if (!(launched_count >= kFewestLaunchedNodes)) {
    return 0;
  }
  if (launched_count >= static_cast<float>(kThunderFreezeMaxNodes)) {
    return kThunderFreezeMaxNodes;
  }
  return static_cast<std::int32_t>(launched_count);
}

std::array<EffectTrack, kEffectTracksPerNode> SeedThunderFreezeTracks(std::int32_t node_rank,
                                                                      float launched_count,
                                                                      std::uint32_t& generator) {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  EffectTrack& leader = tracks[kLeadingTrack];
  leader.active = true;
  // LES SIX ANGLES DU TREMBLEMENT, dans l'ordre des tirages du client.
  for (std::size_t rang = 0; rang < kThunderFreezeWanderSlotsPerAxis * kThunderFreezeWanderAxes;
       ++rang) {
    leader.value[kThunderFreezeSlotOfWander + rang] = DrawnAngle(generator);
  }
  // L'entier multiplié, PUIS la division flottante.
  leader.value[kThunderFreezeSlotOfOrbitDegrees] =
      static_cast<float>(node_rank * kThunderFreezeOrbitSpacingDegrees) / launched_count;
  leader.value[kThunderFreezeSlotOfHeight] = kThunderFreezeClientHeight;
  leader.radius_at_anchor = kThunderFreezeCoreRadius;
  leader.value[kThunderFreezeSlotOfUnread] = 0.0F;
  leader.mark[kThunderFreezeMarkOfHalvedWander] = kThunderFreezeHalvesWander;
  leader.value[kThunderFreezeSlotOfPulseDegrees] = DrawnAngle(generator);
  leader.alpha = 0;

  std::int32_t delay = 0;
  for (EffectTrack& shard : tracks) {
    shard.ticks = delay;
    shard.degrees = static_cast<std::int16_t>(DrawnAngle(generator));
    shard.lift_key = kThunderFreezeShardBirthSize;
    shard.azimuth_degrees = static_cast<std::int16_t>(DrawnAngle(generator));
    shard.tilt_degrees = kThunderFreezeShardBirthAlpha;
    delay += kThunderFreezeShardDelayStep;
  }
  return tracks;
}

std::vector<PooledEffectNode> SowThunderFreezeNodes(glm::vec3 owner_position, float launched_count,
                                                    std::int32_t effect_life_ticks,
                                                    std::uint32_t& generator) {
  std::vector<PooledEffectNode> nodes;
  const std::int32_t count = ThunderFreezeNodeCount(launched_count);
  for (std::int32_t rang = 0; rang < count; ++rang) {
    // ⚠ LE NŒUD NAÎT SUR L'INSTANCE, sans écart, et l'instance est à la place
    // du porteur.
    PooledEffectNode pooled = PooledNodeAt(PooledNodeShape::kThunderFreeze, owner_position);
    pooled.node.life_ticks = std::max(effect_life_ticks, 1);
    pooled.node.render_word = kThunderFreezeRenderWord;
    pooled.resource_paths = {std::string(kThunderFreezeCoreImage),
                             std::string(kThunderFreezeShardImage)};
    pooled.tracks = SeedThunderFreezeTracks(rang, launched_count, generator);
    nodes.push_back(std::move(pooled));
  }
  return nodes;
}

}  // namespace ro::render
