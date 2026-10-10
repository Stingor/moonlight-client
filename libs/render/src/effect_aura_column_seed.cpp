#include "ro/render/effect_aura_column_seed.h"

#include <algorithm>
#include <string>

#include "ro/render/effect_aura_column.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_track.h"

namespace ro::render {

std::span<const EffectAuraColumnPattern> EffectAuraColumnPatternsFor(std::uint32_t effect_id) {
  const std::span<const EffectAuraColumnPattern> table = EffectAuraColumnPatternTable();
  const auto first =
      std::ranges::lower_bound(table, effect_id, {}, &EffectAuraColumnPattern::effect_id);
  const auto last =
      std::ranges::upper_bound(table, effect_id, {}, &EffectAuraColumnPattern::effect_id);
  return table.subspan(static_cast<std::size_t>(first - table.begin()),
                       static_cast<std::size_t>(last - first));
}

std::array<EffectTrack, kEffectTracksPerNode> SeedEffectAuraColumnTracks() {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  // ⚠ UNE SEULE PISTE VIVE : le bâtisseur éteint les quatre puis rallume la
  // première.
  EffectTrack& track = tracks.front();
  track.active = true;
  track.ticks = 0;
  track.alpha = 0;
  track.radius_at_anchor = kAuraColumnRadius;
  // LE DÉCALAGE DE LA COLONNE SUR SON AXE, nul : elle est centrée sur son ancre.
  track.value[kSlotOfAuraColumnShift] = 0.0F;
  track.mark[kMarkOfAuraColumnImage] = 0;
  track.mark[kMarkOfAuraColumnShape] = kAuraColumnShapeMark;
  // ⚠ LES DIX COUCHES NAISSENT ÉTEINTES, et le bâtisseur les met à zéro
  // nommément : rien ne se voit tant que le mouvement n'a pas tourné.
  for (std::size_t layer = 1; layer <= kAuraColumnLayers; ++layer) {
    track.value[layer] = 0.0F;
  }
  return tracks;
}

PooledEffectNode SowEffectAuraColumnNode(const EffectAuraColumnPattern& pattern, glm::vec3 anchor,
                                         std::int32_t effect_life_ticks) {
  PooledEffectNode pooled;
  pooled.shape = PooledNodeShape::kAuraColumn;
  pooled.ground_position = anchor;
  pooled.owner_position = anchor;
  pooled.node.life_ticks = effect_life_ticks;
  pooled.resource_paths = {std::string(pattern.texture)};
  pooled.tracks = SeedEffectAuraColumnTracks();
  return pooled;
}

}  // namespace ro::render
