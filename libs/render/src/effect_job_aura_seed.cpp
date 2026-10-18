#include "ro/render/effect_job_aura_seed.h"

#include <cstddef>
#include <string>

#include "ro/core/sorted_table.h"
#include "ro/render/effect_four_particle_slots.h"
#include "ro/render/effect_job_aura.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_track.h"

#include "effect_draw_below.h"

namespace ro::render {

std::span<const EffectJobAuraPattern> EffectJobAuraPatternsFor(std::uint32_t effect_id) {
  return core::RowsWithKey(EffectJobAuraPatternTable(), effect_id,
                           &EffectJobAuraPattern::effect_id);
}

std::array<EffectTrack, kEffectTracksPerNode> SeedEffectJobAuraTracks(
    const EffectJobAuraPattern& pattern, std::uint32_t& generator) {
  const std::array<float, kJobAuraParticles>& alphas =
      pattern.layered ? kLayeredAuraStartAlphas : kPlainAuraStartAlphas;
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  for (std::size_t rang = 0; rang < tracks.size(); ++rang) {
    EffectTrack& track = tracks[rang];
    if (rang >= pattern.live_tracks) {
      // ⚠ ET ELLE NE TIRE RIEN. Le bâtisseur ne boucle que sur les pistes qu'il
      // allume ; les autres gardent leurs zéros.
      continue;
    }
    track.active = true;
    track.radius_at_anchor = pattern.layered ? kLayeredAuraRadius : kPlainAuraRadius;
    for (std::size_t particle = 0; particle < kJobAuraParticles; ++particle) {
      // L'ORDRE DES TIRAGES EST : la hauteur, l'image, puis l'angle. Les
      // mélanger décalerait toute la suite du générateur.
      glm::vec3 place{0.0F, 0.0F, 0.0F};
      place.y = static_cast<float>(-DrawBelow(generator, kJobAuraSpawnHeightSpread));
      SetEffectFourParticleOffset(track, particle, place);
      const std::size_t base = kJobAuraValuesPerParticle * particle;
      track.value[base + kJobAuraValueOfPhase] = kJobAuraStartPhase;
      track.value[base + kJobAuraValueOfAlpha] = alphas[particle];
      track.value[base + kJobAuraValueOfFrame] =
          static_cast<float>(DrawBelow(generator, kJobAuraFrameCount));
      track.value[base + kJobAuraValueOfAngle] =
          static_cast<float>(DrawBelow(generator, kEffectTrackFullTurnDegrees));
    }
    track.mark[kJobAuraMarkOfPalette] = pattern.palette_mark;
  }
  return tracks;
}

PooledEffectNode SowEffectJobAuraNode(const EffectJobAuraPattern& pattern, glm::vec3 anchor,
                                      std::int32_t effect_life_ticks, std::uint32_t& generator) {
  PooledEffectNode pooled = PooledNodeAt(
      pattern.layered ? PooledNodeShape::kLayeredAura : PooledNodeShape::kPlainAura, anchor);
  pooled.node.life_ticks = effect_life_ticks;
  pooled.resource_paths.reserve(pattern.textures.size());
  for (const std::string_view image : pattern.textures) {
    pooled.resource_paths.emplace_back(image);
  }
  pooled.tracks = SeedEffectJobAuraTracks(pattern, generator);
  return pooled;
}

}  // namespace ro::render
