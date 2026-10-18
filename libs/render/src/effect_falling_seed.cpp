#include "ro/render/effect_falling_seed.h"

#include <algorithm>
#include <string>

#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_track.h"

#include "effect_draw_below.h"

namespace ro::render {
namespace {

// LES CASES QUE LE BÂTISSEUR REMPLIT.
constexpr std::size_t kSlotOfFallStep = 1;
constexpr std::size_t kMarkOfTexture = 0;
constexpr std::size_t kMarkOfDrawing = 2;
constexpr std::size_t kMarkOfSubDrawing = 3;
constexpr std::size_t kMarkOfStyle = 4;
constexpr std::size_t kTailSlotOfSideways = 3;
constexpr std::size_t kTailSlotOfHeight = 4;
constexpr std::size_t kTailSlotOfDepth = 5;

// CE QUE LES CINQ STYLES PARTAGENT.
//
// Le PAS DE CHUTE : de combien l'objet se rapproche de son porteur à chaque tic.
constexpr float kFallStepPerTick = 3.0F;

// ⚠ L'ANGLE DE DÉPART SE TIRE SUR LE TOUR ENTIER, et c'est ce qui fait que deux
// potions lancées d'affilée ne tournent pas en phase. Le poser ferait tomber
// toutes les lances de l'effet 930 dans la même orientation.
constexpr std::int16_t kNoAzimuth = 0;

}  // namespace

const EffectFallingStyleSeed* EffectFallingStyleSeedFor(std::uint8_t style) {
  const std::span<const EffectFallingStyleSeed> table = EffectFallingStyleSeedTable();
  const auto found = std::ranges::find(table, style, &EffectFallingStyleSeed::style);
  return found == table.end() ? nullptr : &*found;
}

std::array<EffectTrack, kEffectTracksPerNode> SeedEffectFallingTracks(
    const EffectFallingStyleSeed& seed, std::uint32_t& generator) {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  // ⚠ LES QUATRE SONT VIVES, ET TROIS SONT NUES. C'est le mouvement qui les
  // remplit, une par tic ; les garnir ici donnerait quatre objets au lieu d'un
  // objet et de sa traînée.
  for (EffectTrack& track : tracks) {
    track.active = true;
    track.alpha = 0;
  }

  EffectTrack& head = tracks.front();
  head.ticks = 0;
  head.azimuth_degrees = kNoAzimuth;
  head.radius_at_anchor = seed.radius_at_anchor;
  head.value[kSlotOfFallStep] = kFallStepPerTick;
  head.mark[kMarkOfTexture] = 0;
  head.mark[kMarkOfDrawing] = seed.draw_mark;
  head.mark[kMarkOfSubDrawing] = seed.sub_mark;
  head.mark[kMarkOfStyle] = seed.style;
  head.tail[kTailSlotOfSideways] = 0.0F;
  head.tail[kTailSlotOfDepth] = 0.0F;

  // ⚠ L'ANGLE SE TIRE AVANT LA HAUTEUR, et l'ordre compte : deux nœuds semés à
  // la suite doivent différer comme chez le client, pas autrement.
  head.degrees = static_cast<std::int16_t>(DrawBelow(generator, kEffectTrackFullTurnDegrees));
  head.tail[kTailSlotOfHeight] =
      seed.lift_above_owner +
      (seed.lift_spread > 0.0F
           ? static_cast<float>(DrawBelow(generator, static_cast<std::int32_t>(seed.lift_spread)))
           : 0.0F);
  return tracks;
}

PooledEffectNode SowEffectFallingNode(const EffectFallingNodePattern& pattern, glm::vec3 anchor,
                                      std::int32_t effect_life_ticks, std::uint32_t& generator) {
  PooledEffectNode pooled;
  pooled.shape = PooledNodeShape::kFallingItem;
  pooled.ground_position = anchor;
  pooled.resource_paths = {std::string(pattern.texture)};
  pooled.node.life_ticks = effect_life_ticks;
  const EffectFallingStyleSeed* style_seed = EffectFallingStyleSeedFor(pattern.style);
  if (style_seed != nullptr) {
    pooled.tracks = SeedEffectFallingTracks(*style_seed, generator);
  }
  return pooled;
}

}  // namespace ro::render
