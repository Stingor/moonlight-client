#include "ro/render/effect_spiral_stack_seed.h"

#include <algorithm>
#include <string>

#include "ro/render/effect_node.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_track.h"

namespace ro::render {
namespace {

// LE GABARIT ORDINAIRE, que six styles sur sept emploient.
constexpr float kRadius = 40.0F;
constexpr float kTop = 50.0F;
constexpr float kRise = 5.0F;
constexpr float kShrink = 2.0F;
constexpr std::uint8_t kSteps = 10;

// ⚠ ET CELUI DU STYLE DIX, QUI REFAIT TOUT. Quatorze étages au lieu de dix, un
// rayon plus petit mais un sommet plus haut, et un rétrécissement NUL : ses
// étages gardent tous le même rayon, si bien qu'il monte en colonne là où les
// autres se resserrent en cône.
constexpr std::uint8_t kStyleWithItsOwnShape = 10;
constexpr float kTallRadius = 14.0F;
constexpr float kTallTop = 70.0F;
constexpr float kNoShrink = 0.0F;
constexpr std::uint8_t kTallSteps = 14;

// LE RYTHME, que le mouvement lit dans la sixième marque : combien de tics la
// pile tient avant de pâlir. Trois valeurs, et elles ne suivent aucune règle
// qu'on puisse déduire du style — elles se relèvent une à une.
constexpr std::size_t kMarkOfSpiralRhythm = 5;
constexpr std::uint8_t kRhythmOrdinary = 0;
constexpr std::uint8_t kRhythmLate = 1;
constexpr std::uint8_t kRhythmNeverFades = 10;
constexpr std::uint8_t kStyleThatNeverFades = 2;

// L'alpha de départ : nul. C'est le mouvement qui allume.
constexpr std::int16_t kStartsInvisible = 0;

}  // namespace

std::span<const EffectSpiralStackPattern> EffectSpiralStackPatternsFor(std::uint32_t effect_id) {
  const std::span<const EffectSpiralStackPattern> table = EffectSpiralStackPatternTable();
  const auto first =
      std::ranges::lower_bound(table, effect_id, {}, &EffectSpiralStackPattern::effect_id);
  const auto last =
      std::ranges::upper_bound(table, effect_id, {}, &EffectSpiralStackPattern::effect_id);
  return table.subspan(static_cast<std::size_t>(first - table.begin()),
                       static_cast<std::size_t>(last - first));
}

std::array<EffectTrack, kEffectTracksPerNode> SeedEffectSpiralStackTracks(
    const EffectSpiralStackPattern& pattern, std::uint32_t& generator) {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  // ⚠ UNE SEULE PISTE VIVE : le bâtisseur éteint les quatre, puis rallume la
  // première. Les allumer toutes montrerait quatre piles au même endroit.
  EffectTrack& track = tracks.front();
  track.active = true;
  track.ticks = 0;
  track.alpha = kStartsInvisible;
  track.azimuth_degrees = static_cast<std::int16_t>(
      NextEffectNodeDraw(generator) % static_cast<std::uint32_t>(kEffectTrackFullTurnDegrees));
  track.value[kSlotOfSpiralRise] = kRise;
  track.mark[kMarkOfSpiralStyle] = pattern.style;

  if (pattern.style == kStyleWithItsOwnShape) {
    track.lift_key = kTallRadius;
    track.value[kSlotOfSpiralTop] = kTallTop;
    track.radius_at_anchor = kNoShrink;
    track.mark[kMarkOfSpiralSteps] = kTallSteps;
    track.mark[kMarkOfSpiralRhythm] = kRhythmLate;
    return tracks;
  }

  track.lift_key = kRadius;
  track.value[kSlotOfSpiralTop] = kTop;
  track.radius_at_anchor = kShrink;
  track.mark[kMarkOfSpiralSteps] = kSteps;
  track.mark[kMarkOfSpiralRhythm] =
      pattern.style == kStyleThatNeverFades ? kRhythmNeverFades : kRhythmOrdinary;
  return tracks;
}

PooledEffectNode SowEffectSpiralStackNode(const EffectSpiralStackPattern& pattern, glm::vec3 anchor,
                                          std::int32_t effect_life_ticks,
                                          std::uint32_t& generator) {
  PooledEffectNode pooled;
  pooled.shape = PooledNodeShape::kSpiralStack;
  pooled.ground_position = anchor;
  pooled.owner_position = anchor;
  pooled.node.life_ticks = effect_life_ticks;
  pooled.resource_paths = {std::string(pattern.texture)};
  pooled.tracks = SeedEffectSpiralStackTracks(pattern, generator);
  return pooled;
}

}  // namespace ro::render
