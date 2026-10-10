#include "ro/render/effect_rainstorm_cylinder_seed.h"

#include <algorithm>
#include <string>

#include "ro/render/effect_node.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_track.h"

namespace ro::render {

std::span<const EffectRainstormCylinderPattern> EffectRainstormCylinderPatternsFor(
    std::uint32_t effect_id) {
  const std::span<const EffectRainstormCylinderPattern> table =
      EffectRainstormCylinderPatternTable();
  const auto first =
      std::ranges::lower_bound(table, effect_id, {}, &EffectRainstormCylinderPattern::effect_id);
  const auto last =
      std::ranges::upper_bound(table, effect_id, {}, &EffectRainstormCylinderPattern::effect_id);
  return table.subspan(static_cast<std::size_t>(first - table.begin()),
                       static_cast<std::size_t>(last - first));
}

std::int32_t RainstormCylinderLifeTicks(const EffectRainstormCylinderPattern& pattern,
                                        std::int32_t effect_life_ticks) {
  return std::max(effect_life_ticks, pattern.instance_life_floor_ticks);
}

std::array<EffectTrack, kEffectTracksPerNode> SeedRainstormCylinderTracks(
    const EffectRainstormCylinderPattern& pattern) {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  for (std::size_t rang = 0; rang < tracks.size(); ++rang) {
    EffectTrack& track = tracks[rang];
    track.active = true;
    track.ticks = 0;
    track.degrees = kRainstormCylinderSweepDegrees;
    track.lift_key = kRainstormCylinderLiftKey;
    track.azimuth_degrees = kRainstormCylinderAzimuths[rang];
    track.alpha = 0;
    track.radius_at_anchor = pattern.radius;
    track.tilt_degrees = kRainstormCylinderTilts[rang];
    // L'OCTET DU SITE, rangé au style par la boucle de fin. Ni le
    // mouvement ni le dessinateur ne le lisent ; on le porte pour que la piste
    // soit celle du client.
    track.style = static_cast<std::int16_t>(pattern.style);
  }
  return tracks;
}

PooledEffectNode SowRainstormCylinderNode(const EffectRainstormCylinderPattern& pattern,
                                          glm::vec3 owner_position,
                                          std::int32_t effect_life_ticks) {
  PooledEffectNode pooled;
  pooled.shape = PooledNodeShape::kRainstormCylinder;
  // ⚠ LE NŒUD NAÎT SUR L'INSTANCE, sans écart, et le mouvement l'y recale à
  // chaque tic. Voir `AdvanceTracksOf`.
  pooled.ground_position = owner_position;
  pooled.owner_position = owner_position;
  pooled.node.life_ticks = std::max(RainstormCylinderLifeTicks(pattern, effect_life_ticks), 1);
  pooled.node.render_word = kRainstormCylinderRenderWord;
  // ⚠ L'IMAGE NE CHANGE PAS D'ELLE-MÊME : c'est le compteur des pistes qui la
  // choisit, et le mouvement le recopie dans `frame`. Laisser la retenue par
  // défaut ferait défiler une seconde fois, quatre fois moins vite.
  pooled.node.frame_hold_ticks = 0;
  pooled.node.texture_count = kRainstormCylinderImageCount;
  for (const std::string_view image : RainstormCylinderImages()) {
    pooled.resource_paths.emplace_back(image);
  }
  pooled.trail_variant = kRainstormCylinderTrailVariant;
  pooled.tracks = SeedRainstormCylinderTracks(pattern);
  return pooled;
}

}  // namespace ro::render
