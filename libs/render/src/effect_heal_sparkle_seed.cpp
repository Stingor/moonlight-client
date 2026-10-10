#include "ro/render/effect_heal_sparkle_seed.h"

#include <algorithm>
#include <cstddef>
#include <string>

#include "ro/render/effect_node.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_track.h"

namespace ro::render {
namespace {

// LA MARQUE QUE LA FIGURE LIT, et le client l'écrit dans la troisième marque
// de la piste zéro. Voir `effect_tinted_quad.h` : la figure la lit à un
// déplacement ABSOLU, que sa boucle des quatre pistes ne décale pas.
constexpr std::size_t kMarkOfQuadDrawing = 2;

// L'ANGLE PROPRE de la piste, celui que la figure lit pour faire respirer son
// rayon. Même case que l'arc de ruban et le carré teinté.
constexpr std::size_t kSlotOfSpinDegrees = 8;

[[nodiscard]] std::int32_t DrawBelow(std::uint32_t& rng_state, std::int32_t exclusive_bound) {
  return static_cast<std::int32_t>(NextEffectNodeDraw(rng_state) %
                                   static_cast<std::uint32_t>(exclusive_bound));
}

}  // namespace

std::span<const EffectHealSparklePattern> EffectHealSparklePatternsFor(std::uint32_t effect_id) {
  const std::span<const EffectHealSparklePattern> table = EffectHealSparklePatternTable();
  const auto first =
      std::ranges::lower_bound(table, effect_id, {}, &EffectHealSparklePattern::effect_id);
  const auto last =
      std::ranges::upper_bound(table, effect_id, {}, &EffectHealSparklePattern::effect_id);
  return table.subspan(static_cast<std::size_t>(first - table.begin()),
                       static_cast<std::size_t>(last - first));
}

std::array<EffectTrack, kEffectTracksPerNode> SeedEffectHealSparkleTracks(
    const EffectHealSparklePattern& pattern, std::uint32_t& generator) {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  EffectTrack& track = tracks.front();
  track.active = true;
  // ⚠ ALPHA NUL AU DÉPART : le mouvement l'ouvre de quatre par tic.
  track.alpha = 0;
  track.radius_at_anchor = pattern.radius;
  track.mark[kMarkOfQuadDrawing] = pattern.mark;
  // 🔴 L'ÉLÉVATION AU-DESSUS DU PORTEUR. Voir `kHealSparkleLiftAboveOwner` : le
  // bâtisseur l'écrit en RETRANCHANT douze, dans un repère qui descend. L'ancre
  // sort d'ici en écart ; `SowEffectHealSparkleNode` la pose dans le monde.
  track.anchor = {0.0F, kHealSparkleLiftAboveOwner, 0.0F};
  // 🔴 ET DEUX ANGLES TIRÉS AU SORT, chacun sur le tour entier. Le premier
  // tourne le carré dans son plan ; le second dit où sa respiration en est.
  track.degrees = static_cast<std::int16_t>(DrawBelow(generator, kEffectTrackFullTurnDegrees));
  track.value[kSlotOfSpinDegrees] =
      static_cast<float>(DrawBelow(generator, kEffectTrackFullTurnDegrees));
  return tracks;
}

PooledEffectNode SowEffectHealSparkleNode(const EffectHealSparklePattern& pattern, glm::vec3 anchor,
                                          std::int32_t effect_life_ticks,
                                          std::uint32_t& generator) {
  PooledEffectNode pooled;
  pooled.shape = PooledNodeShape::kHealSparkle;
  pooled.ground_position = anchor;
  pooled.owner_position = anchor;
  pooled.node.life_ticks = effect_life_ticks;
  pooled.resource_paths = {std::string(pattern.texture)};
  pooled.tracks = SeedEffectHealSparkleTracks(pattern, generator);
  // ⚠ L'ANCRE EST LA POSITION DU PORTEUR AU SEMIS, lue dans l'instance, et la
  // figure n'y ajoute rien. L'étincelle ne suit donc pas un porteur qui
  // marche.
  pooled.tracks.front().anchor += anchor;
  return pooled;
}

}  // namespace ro::render
