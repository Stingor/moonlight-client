#include "ro/render/effect_fading_streak_seed.h"

#include <algorithm>
#include <string>

#include "ro/render/effect_fading_streak.h"
#include "ro/render/effect_node.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_track.h"

#include "effect_track_angles.h"

namespace ro::render {

std::span<const EffectFadingStreakPattern> EffectFadingStreakPatternsFor(std::uint32_t effect_id) {
  const std::span<const EffectFadingStreakPattern> table = EffectFadingStreakPatternTable();
  const auto first =
      std::ranges::lower_bound(table, effect_id, {}, &EffectFadingStreakPattern::effect_id);
  const auto last =
      std::ranges::upper_bound(table, effect_id, {}, &EffectFadingStreakPattern::effect_id);
  return table.subspan(static_cast<std::size_t>(first - table.begin()),
                       static_cast<std::size_t>(last - first));
}

std::array<EffectTrack, kEffectTracksPerNode> SeedEffectFadingStreakTracks(
    glm::vec3 source, glm::vec3 target, std::uint32_t& generator) {
  // ⚠ LE CAP SE MESURE DE LA CIBLE VERS LA SOURCE, et non l'inverse : c'est
  // ainsi que le client l'écrit, et la dispersion de cent soixante à deux cents
  // degrés le retourne ensuite. Mesurer dans l'autre sens ferait partir les
  // flocons à l'opposé de leur cible.
  const std::int32_t vers_la_source = HeadingOfVector(source.x - target.x, source.z - target.z);

  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  for (EffectTrack& track : tracks) {
    track.active = true;
    track.ticks = 0;
    // ⚠ L'ALPHA PART À ZÉRO : c'est le mouvement qui allume le flocon en neuf
    // tics.
    track.alpha = 0;
    track.radius_at_anchor =
        kFadingStreakRadiusFloor +
        static_cast<float>(NextEffectNodeDraw(generator) % kFadingStreakRadiusSpread) *
            kFadingStreakRadiusStep;
    track.mark[kMarkOfFadingStreakTint] =
        static_cast<std::uint8_t>(NextEffectNodeDraw(generator) % kFadingStreakTintSpread);
    track.degrees = static_cast<std::int16_t>(
        NextEffectNodeDraw(generator) % static_cast<std::uint32_t>(kEffectTrackFullTurnDegrees));
    // L'ANCRE EST LA CIBLE, et sa hauteur descend de douze — que personne ne lit.
    track.anchor = {target.x, target.y + kFadingStreakAnchorDrop, target.z};
    track.tail[4] = track.anchor.y;
    track.tilt_degrees = static_cast<std::int16_t>(
        vers_la_source +
        static_cast<std::int32_t>(kFadingStreakHeadingFloor +
                                  NextEffectNodeDraw(generator) % kFadingStreakHeadingSpread));
    // ⚠ UNE SEULE SOUSTRACTION, ET PAS DE MODULO : le client retranche un tour
    // quand le cap l'atteint, une fois.
    DropOneTurn(track.tilt_degrees);
    track.azimuth_degrees = static_cast<std::int16_t>(
        NextEffectNodeDraw(generator) % static_cast<std::uint32_t>(kEffectTrackFullTurnDegrees));
    track.mark[kMarkOfFadingStreakImage] = kFadingStreakImage;
    track.mark[kMarkOfFadingStreakSpeedKey] = static_cast<std::uint8_t>(
        kFadingStreakSpeedKeyCeiling - NextEffectNodeDraw(generator) % kFadingStreakSpeedKeySpread);
    // ⚠ LES DEUX CASES DE FORME ET DE RESPIRATION RESTENT NULLES, et le
    // bâtisseur les écrit nommément : le quatre-coins est donc un CARRÉ, et le
    // rayon ne respire pas. Les laisser au hasard de la piste ferait un losange
    // un nœud sur trois.
    track.value[kSlotOfFadingStreakShape] = 0.0F;
    track.value[kSlotOfFadingStreakBreath] = 0.0F;
  }
  return tracks;
}

PooledEffectNode SowEffectFadingStreakNode(const EffectFadingStreakPattern& pattern,
                                           glm::vec3 source, glm::vec3 target,
                                           std::int32_t effect_life_ticks,
                                           std::uint32_t& generator) {
  PooledEffectNode pooled;
  pooled.shape = PooledNodeShape::kFadingStreak;
  pooled.ground_position = source;
  pooled.owner_position = source;
  pooled.target_position = target;
  pooled.node.life_ticks = effect_life_ticks;
  pooled.resource_paths.clear();
  for (const std::string_view path : pattern.textures) {
    pooled.resource_paths.emplace_back(path);
  }
  pooled.tracks = SeedEffectFadingStreakTracks(source, target, generator);
  return pooled;
}

}  // namespace ro::render
