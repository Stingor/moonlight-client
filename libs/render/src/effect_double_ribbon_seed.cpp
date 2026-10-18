#include "ro/render/effect_double_ribbon_seed.h"

#include <string>

#include "ro/core/sorted_table.h"
#include "ro/render/effect_double_ribbon.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_track.h"

namespace ro::render {
namespace {

// LA CASE QUE LE BÂTISSEUR REMPLIT ET QUE PERSONNE NE LIT.
//
// ⚠ ELLE EST ÉCRITE ET RESTE MUETTE : ni la figure ni le mouvement du sous-type
// 106 ne la touchent. La poser quand même est ce qui permettra de s'en
// apercevoir le jour où un second dessinateur du même bâtisseur la lira.
constexpr std::size_t kSlotWrittenAndNeverRead = 0;
constexpr float kValueWrittenAndNeverRead = 10.0F;

}  // namespace

std::span<const EffectDoubleRibbonPattern> EffectDoubleRibbonPatternsFor(std::uint32_t effect_id) {
  return core::RowsWithKey(EffectDoubleRibbonPatternTable(), effect_id,
                           &EffectDoubleRibbonPattern::effect_id);
}

std::array<EffectTrack, kEffectTracksPerNode> SeedEffectDoubleRibbonTracks(
    std::int32_t delay_ticks) {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  for (std::size_t rang = 0; rang < tracks.size(); ++rang) {
    EffectTrack& track = tracks[rang];
    track.active = true;
    track.ticks = delay_ticks;
    // ⚠ L'ALPHA PART À ZÉRO : rien ne se voit tant que le mouvement n'a pas
    // tourné, et la figure abandonne une piste dont l'alpha est nul.
    track.alpha = 0;
    track.degrees = static_cast<std::int16_t>(kEffectTrackFullTurnDegrees);
    track.lift_key = kDoubleRibbonLiftKey;
    track.radius_at_anchor = kDoubleRibbonRadius;
    track.value[kSlotWrittenAndNeverRead] = kValueWrittenAndNeverRead;
    // ⚠ LES DEUX ÉCHELONNEMENTS NE SONT PAS LE MÊME : le TOUR décale l'anneau
    // sur son cercle, l'OUVERTURE décale son battement dans le temps. Les quatre
    // pistes sont donc quatre anneaux à des âges différents et à des places
    // différentes ; n'en échelonner qu'un les empilerait à moitié.
    track.tilt_degrees =
        static_cast<std::int16_t>(kDoubleRibbonTurnPerTrack * static_cast<std::int16_t>(rang));
    track.value[kSlotOfDoubleRibbonOpening] =
        kDoubleRibbonOpeningPerTrack * static_cast<float>(rang);
  }
  return tracks;
}

PooledEffectNode SowEffectDoubleRibbonNode(const EffectDoubleRibbonPattern& pattern,
                                           glm::vec3 anchor, std::int32_t delay_ticks,
                                           std::int32_t effect_life_ticks) {
  PooledEffectNode pooled = PooledNodeAt(PooledNodeShape::kDoubleRibbon, anchor);
  pooled.node.life_ticks = effect_life_ticks;
  pooled.resource_paths = {std::string(pattern.texture)};
  pooled.tracks = SeedEffectDoubleRibbonTracks(delay_ticks);
  // ⚠ LA VARIANTE VIT DANS LE NŒUD, et c'est la PRIMITIVE qui la lit — pas la
  // figure. Voir `effect_trail_palette.h`.
  pooled.trail_variant = kDoubleRibbonTrailVariant;
  return pooled;
}

}  // namespace ro::render
