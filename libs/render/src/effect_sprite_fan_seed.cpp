#include "ro/render/effect_sprite_fan_seed.h"

#include <cstddef>
#include <string>

#include "ro/core/sorted_table.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_sprite_fan.h"
#include "ro/render/effect_track.h"

#include "effect_track_angles.h"

namespace ro::render {

std::span<const EffectSpriteFanPattern> EffectSpriteFanPatternsFor(std::uint32_t effect_id) {
  return core::RowsWithKey(EffectSpriteFanPatternTable(), effect_id,
                           &EffectSpriteFanPattern::effect_id);
}

std::array<EffectTrack, kEffectTracksPerNode> SeedEffectSpriteFanTracks(
    const EffectSpriteFanPattern& pattern, std::int32_t effect_age_ticks) {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  for (std::size_t rang = 0; rang < tracks.size(); ++rang) {
    EffectTrack& track = tracks[rang];
    track.active = true;
    // ⚠ L'ÂGE DE L'EFFET ENTRE DANS L'AZIMUT, et c'est ce qui sépare les deux
    // éventails : quarante-cinq fois quatre font cent quatre-vingts, si bien
    // que le second tombe entre les branches du premier.
    const auto tour = static_cast<std::int16_t>(
        kSpriteFanDegreesPerTrack *
        (static_cast<std::int16_t>(rang) +
         kSpriteFanTracksPerTurn * static_cast<std::int16_t>(effect_age_ticks)));
    track.azimuth_degrees = tour;
    track.value[kSpriteFanValueOfSpread] = kSpriteFanSpreadStep;
    // ⚠ LE CLIENT POSE L'ANCRE À ZÉRO PUIS Y AJOUTE le point du cercle, au lieu
    // de l'y écrire : c'est la même valeur, et le transcrire en une affectation
    // ne change rien — sauf si un jour un bâtisseur posait l'ancre avant.
    track.anchor = {CosOfDegrees(tour) * kSpriteFanSpawnRadius, kSpriteFanSpawnHeight,
                    SinOfDegrees(tour) * kSpriteFanSpawnRadius};
    track.mark[kSpriteFanMarkOfColor] = pattern.color_mark;
  }
  return tracks;
}

PooledEffectNode SowEffectSpriteFanNode(const EffectSpriteFanPattern& pattern, glm::vec3 anchor,
                                        std::int32_t effect_life_ticks,
                                        std::int32_t effect_age_ticks) {
  PooledEffectNode pooled = PooledNodeAt(PooledNodeShape::kSpriteFan, anchor);
  pooled.node.life_ticks = effect_life_ticks;
  // L'ANIMATION JOUE UNE FOIS, une image tous les quatre tics, et sa dernière
  // image tue le nœud. Le compte d'images vient du `.act` : c'est l'appelant
  // qui le pose.
  pooled.node.frame_hold_ticks = kSpriteFanFrameHoldTicks;
  pooled.node.loops_frames = false;
  pooled.resource_paths = {std::string(pattern.sprite_base)};
  pooled.tracks = SeedEffectSpriteFanTracks(pattern, effect_age_ticks);
  return pooled;
}

}  // namespace ro::render
