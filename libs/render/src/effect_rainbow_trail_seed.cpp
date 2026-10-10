#include "ro/render/effect_rainbow_trail_seed.h"

#include <algorithm>
#include <string>

#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_rainbow_trail.h"
#include "ro/render/effect_track.h"

namespace ro::render {

std::span<const EffectRainbowTrailPattern> EffectRainbowTrailPatternsFor(std::uint32_t effect_id) {
  const std::span<const EffectRainbowTrailPattern> table = EffectRainbowTrailPatternTable();
  const auto first =
      std::ranges::lower_bound(table, effect_id, {}, &EffectRainbowTrailPattern::effect_id);
  const auto last =
      std::ranges::upper_bound(table, effect_id, {}, &EffectRainbowTrailPattern::effect_id);
  return table.subspan(static_cast<std::size_t>(first - table.begin()),
                       static_cast<std::size_t>(last - first));
}

std::array<EffectTrack, kEffectTracksPerNode> SeedEffectRainbowTrailTracks(
    std::int16_t billboard_turn_degrees) {
  // ⚠ LE TOUR EST DÉJÀ `360 − lacet` DE LA CAMÉRA DU CLIENT : c'est l'angle même
  // que son bâtisseur pose. Le recompléter ici rendrait le lacet brut.
  const std::int16_t facing_camera = billboard_turn_degrees;

  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  // ⚠ UNE SEULE PISTE VIVE : le bâtisseur éteint les quatre puis rallume la
  // première.
  EffectTrack& track = tracks.front();
  track.active = true;
  track.ticks = 0;
  track.alpha = 0;
  track.radius_at_anchor = kRainbowTrailRadius;
  track.degrees = kRainbowTrailSweepDegrees;
  // ⚠ L'AZIMUT PART DE ZÉRO et c'est le mouvement qui le fait courir : l'arc ne
  // s'ouvre pas d'un bloc, il se trace segment par segment.
  track.azimuth_degrees = 0;
  // ⚠ L'INCLINAISON EST FIGÉE À LA NAISSANCE — voir l'en-tête. Le style de la
  // première piste porte la même valeur chez le client ; nous la rangeons dans
  // l'inclinaison, seule case que la figure lise.
  track.tilt_degrees = facing_camera;
  track.style = facing_camera;
  return tracks;
}

PooledEffectNode SowEffectRainbowTrailNode(const EffectRainbowTrailPattern& pattern,
                                           glm::vec3 anchor, std::int32_t effect_life_ticks,
                                           std::int16_t billboard_turn_degrees) {
  PooledEffectNode pooled;
  pooled.shape = PooledNodeShape::kRainbowTrail;
  pooled.ground_position = anchor;
  pooled.owner_position = anchor;
  pooled.node.life_ticks = effect_life_ticks;
  pooled.resource_paths = {std::string(pattern.texture)};
  pooled.tracks = SeedEffectRainbowTrailTracks(billboard_turn_degrees);
  return pooled;
}

}  // namespace ro::render
