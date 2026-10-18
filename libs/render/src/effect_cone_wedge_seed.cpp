#include "ro/render/effect_cone_wedge_seed.h"

#include <string>

#include "ro/core/sorted_table.h"
#include "ro/render/effect_node.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_track.h"

namespace ro::render {
namespace {

// CE QUE LES QUATRE PISTES PARTAGENT.
constexpr float kRadiusAtAnchor = 2.0F;
constexpr float kApexHeight = -12.0F;
constexpr std::int16_t kStartsInvisible = 0;

// DE COMBIEN LES QUATRE PISTES S'ÉCARTENT l'une de l'autre : un quart de tour.
constexpr std::int16_t kDegreesPerTrack = 90;

// ⚠ ET DE COMBIEN CHAQUE TOUR DE BOUCLE DÉCALE L'ENSEMBLE. Vingt-deux degrés
// d'ordinaire, SEPT pour la seule variante trois — son éventail est donc trois
// fois plus resserré, ce qui va de pair avec ses coins plus fins.
constexpr std::int16_t kDegreesPerSpoke = 22;
constexpr std::int16_t kTightDegreesPerSpoke = 7;
constexpr std::uint8_t kVariantWithTightSpokes = 3;

// LES TROIS VARIANTES QUI PARTENT TOUT DE SUITE. Toutes les autres attendent.
constexpr std::int32_t kDelayedStartTick = -24;
[[nodiscard]] bool StartsImmediately(std::uint8_t variant) {
  return variant == 2 || variant == 3 || variant == 6;
}

}  // namespace

std::span<const EffectConeWedgePattern> EffectConeWedgePatternsFor(std::uint32_t effect_id) {
  return core::RowsWithKey(EffectConeWedgePatternTable(), effect_id,
                           &EffectConeWedgePattern::effect_id);
}

std::array<EffectTrack, kEffectTracksPerNode> SeedEffectConeWedgeTracks(
    const EffectConeWedgePattern& pattern, std::uint8_t spoke, std::uint32_t& generator) {
  const std::int16_t per_spoke =
      pattern.variant == kVariantWithTightSpokes ? kTightDegreesPerSpoke : kDegreesPerSpoke;
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  for (std::size_t rang = 0; rang < tracks.size(); ++rang) {
    EffectTrack& track = tracks[rang];
    // ⚠ LES QUATRE SONT VIVES, à la différence de la plupart des familles : la
    // boucle du bâtisseur les parcourt toutes.
    track.active = true;
    track.ticks = StartsImmediately(pattern.variant) ? 0 : kDelayedStartTick;
    track.alpha = kStartsInvisible;
    track.radius_at_anchor = kRadiusAtAnchor;
    track.value[kSlotOfWedgeApexHeight] = kApexHeight;
    track.value[kSlotOfWedgeVariant] = static_cast<float>(pattern.variant);
    // ⚠ L'INCLINAISON PORTE LES DEUX ÉCARTS À LA FOIS : celui de la piste dans le
    // nœud, et celui du nœud dans l'éventail. C'est cette somme qui déploie les
    // rayons ; n'en garder qu'un replierait l'éventail sur lui-même.
    track.tilt_degrees = static_cast<std::int16_t>(
        kDegreesPerTrack * static_cast<std::int16_t>(rang) + per_spoke * spoke);
    track.azimuth_degrees = static_cast<std::int16_t>(
        NextEffectNodeDraw(generator) % static_cast<std::uint32_t>(kEffectTrackFullTurnDegrees));
  }
  return tracks;
}

PooledEffectNode SowEffectConeWedgeNode(const EffectConeWedgePattern& pattern, std::uint8_t spoke,
                                        glm::vec3 anchor, std::int32_t effect_life_ticks,
                                        std::uint32_t& generator) {
  PooledEffectNode pooled = PooledNodeAt(PooledNodeShape::kConeWedge, anchor);
  pooled.node.life_ticks = effect_life_ticks;
  pooled.resource_paths = {std::string(pattern.texture)};
  pooled.tracks = SeedEffectConeWedgeTracks(pattern, spoke, generator);
  return pooled;
}

}  // namespace ro::render
