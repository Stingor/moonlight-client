#include "ro/render/effect_ground_box_seed.h"

#include <algorithm>
#include <cstddef>
#include <string>

#include "ro/render/effect_ground_box.h"
#include "ro/render/effect_node.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_track.h"

namespace ro::render {
namespace {

[[nodiscard]] std::int32_t DrawBelow(std::uint32_t& rng_state, std::int32_t exclusive_bound) {
  return static_cast<std::int32_t>(NextEffectNodeDraw(rng_state) %
                                   static_cast<std::uint32_t>(exclusive_bound));
}

}  // namespace

EffectGroundBoxRecipe EffectGroundBoxRecipeOf(std::uint8_t style_byte) {
  EffectGroundBoxRecipe recipe;
  // ⚠ LA TAILLE EST LA SEULE CHOSE QUE TOUT OCTET REÇOIT : le bâtisseur la pose
  // dans chacune de ses branches, y compris celle qui ne pose rien d'autre.
  recipe.half_side =
      style_byte == kGroundBoxPlainByte ? kGroundBoxWideHalfSide : kGroundBoxNarrowHalfSide;
  switch (style_byte) {
    case kGroundBoxPlainByte:
      recipe.lift_key = kGroundBoxTallLift;
      recipe.alpha = kGroundBoxDimAlpha;
      recipe.duration = kGroundBoxShortDuration;
      return recipe;
    case kGroundBoxPaleByte:
      recipe.lift_key = kGroundBoxMidLift;
      recipe.alpha = kGroundBoxBrightAlpha;
      recipe.duration = kGroundBoxMidDuration;
      return recipe;
    case kGroundBoxFullByte:
      recipe.lift_key = kGroundBoxFullLift;
      recipe.alpha = kGroundBoxDimAlpha;
      recipe.duration = kGroundBoxEndlessDuration;
      return recipe;
    case kGroundBoxMuteByte:
      // ⚠ CET OCTET NE POSE NI ALPHA NI DURÉE, et sa boîte naîtrait donc
      // invisible. Aucun site ne l'emploie ; lui prêter l'alpha d'un voisin
      // montrerait une figure que le client ne montre pas.
      recipe.lift_key = kGroundBoxMidLift;
      return recipe;
    case kGroundBoxPulsingByte:
      recipe.lift_key = kGroundBoxMidLift;
      recipe.alpha = kGroundBoxBrightAlpha;
      recipe.duration = kGroundBoxMidDuration;
      recipe.draws_blue = true;
      return recipe;
    default:
      // Le cas par défaut du bâtisseur ne pose QUE la taille.
      return recipe;
  }
}

std::span<const EffectGroundBoxPattern> EffectGroundBoxPatternsFor(std::uint32_t effect_id) {
  const std::span<const EffectGroundBoxPattern> table = EffectGroundBoxPatternTable();
  const auto first =
      std::ranges::lower_bound(table, effect_id, {}, &EffectGroundBoxPattern::effect_id);
  const auto last =
      std::ranges::upper_bound(table, effect_id, {}, &EffectGroundBoxPattern::effect_id);
  return table.subspan(static_cast<std::size_t>(first - table.begin()),
                       static_cast<std::size_t>(last - first));
}

std::array<EffectTrack, kEffectTracksPerNode> SeedEffectGroundBoxTracks(
    const EffectGroundBoxPattern& pattern, std::uint32_t& generator) {
  const EffectGroundBoxRecipe recipe = EffectGroundBoxRecipeOf(pattern.style_byte);
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  EffectTrack& track = tracks.front();
  track.active = true;
  track.ticks = 0;
  // ⚠ L'ANGLE DE DÉPART EST TIRÉ AU SORT, et c'est le seul tirage que tous les
  // octets font : deux boîtes posées ensemble ne respirent pas en phase.
  track.degrees = static_cast<std::int16_t>(DrawBelow(generator, kEffectTrackFullTurnDegrees));
  track.azimuth_degrees = 0;
  track.alpha = recipe.alpha;
  track.style = recipe.duration;
  track.lift_key = recipe.lift_key;
  track.value[kGroundBoxSlotOfHalfX] = recipe.half_side;
  track.value[kGroundBoxSlotOfHalfZ] = recipe.half_side;
  // ⚠ LA HAUTEUR NAÎT NULLE : c'est le mouvement qui déplie la boîte.
  track.value[kGroundBoxSlotOfHeight] = 0.0F;
  track.value[kGroundBoxSlotOfVariant] = static_cast<float>(pattern.style_byte);
  if (recipe.draws_blue) {
    track.value[kGroundBoxSlotOfBlueFloor] =
        static_cast<float>(DrawBelow(generator, kGroundBoxBlueFloorSpread));
  }
  track.value[kGroundBoxSlotOfBlueWave] = 0.0F;
  track.value[kGroundBoxSlotOfBlueFalling] = 0.0F;
  return tracks;
}

PooledEffectNode SowEffectGroundBoxNode(const EffectGroundBoxPattern& pattern, glm::vec3 anchor,
                                        std::int32_t effect_life_ticks, std::uint32_t& generator) {
  PooledEffectNode pooled;
  pooled.shape = PooledNodeShape::kGroundBox;
  pooled.ground_position = anchor;
  pooled.owner_position = anchor;
  pooled.node.life_ticks = effect_life_ticks;
  pooled.resource_paths = {std::string(pattern.texture)};
  pooled.tracks = SeedEffectGroundBoxTracks(pattern, generator);
  return pooled;
}

}  // namespace ro::render
