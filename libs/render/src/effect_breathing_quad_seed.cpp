#include "ro/render/effect_breathing_quad_seed.h"

#include <algorithm>
#include <string>

#include "ro/render/effect_node.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_track.h"

namespace ro::render {
namespace {

// LA HAUTEUR, la même pour tous : le client l'écrit en constante, et elle est
// NÉGATIVE — son repère descend, donc le carré est au-dessus de l'ancre.
constexpr float kHeightAboveAnchor = -2.0F;

// ⚠ TROIS OCTETS POSENT LEUR INCLINAISON au lieu de la tirer, et LA VALEUR QU'ILS POSENT EST TROIS
// CENT SOIXANTE, pas zéro. Hors du tour, donc, et c'est voulu : le mouvement la fait redescendre au
// tic suivant, mais au premier tic son sinus vaut celui de zéro. Le ramener d'avance changerait ce
// premier tic.
constexpr std::int16_t kFixedTilt = 360;

}  // namespace

std::span<const EffectBreathingQuadPattern> EffectBreathingQuadPatternsFor(
    std::uint32_t effect_id) {
  const std::span<const EffectBreathingQuadPattern> table = EffectBreathingQuadPatternTable();
  const auto first =
      std::ranges::lower_bound(table, effect_id, {}, &EffectBreathingQuadPattern::effect_id);
  const auto last =
      std::ranges::upper_bound(table, effect_id, {}, &EffectBreathingQuadPattern::effect_id);
  return table.subspan(static_cast<std::size_t>(first - table.begin()),
                       static_cast<std::size_t>(last - first));
}

EffectBreathingQuadSeed EffectBreathingQuadSeedFor(std::uint8_t builder_argument) {
  // LES SEPT JEUX, branche par branche comme le client les écrit. Les rapprocher
  // par une règle donnerait six carrés au même rayon.
  switch (builder_argument) {
    case 2:
      return {12.0F, 0, 100, 5, false};
    case 3:
      return {10.0F, 225, 250, 111, false};
    case 4:
      return {12.0F, 0, 200, 4, false};
    case 10:
      return {30.0F, 0, 200, 10, true};
    case 11:
      return {28.0F, 0, 200, 10, true};
    case 12:
      return {20.0F, 0, 200, 10, true};
    default:
      return {7.0F, 0, 100, 5, false};
  }
}

std::array<EffectTrack, kEffectTracksPerNode> SeedEffectBreathingQuadTracks(
    const EffectBreathingQuadPattern& pattern, std::uint32_t& generator) {
  const EffectBreathingQuadSeed seed = EffectBreathingQuadSeedFor(pattern.builder_argument);
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  // ⚠ UNE SEULE PISTE VIVE : le bâtisseur allume la première et éteint les trois
  // autres. Les allumer toutes montrerait quatre carrés au même endroit.
  EffectTrack& track = tracks.front();
  track.active = true;
  track.ticks = 0;
  track.alpha = seed.alpha;
  track.radius_at_anchor = seed.radius_at_anchor;
  track.azimuth_degrees = seed.azimuth_degrees;
  track.value[kSlotOfBreathingHeight] = kHeightAboveAnchor;
  track.value[kSlotOfBreathingVariant] = static_cast<float>(pattern.builder_argument);
  // ⚠ L'INCLINAISON EST TIRÉE, sauf pour trois octets. C'est elle qui mène le
  // souffle du rayon : la poser à zéro pour tous ferait partir les six carrés
  // exactement à la même taille.
  track.tilt_degrees =
      seed.tilt_is_fixed
          ? kFixedTilt
          : static_cast<std::int16_t>(NextEffectNodeDraw(generator) %
                                      static_cast<std::uint32_t>(kEffectTrackFullTurnDegrees));
  return tracks;
}

PooledEffectNode SowEffectBreathingQuadNode(const EffectBreathingQuadPattern& pattern,
                                            glm::vec3 anchor, std::int32_t effect_life_ticks,
                                            std::uint32_t& generator) {
  PooledEffectNode pooled;
  pooled.shape = PooledNodeShape::kBreathingQuad;
  pooled.ground_position = anchor;
  pooled.owner_position = anchor;
  pooled.node.life_ticks = effect_life_ticks;
  pooled.resource_paths = {std::string(pattern.texture)};
  pooled.tracks = SeedEffectBreathingQuadTracks(pattern, generator);
  // ⚠ LE STYLE VIT DANS LE NŒUD : la figure le lit à un déplacement que sa
  // boucle de pistes ne décale pas, si bien que les quatre le partagent.
  pooled.trail_variant =
      static_cast<std::uint8_t>(EffectBreathingQuadSeedFor(pattern.builder_argument).node_style);
  return pooled;
}

}  // namespace ro::render
