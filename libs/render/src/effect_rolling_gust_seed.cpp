#include "ro/render/effect_rolling_gust_seed.h"

#include <cstddef>
#include <string>

#include "ro/core/sorted_table.h"
#include "ro/render/effect_ground_gust.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_rolling_gust.h"
#include "ro/render/effect_track.h"

#include "effect_draw_below.h"
#include "effect_track_angles.h"

namespace ro::render {
namespace {

// LES BRANCHES DE LA CROIX, par tic de semis.
constexpr std::int32_t kCrossCenter = 0;
constexpr std::int32_t kCrossEast = 1;
constexpr std::int32_t kCrossWest = 2;
constexpr std::int32_t kCrossClientDeeper = 3;

// LE CAP QUE LE BÂTISSEUR TIRE (`rand() % 360`) et que le premier tic utile
// écrase.
constexpr std::int32_t kDiscardedHeadingDraws = kEffectTrackFullTurnDegrees;

// LES DEUX MARQUES QU'IL POSE : zéro et un. ⚠ NI LE MOUVEMENT
// NI LE DESSINATEUR NE LES LISENT — voir `ComposeRollingGustQuad`.
constexpr std::uint8_t kUnreadColorMark = 1;

}  // namespace

std::span<const EffectRollingGustPattern> EffectRollingGustPatternsFor(std::uint32_t effect_id) {
  return core::RowsWithKey(EffectRollingGustPatternTable(), effect_id,
                           &EffectRollingGustPattern::effect_id);
}

glm::vec3 RollingGustCrossOffset(std::int32_t sow_tick) {
  switch (sow_tick) {
    case kCrossCenter:
      return {0.0F, 0.0F, 0.0F};
    case kCrossEast:
      return {kRollingGustCrossStep, 0.0F, 0.0F};
    case kCrossWest:
      return {-kRollingGustCrossStep, 0.0F, 0.0F};
    case kCrossClientDeeper:
      // ⚠ PLUS CINQ EN PROFONDEUR CHEZ LE CLIENT, MOINS CINQ CHEZ NOUS.
      return {0.0F, 0.0F, -kRollingGustCrossStep};
    default:
      // ⚠ LE DERNIER BRAS EST UN `else` : tout tic au-delà de trois prend
      // l'autre côté de la profondeur. Le bâtisseur n'en voit qu'un, le
      // quatrième.
      return {0.0F, 0.0F, kRollingGustCrossStep};
  }
}

std::array<EffectTrack, kEffectTracksPerNode> SeedEffectRollingGustTracks(
    glm::vec3 anchor, glm::vec3 toward_target, std::int32_t sow_tick, std::uint32_t& generator) {
  const std::int16_t cap =
      static_cast<std::int16_t>(HeadingOfVector(toward_target.x, toward_target.z));
  const glm::vec3 branche = anchor + RollingGustCrossOffset(sow_tick);
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  for (std::size_t rang = 0; rang < tracks.size(); ++rang) {
    EffectTrack& track = tracks[rang];
    track.active = true;
    // ⚠ LE RETARD DU 119, PLUS CELUI DU TIC DE SEMIS : voir l'en-tête. Seul
    // le compteur de la piste zéro est lu ; les trois autres sont posés comme
    // le bâtisseur les pose (rang − 3).
    track.ticks = static_cast<std::int32_t>(rang) - kRollingGustDelayTicks - sow_tick;
    track.alpha = 0;
    track.radius_at_anchor = kRollingGustRadius;
    // ⚠ DEUX TIRAGES PAR PISTE, la hauteur puis le cap. Le second ne se voit
    // jamais — le premier tic utile écrase le cap —, mais il est tiré, et le
    // sauter décalerait tous les suivants.
    const float descent = static_cast<float>(DrawBelow(generator, kRollingGustLiftSpread));
    static_cast<void>(DrawBelow(generator, kDiscardedHeadingDraws));
    track.anchor = {branche.x, anchor.y + kRollingGustLiftAboveNode - descent, branche.z};
    track.azimuth_degrees = cap;
    track.tilt_degrees = static_cast<std::int16_t>(rang);
    track.value[kGroundGustSlotOfSpeed] = kRollingGustStartSpeed;
    track.mark[0] = 0;
    track.mark[kGroundGustMarkOfColor] = kUnreadColorMark;
  }
  return tracks;
}

PooledEffectNode SowEffectRollingGustNode(const EffectRollingGustPattern& pattern, glm::vec3 anchor,
                                          glm::vec3 toward_target, std::int32_t sow_tick,
                                          std::int32_t effect_life_ticks,
                                          std::uint32_t& generator) {
  PooledEffectNode pooled = PooledNodeAt(PooledNodeShape::kRollingGust, anchor);
  pooled.node.life_ticks = effect_life_ticks + sow_tick;
  pooled.resource_paths = {std::string(pattern.texture)};
  pooled.tracks = SeedEffectRollingGustTracks(anchor, toward_target, sow_tick, generator);
  return pooled;
}

}  // namespace ro::render
