#include "ro/render/effect_aura_column_seed.h"

#include <string>

#include "ro/core/sorted_table.h"
#include "ro/render/effect_aura_column.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_track.h"

namespace ro::render {

std::span<const EffectAuraColumnPattern> EffectAuraColumnPatternsFor(std::uint32_t effect_id) {
  return core::RowsWithKey(EffectAuraColumnPatternTable(), effect_id,
                           &EffectAuraColumnPattern::effect_id);
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
  PooledEffectNode pooled = PooledNodeAt(PooledNodeShape::kAuraColumn, anchor);
  pooled.node.life_ticks = effect_life_ticks;
  pooled.resource_paths = {std::string(pattern.texture)};
  pooled.tracks = SeedEffectAuraColumnTracks();
  return pooled;
}

}  // namespace ro::render
