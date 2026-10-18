#include "ro/render/effect_frame_and_bar_seed.h"

#include <algorithm>
#include <cstddef>
#include <string>

#include "ro/core/sorted_table.h"
#include "ro/render/effect_frame_and_bar.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_track.h"

namespace ro::render {
namespace {

// LES TROIS OCTETS QUI MONTENT HAUT : zéro, dix et vingt. Les autres bâtissent
// l'étage bas, et c'est le premier partage du bâtisseur.
constexpr std::array<std::uint8_t, 3> kTallStyles = {0, 10, 20};
// LES DEUX OCTETS QUI PARTENT DU COIN et les deux qui partent du milieu.
constexpr std::uint8_t kSquatStyle = 10;
constexpr std::uint8_t kSquatWatchedStyle = 20;
constexpr std::uint8_t kOffsetStyle = 11;
constexpr std::uint8_t kOffsetWatchedStyle = 21;
constexpr std::uint8_t kQuietStyle = 0;

// LES DEMI-CÔTÉS DE CADRE, un par famille d'octets.
constexpr float kQuietFrameHalfSide = 12.5F;
constexpr float kSquatFrameHalfSide = 7.0F;
constexpr float kOffsetFrameHalfSide = 8.5F;
constexpr float kPlainFrameHalfSide = 14.0F;
// LES DEMI-CÔTÉS DE BARRE, toujours un peu plus larges que leur cadre : c'est
// pour cela que la barre court À L'EXTÉRIEUR des murs.
constexpr float kQuietBarHalfSide = 13.3F;
constexpr float kSquatBarHalfSide = 7.8F;
constexpr float kOffsetBarHalfSide = 9.3F;
constexpr float kPlainBarHalfSide = 14.8F;
// LES DEUX ÉCARTS DE DÉPART entre les deux barres d'un nœud, et le décalage
// que deux octets leur donnent en plus.
constexpr float kWideStartStep = 53.2F;
constexpr float kNarrowStartStep = 30.2F;
constexpr float kNarrowStartFloor = 15.1F;
constexpr float kWideStartFloor = 26.6F;
// LES DEUX LONGUEURS DE BARRE.
constexpr float kWideBarLength = 26.6F;
constexpr float kNarrowBarLength = 18.6F;

// LA DIZAINE, celle dont la durée vient de la vie de l'effet.
constexpr std::uint8_t kFirstLifeStyle = 10;
constexpr std::uint8_t kLastLifeStyle = 19;

[[nodiscard]] bool IsTop(std::uint8_t octet) {
  return std::ranges::find(kTallStyles, octet) != std::ranges::end(kTallStyles);
}

}  // namespace

EffectFrameAndBarRecipe EffectFrameAndBarRecipeOf(std::uint8_t style_byte) {
  const bool top = IsTop(style_byte);
  const bool ramasse = style_byte == kSquatStyle || style_byte == kSquatWatchedStyle;
  const bool shifted = style_byte == kOffsetStyle || style_byte == kOffsetWatchedStyle;

  EffectFrameAndBarRecipe recipe;
  recipe.lift_floor = top ? kFrameAndBarTallLift : kFrameAndBarShortLift;
  recipe.frame_alpha = top ? kFrameAndBarTallAlpha : kFrameAndBarShortAlpha;
  recipe.duration_from_life = style_byte >= kFirstLifeStyle && style_byte <= kLastLifeStyle;

  if (style_byte == kSquatWatchedStyle) {
    recipe.frame_duration = kFrameAndBarWatchedDuration;
  } else if (style_byte == kOffsetWatchedStyle) {
    recipe.frame_duration = kFrameAndBarShortDuration;
  } else if (top) {
    recipe.frame_duration = kFrameAndBarLongDuration;
  } else {
    recipe.frame_duration = kFrameAndBarPlainDuration;
  }
  // ⚠ LA BARRE NE PARTAGE PAS LA DURÉE DU CADRE : la sienne ne connaît qu'un
  // seuil, celui de vingt, là où le cadre en a quatre. Les octets UN et VINGT
  // ET UN ont donc un cadre et une barre qui ne s'éteignent pas ensemble ;
  // leur donner la même durée avancerait la barre de trente tics.
  recipe.bar_duration =
      style_byte < kSquatWatchedStyle ? kFrameAndBarLongDuration : kFrameAndBarWatchedDuration;

  if (ramasse) {
    recipe.frame_half_side = kSquatFrameHalfSide;
  } else if (shifted) {
    recipe.frame_half_side = kOffsetFrameHalfSide;
  } else if (style_byte == kQuietStyle) {
    recipe.frame_half_side = kQuietFrameHalfSide;
  } else {
    recipe.frame_half_side = kPlainFrameHalfSide;
  }

  if (ramasse) {
    recipe.bar_half_side = kSquatBarHalfSide;
    recipe.bar_start_step = kNarrowStartStep;
    recipe.bar_start_floor = 0.0F;
  } else if (shifted) {
    recipe.bar_half_side = kOffsetBarHalfSide;
    recipe.bar_start_step = kNarrowStartStep;
    recipe.bar_start_floor = kNarrowStartFloor;
  } else if (style_byte == kQuietStyle) {
    recipe.bar_half_side = kQuietBarHalfSide;
    recipe.bar_start_step = kWideStartStep;
    recipe.bar_start_floor = 0.0F;
  } else {
    recipe.bar_half_side = kPlainBarHalfSide;
    recipe.bar_start_step = kWideStartStep;
    recipe.bar_start_floor = kWideStartFloor;
  }

  recipe.bar_length = style_byte < kFirstLifeStyle ? kWideBarLength : kNarrowBarLength;
  return recipe;
}

std::span<const EffectFrameAndBarPattern> EffectFrameAndBarPatternsFor(std::uint32_t effect_id) {
  return core::RowsWithKey(EffectFrameAndBarPatternTable(), effect_id,
                           &EffectFrameAndBarPattern::effect_id);
}

std::array<EffectTrack, kEffectTracksPerNode> SeedEffectFrameAndBarTracks(
    const EffectFrameAndBarPattern& pattern, std::int32_t effect_life_ticks) {
  const EffectFrameAndBarRecipe recipe = EffectFrameAndBarRecipeOf(pattern.style_byte);
  // ⚠ LE CLIENT BORNE LA VIE AVANT D'EN RETRANCHER SA MARGE, et non après : une
  // vie de trente-et-un mille tics donne trente mille, pas trente mille neuf
  // cent dix.
  const std::int32_t from_life_ticks = effect_life_ticks <= kFrameAndBarLifeCeiling
                                           ? effect_life_ticks - kFrameAndBarLifeMargin
                                           : kFrameAndBarLifeCeiling;

  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  for (std::size_t rang = 0; rang < tracks.size(); ++rang) {
    EffectTrack& track = tracks[rang];
    const bool frame = rang < kFrameAndBarFrameTracks;
    track.active = true;
    track.ticks = 0;
    track.lift_key = static_cast<float>(rang) * kFrameAndBarRankHeightStep + recipe.lift_floor;
    track.alpha = frame ? recipe.frame_alpha : kFrameAndBarBarAlpha;
    track.style = static_cast<std::int16_t>(
        recipe.duration_from_life ? from_life_ticks
                                  : (frame ? recipe.frame_duration : recipe.bar_duration));
    if (frame) {
      track.value[kFrameAndBarSlotOfHalfX] =
          static_cast<float>(rang) * kFrameAndBarRankSideStep + recipe.frame_half_side;
    } else {
      track.value[kFrameAndBarSlotOfHalfX] = recipe.bar_half_side;
      track.radius_at_anchor =
          static_cast<float>(rang - kFrameAndBarFrameTracks) * recipe.bar_start_step +
          recipe.bar_start_floor;
      track.value[kFrameAndBarSlotOfBarLength] = recipe.bar_length;
    }
    // ⚠ LE CLIENT RECOPIE LE DEMI-CÔTÉ dans la case suivante, et sa barre ne
    // relit que la PREMIÈRE pour mesurer son tour. Un cadre rectangulaire
    // replierait donc sa barre au mauvais endroit — mais aucun octet n'en pose.
    track.value[kFrameAndBarSlotOfHalfZ] = track.value[kFrameAndBarSlotOfHalfX];
    track.value[kFrameAndBarSlotOfStyle] = static_cast<float>(pattern.style_byte);
  }
  return tracks;
}

PooledEffectNode SowEffectFrameAndBarNode(const EffectFrameAndBarPattern& pattern, glm::vec3 anchor,
                                          std::int32_t effect_life_ticks) {
  PooledEffectNode pooled = PooledNodeAt(PooledNodeShape::kFrameAndBar, anchor);
  pooled.node.life_ticks = effect_life_ticks;
  pooled.resource_paths = {std::string(pattern.texture)};
  pooled.tracks = SeedEffectFrameAndBarTracks(pattern, effect_life_ticks);
  return pooled;
}

}  // namespace ro::render
