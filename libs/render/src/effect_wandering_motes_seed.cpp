#include "ro/render/effect_wandering_motes_seed.h"

#include <algorithm>
#include <cstddef>
#include <string>

#include "ro/render/effect_four_particle_slots.h"
#include "ro/render/effect_node.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_track.h"
#include "ro/render/effect_wandering_motes.h"

namespace ro::render {
namespace {

[[nodiscard]] std::int32_t DrawBelow(std::uint32_t& rng_state, std::int32_t exclusive_bound) {
  return static_cast<std::int32_t>(NextEffectNodeDraw(rng_state) %
                                   static_cast<std::uint32_t>(exclusive_bound));
}

}  // namespace

std::span<const EffectWanderingMotePattern> EffectWanderingMotePatternsFor(
    std::uint32_t effect_id) {
  const std::span<const EffectWanderingMotePattern> table = EffectWanderingMotePatternTable();
  const auto first =
      std::ranges::lower_bound(table, effect_id, {}, &EffectWanderingMotePattern::effect_id);
  const auto last =
      std::ranges::upper_bound(table, effect_id, {}, &EffectWanderingMotePattern::effect_id);
  return table.subspan(static_cast<std::size_t>(first - table.begin()),
                       static_cast<std::size_t>(last - first));
}

std::array<EffectTrack, kEffectTracksPerNode> SeedEffectWanderingMoteTracks(
    const EffectWanderingMotePattern& pattern, std::uint32_t& generator) {
  const bool solitaire = pattern.style_byte == kWanderingMoteLoneStyle;
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  for (std::size_t rang = 0; rang < tracks.size(); ++rang) {
    EffectTrack& track = tracks[rang];
    // ⚠ L'OCTET UN N'ALLUME QUE LA PREMIÈRE PISTE, et les trois autres tirent
    // quand même.
    track.active = !(solitaire && rang >= 1);
    track.lift_key = kWanderingMoteUnusedKey;
    track.radius_at_anchor = kWanderingMoteSquareRadius;
    for (std::size_t firefly = 0; firefly < kWanderingMoteParticles; ++firefly) {
      glm::vec3 start{0.0F, 0.0F, 0.0F};
      if (!solitaire) {
        start.x = static_cast<float>(DrawBelow(generator, kWanderingMoteSpawnSpread) +
                                     kWanderingMoteSpawnFloor);
        start.z = static_cast<float>(DrawBelow(generator, kWanderingMoteSpawnSpread) +
                                     kWanderingMoteSpawnFloor);
      }
      // ⚠ CE TIRAGE-CI EST JETÉ : le bâtisseur range une hauteur puis remet les
      // quatre à zéro avant de rendre la main. Le sauter décalerait la suite.
      static_cast<void>(DrawBelow(generator, kWanderingMoteDiscardedSpread));
      SetEffectFourParticleOffset(track, firefly, start);
      for (std::size_t angle = 0; angle < 4; ++angle) {
        track.value[4 * firefly + angle] =
            static_cast<float>(DrawBelow(generator, kEffectTrackFullTurnDegrees));
      }
      track.mark[kWanderingMoteMarkOfFirstGlow + firefly] = 0;
      track.mark[kWanderingMoteMarkOfFirstTwinkle + firefly] = 0;
    }
    track.mark[kWanderingMoteMarkOfStyle] = pattern.style_byte;
    // ⚠ L'OCTET DEUX PEINT EN BLANC, les deux autres en jaune. La teinte rose et
    // la verte du dessinateur n'ont aucun site.
    track.mark[kWanderingMoteMarkOfColor] = pattern.style_byte == kWanderingMoteWhiteStyle
                                                ? kWanderingMoteWhiteColor
                                                : kWanderingMoteYellowColor;
  }
  return tracks;
}

PooledEffectNode SowEffectWanderingMoteNode(const EffectWanderingMotePattern& pattern,
                                            glm::vec3 anchor, std::int32_t effect_life_ticks,
                                            std::uint32_t& generator) {
  PooledEffectNode pooled;
  pooled.shape = PooledNodeShape::kWanderingMote;
  pooled.ground_position = anchor;
  pooled.owner_position = anchor;
  pooled.node.life_ticks = effect_life_ticks;
  pooled.resource_paths = {std::string(pattern.texture)};
  pooled.tracks = SeedEffectWanderingMoteTracks(pattern, generator);
  return pooled;
}

}  // namespace ro::render
