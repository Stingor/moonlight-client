#include "ro/render/effect_pie_wedge_seed.h"

#include <string>

#include "ro/core/sorted_table.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_track.h"

namespace ro::render {
namespace {

// LE RAYON DU CERCLE, le même pour les douze quartiers : cent unités.
constexpr float kPieWedgeRadius = 100.0F;
// LA HAUTEUR DE LA POINTE, NÉGATIVE : le repère du client descend, donc le
// quartier est au-dessus de son ancre.
constexpr float kPieWedgeApexHeight = -5.0F;
constexpr std::int16_t kFullAlpha = 255;

}  // namespace

std::span<const EffectPieWedgePattern> EffectPieWedgePatternsFor(std::uint32_t effect_id) {
  return core::RowsWithKey(EffectPieWedgePatternTable(), effect_id,
                           &EffectPieWedgePattern::effect_id);
}

std::array<EffectTrack, kEffectTracksPerNode> SeedEffectPieWedgeTracks(std::int32_t tick) {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  for (std::size_t rang = 0; rang < tracks.size(); ++rang) {
    EffectTrack& track = tracks[rang];
    track.active = true;
    track.ticks = 0;
    track.alpha = kFullAlpha;
    track.radius_at_anchor = kPieWedgeRadius;
    track.azimuth_degrees = 0;
    track.value[kSlotOfPieWedgeApexHeight] = kPieWedgeApexHeight;
    // ⚠ LE RANG ET LE TIC SE COMPOSENT, et c'est ce qui ferme le cercle. Le
    // client lit son compteur de tics dans l'instance et le multiplie par
    // QUATRE — le nombre de pistes — avant d'y ajouter le rang : chaque nœud
    // reprend donc là où le précédent s'est arrêté.
    const auto place =
        static_cast<std::int32_t>(rang) + static_cast<std::int32_t>(kEffectTracksPerNode) * tick;
    track.tilt_degrees = static_cast<std::int16_t>(kPieWedgeDegreesPerStep * place);
  }
  return tracks;
}

PooledEffectNode SowEffectPieWedgeNode(const EffectPieWedgePattern& pattern, glm::vec3 anchor,
                                       std::int32_t tick, std::int32_t effect_life_ticks) {
  PooledEffectNode pooled = PooledNodeAt(PooledNodeShape::kPieWedge, anchor);
  pooled.node.life_ticks = effect_life_ticks;
  pooled.resource_paths = {std::string(pattern.texture)};
  pooled.tracks = SeedEffectPieWedgeTracks(tick);
  return pooled;
}

}  // namespace ro::render
