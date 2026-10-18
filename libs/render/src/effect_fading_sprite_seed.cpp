#include "ro/render/effect_fading_sprite_seed.h"

#include <string>

#include "ro/core/sorted_table.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_trail_history.h"

namespace ro::render {

std::span<const EffectFadingSpritePattern> EffectFadingSpritePatternsFor(std::uint32_t effect_id) {
  return core::RowsWithKey(EffectFadingSpritePatternTable(), effect_id,
                           &EffectFadingSpritePattern::effect_id);
}

std::array<EffectTrack, kEffectTracksPerNode> SeedEffectFadingSpriteTracks(
    const EffectFadingSpritePattern& pattern, glm::vec3 anchor, std::int32_t tick) {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  EffectTrack& single = tracks.front();
  single.active = true;
  single.ticks = 0;
  const bool hanbok =
      pattern.style == kFadingSpriteHanbokBody || pattern.style == kFadingSpriteHanbokWings;
  single.anchor = {anchor.x,
                   anchor.y + (hanbok ? kFadingSpriteHanbokSpawnHeight : kFadingSpriteSpawnHeight),
                   anchor.z};
  single.mark[kFadingSpriteMarkOfStyle] = pattern.style;
  // ⚠ LE TIC DE NAISSANCE EST RANGÉ EN OCTET, et le client ne s'en garde pas :
  // c'est lui qui plafonne l'alpha des nœuds resemés, et c'est ce qui pâlit
  // chaque battement d'ailes un peu plus que le précédent.
  single.mark[kFadingSpriteMarkOfBirthTick] = static_cast<std::uint8_t>(tick);
  return tracks;
}

PooledEffectNode SowEffectFadingSpriteNode(const EffectFadingSpritePattern& pattern,
                                           glm::vec3 anchor, std::int32_t effect_life_ticks,
                                           std::int32_t tick) {
  PooledEffectNode pooled;
  pooled.shape = PooledNodeShape::kFadingSprite;
  pooled.node.life_ticks = effect_life_ticks;
  // ⚠ L'ALPHA NAÎT NUL : le sprite s'allume sous le mouvement, et le semer
  // brillant lui retirerait son apparition.
  pooled.node.alpha = 0.0F;
  pooled.node.frame = 0;
  pooled.tracks = SeedEffectFadingSpriteTracks(pattern, anchor, tick);
  // LE NŒUD PART DE SON ANCRE, déjà plus haut que le point de l'effet.
  pooled.ground_position = pooled.tracks.front().anchor;
  pooled.owner_position = anchor;
  pooled.resource_paths = {std::string(pattern.sprite_base)};
  pooled.trail.resize(kFadingSpriteTrailSteps);
  for (EffectTrailStep& notch : pooled.trail) {
    notch.ground_position = pooled.ground_position;
  }
  return pooled;
}

}  // namespace ro::render
