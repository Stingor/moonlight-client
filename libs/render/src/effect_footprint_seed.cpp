#include "ro/render/effect_footprint_seed.h"

#include <algorithm>
#include <string>

#include "ro/render/effect_breathing_quad.h"
#include "ro/render/effect_node.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_track.h"
#include "ro/render/effect_track_motion.h"

namespace ro::render {

std::span<const EffectFootprintPattern> EffectFootprintPatternsFor(std::uint32_t effect_id) {
  const std::span<const EffectFootprintPattern> table = EffectFootprintPatternTable();
  const auto first =
      std::ranges::lower_bound(table, effect_id, {}, &EffectFootprintPattern::effect_id);
  const auto last =
      std::ranges::upper_bound(table, effect_id, {}, &EffectFootprintPattern::effect_id);
  return table.subspan(static_cast<std::size_t>(first - table.begin()),
                       static_cast<std::size_t>(last - first));
}

std::array<EffectTrack, kEffectTracksPerNode> SeedEffectFootprintTracks(
    const EffectFootprintPattern& pattern, std::int16_t azimuth_degrees) {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  // ⚠ UNE SEULE PISTE VIVE : le bâtisseur allume la première et éteint les trois
  // autres.
  EffectTrack& track = tracks.front();
  track.active = true;
  track.ticks = 0;
  track.alpha = pattern.alpha;
  track.radius_at_anchor = pattern.radius_at_anchor;
  track.azimuth_degrees = azimuth_degrees;
  // ⚠ L'INCLINAISON EST NULLE ET LE RESTE : sans souffle, le carré garde neuf
  // dixièmes de son rayon d'ancre.
  track.tilt_degrees = 0;
  track.value[kSlotOfBreathingHeight] = kFootprintClientHeight;
  return tracks;
}

PooledEffectNode SowEffectFootprintNode(const EffectFootprintPattern& pattern, glm::vec3 anchor,
                                        std::int16_t azimuth_degrees,
                                        std::int32_t effect_life_ticks) {
  PooledEffectNode pooled;
  pooled.shape = PooledNodeShape::kFootprint;
  pooled.ground_position = anchor;
  pooled.owner_position = anchor;
  pooled.node.life_ticks = effect_life_ticks;
  pooled.resource_paths = {std::string(pattern.texture)};
  pooled.tracks = SeedEffectFootprintTracks(pattern, azimuth_degrees);
  pooled.trail_variant = static_cast<std::uint8_t>(pattern.node_style);
  // Le mot de rendu que le bâtisseur écrit : voir `effect_render_word.h`.
  pooled.node.render_word = kFootprintRenderWord;
  return pooled;
}

void AdvanceFootprintTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks) {
  // Le mouvement ne touche que la première piste, la seule que le bâtisseur
  // allume.
  EffectTrack& track = tracks.front();
  ++track.ticks;
  if (track.ticks <= 0) {
    return;
  }
  if (track.ticks % 2 == 1) {
    --track.alpha;
  }
  track.alpha = std::max<std::int16_t>(track.alpha, 0);
}

}  // namespace ro::render
