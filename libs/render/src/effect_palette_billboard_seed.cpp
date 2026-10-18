#include "ro/render/effect_palette_billboard_seed.h"

#include <string>

#include "ro/core/sorted_table.h"
#include "ro/render/effect_node.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_track.h"

namespace ro::render {
namespace {

// CE QUE LES QUATRE PISTES PARTAGENT, relevé dans le bâtisseur.
constexpr float kSeededKey = 10.0F;
constexpr float kSeededLift = 10.0F;

// DE COMBIEN CHAQUE PISTE EST RETARDÉE sur la précédente, en tics. Le client
// écrit des compteurs négatifs, et le mouvement ne fait rien tant qu'ils le
// sont : c'est l'échelonnement qui fait la spirale.
constexpr std::int32_t kDelayPerTrack = 20;

// Un tirage entier sur le tour, comme le modulo du client.
[[nodiscard]] std::int16_t DrawOnTheTurn(std::uint32_t& generator) {
  return static_cast<std::int16_t>(NextEffectNodeDraw(generator) %
                                   static_cast<std::uint32_t>(kEffectTrackFullTurnDegrees));
}

}  // namespace

std::span<const EffectPaletteBillboardPattern> EffectPaletteBillboardPatternsFor(
    std::uint32_t effect_id) {
  return core::RowsWithKey(EffectPaletteBillboardPatternTable(), effect_id,
                           &EffectPaletteBillboardPattern::effect_id);
}

std::array<EffectTrack, kEffectTracksPerNode> SeedEffectPaletteBillboardTracks(
    const EffectPaletteBillboardPattern& pattern, std::uint32_t& generator) {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  for (std::size_t rang = 0; rang < tracks.size(); ++rang) {
    EffectTrack& track = tracks[rang];
    track.active = true;
    track.ticks = -kDelayPerTrack * static_cast<std::int32_t>(rang);
    track.alpha = pattern.alpha;
    track.lift_key = kSeededKey;
    track.value[kSlotOfBillboardLift] = kSeededLift;
    // ⚠ LA NUANCE EST LE RANG, et c'est ce qui fait paraître les quatre ensemble.
    track.value[kSlotOfBillboardShade] = static_cast<float>(rang);
    track.value[kSlotOfBillboardPalette] = static_cast<float>(pattern.palette);
    // ⚠ LA TAILLE RESTE À ZÉRO, et c'est voulu : le mouvement la calcule. La
    // semer donnerait un premier tic à la mauvaise échelle.
    track.value[kSlotOfBillboardSize] = 0.0F;
    // ⚠ LE BALAYAGE SE TIRE AVANT L'AZIMUT, et l'ordre compte : deux éclats
    // semés à la suite doivent différer comme chez le client.
    track.degrees = DrawOnTheTurn(generator);
    track.azimuth_degrees = DrawOnTheTurn(generator);
  }
  return tracks;
}

PooledEffectNode SowEffectPaletteBillboardNode(const EffectPaletteBillboardPattern& pattern,
                                               glm::vec3 anchor, std::int32_t effect_life_ticks,
                                               std::uint32_t& generator) {
  PooledEffectNode pooled = PooledNodeAt(PooledNodeShape::kPaletteBillboard, anchor);
  pooled.node.life_ticks = effect_life_ticks;
  pooled.resource_paths = {std::string(pattern.texture)};
  pooled.tracks = SeedEffectPaletteBillboardTracks(pattern, generator);
  return pooled;
}

}  // namespace ro::render
