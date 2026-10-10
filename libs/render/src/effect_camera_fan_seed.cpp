#include "ro/render/effect_camera_fan_seed.h"

#include <algorithm>
#include <string>

#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_track.h"

namespace ro::render {

std::span<const EffectCameraFanPattern> EffectCameraFanPatternsFor(std::uint32_t effect_id) {
  const std::span<const EffectCameraFanPattern> table = EffectCameraFanPatternTable();
  const auto first =
      std::ranges::lower_bound(table, effect_id, {}, &EffectCameraFanPattern::effect_id);
  const auto last =
      std::ranges::upper_bound(table, effect_id, {}, &EffectCameraFanPattern::effect_id);
  return table.subspan(static_cast<std::size_t>(first - table.begin()),
                       static_cast<std::size_t>(last - first));
}

std::array<EffectTrack, kEffectTracksPerNode> SeedEffectCameraFanTracks(
    const EffectCameraFanPattern& pattern) {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  // ⚠ LES PISTES AU-DELÀ DU COMPTE RESTENT ÉTEINTES, et le client les éteint
  // explicitement : il passe les quatre à zéro, puis ne rallume que les siennes.
  for (EffectTrack& track : tracks) {
    track.active = false;
  }
  const std::size_t allumees = std::min(pattern.trail_count, kCameraFanMostTrailsPerNode);
  for (std::size_t trail = 0; trail < allumees; ++trail) {
    EffectTrack& track = tracks[trail];
    const auto rang = static_cast<std::int32_t>(trail);
    track.active = true;
    // ⚠ L'ALPHA ET LES DIX LAMES NAISSENT NULS : l'éventail s'ouvre lame par
    // lame sous le mouvement, et le semer brillant lui retirerait son ouverture.
    track.alpha = 0;
    track.ticks = pattern.seed.first_delay + pattern.seed.delay_step * rang;
    track.value[kCameraFanSlotOfSideways] =
        pattern.seed.first_sideways + pattern.seed.sideways_step * static_cast<float>(rang);
    track.radius_at_anchor = pattern.seed.radius_at_anchor;
    // ⚠ LA MARQUE ZÉRO EST UN INDEX D'IMAGE, et c'est le RANG de la traînée : le
    // client en charge une par traînée et les distribue dans l'ordre.
    track.mark[kCameraFanMarkOfTexture] = static_cast<std::uint8_t>(trail);
    track.mark[kCameraFanMarkOfStyle] = pattern.seed.draw_mark;
  }
  return tracks;
}

PooledEffectNode SowEffectCameraFanNode(const EffectCameraFanPattern& pattern, glm::vec3 anchor,
                                        std::int32_t effect_life_ticks) {
  PooledEffectNode pooled;
  pooled.shape = PooledNodeShape::kCameraFan;
  pooled.ground_position = anchor;
  pooled.owner_position = anchor;
  pooled.node.life_ticks = effect_life_ticks;
  const std::size_t allumees = std::min(pattern.trail_count, kCameraFanMostTrailsPerNode);
  pooled.resource_paths.reserve(allumees);
  for (std::size_t image = 0; image < allumees; ++image) {
    pooled.resource_paths.emplace_back(pattern.textures[image]);
  }
  pooled.tracks = SeedEffectCameraFanTracks(pattern);
  return pooled;
}

}  // namespace ro::render
