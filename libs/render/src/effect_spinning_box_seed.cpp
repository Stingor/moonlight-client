#include "ro/render/effect_spinning_box_seed.h"

#include <algorithm>
#include <string>

#include "ro/render/effect_life_table.h"
#include "ro/render/effect_node.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_spinning_box.h"
#include "ro/render/effect_track.h"

namespace ro::render {
namespace {

// L'UNIQUE PISTE VIVE : le bâtisseur allume la première et éteint les trois
// autres.
constexpr std::size_t kLivingTrack = 0;

[[nodiscard]] std::int32_t DrawBelow(std::uint32_t& generator, std::int32_t bound) {
  return static_cast<std::int32_t>(NextEffectNodeDraw(generator) %
                                   static_cast<std::uint32_t>(bound));
}

}  // namespace

std::span<const EffectSpinningBoxPattern> EffectSpinningBoxPatternsFor(std::uint32_t effect_id) {
  const std::span<const EffectSpinningBoxPattern> table = EffectSpinningBoxPatternTable();
  const auto first = std::ranges::find(table, effect_id, &EffectSpinningBoxPattern::effect_id);
  const auto after = std::find_if(first, table.end(), [effect_id](const auto& node_pattern) {
    return node_pattern.effect_id != effect_id;
  });
  return {first, after};
}

std::array<EffectTrack, kEffectTracksPerNode> SeedEffectSpinningBoxTracks(
    std::uint32_t& generator) {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  EffectTrack& living = tracks[kLivingTrack];
  living.active = true;
  living.radius_at_anchor = kSpinningBoxAnchorRadius;
  // ⚠ L'ORDRE DES TIRAGES EST CELUI DU CLIENT : l'inclinaison, l'azimut, puis
  // la marque. Les intervertir donnerait une autre boîte à la même graine.
  living.tilt_degrees =
      static_cast<std::int16_t>(DrawBelow(generator, kEffectTrackFullTurnDegrees));
  living.azimuth_degrees =
      static_cast<std::int16_t>(DrawBelow(generator, kEffectTrackFullTurnDegrees));
  // L'ALPHA PART DE ZÉRO : le mouvement l'allume.
  living.alpha = 0;
  living.value[kSpinningBoxSlotOfLift] = kSpinningBoxClientLift;
  living.mark[kSpinningBoxMarkOfRedGreen] =
      static_cast<std::uint8_t>(DrawBelow(generator, kSpinningBoxRedGreenDraws));
  return tracks;
}

PooledEffectNode SowEffectSpinningBoxNode(const EffectSpinningBoxPattern& pattern,
                                          glm::vec3 ground_position, std::uint32_t& generator) {
  PooledEffectNode pooled;
  pooled.shape = PooledNodeShape::kSpinningBox;
  pooled.ground_position = ground_position;
  pooled.owner_position = ground_position;
  pooled.resource_paths = {std::string(pattern.texture)};
  // 🔴 LA VIE EST CELLE DE L'EFFET, que le bâtisseur recopie : vingt-neuf mille
  // tics. Un défaut de cent tics éteindrait la boîte au bout de deux secondes.
  pooled.node.life_ticks = EffectNodeLifeFor(pattern.effect_id);
  pooled.node.render_word = kSpinningBoxRenderWord;
  pooled.tracks = SeedEffectSpinningBoxTracks(generator);
  return pooled;
}

}  // namespace ro::render
