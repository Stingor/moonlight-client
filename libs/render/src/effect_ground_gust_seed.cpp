#include "ro/render/effect_ground_gust_seed.h"

#include <cstddef>
#include <string>

#include "ro/core/sorted_table.h"
#include "ro/render/effect_ground_gust.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_track.h"

#include "effect_draw_below.h"
#include "effect_track_angles.h"

namespace ro::render {

std::span<const EffectGroundGustPattern> EffectGroundGustPatternsFor(std::uint32_t effect_id) {
  return core::RowsWithKey(EffectGroundGustPatternTable(), effect_id,
                           &EffectGroundGustPattern::effect_id);
}

std::array<EffectTrack, kEffectTracksPerNode> SeedEffectGroundGustTracks(glm::vec3 anchor,
                                                                         glm::vec3 toward_target,
                                                                         std::uint32_t& generator) {
  const std::int16_t cap =
      static_cast<std::int16_t>(HeadingOfVector(toward_target.x, toward_target.z));
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  for (std::size_t rang = 0; rang < tracks.size(); ++rang) {
    EffectTrack& track = tracks[rang];
    track.active = true;
    // ⚠ TROIS DES QUATRE COMPTEURS NE SERVENT À RIEN, et ils sont posés quand
    // même : le bâtisseur écrit moins trois, moins deux, moins un et zéro,
    // mais le mouvement ne lit que celui de la piste zéro. Les poser tous est
    // la transcription ; en déduire que les trois échos se réveillent à leur
    // tour serait inventer.
    track.ticks = static_cast<std::int32_t>(rang) - kGroundGustDelayTicks;
    track.alpha = 0;
    track.radius_at_anchor = kGroundGustRadius;
    // ⚠ L'INCLINAISON DE DÉPART EST LE RANG DE LA PISTE, et seule celle de la
    // piste zéro tourne ensuite : les trois autres reçoivent la sienne par
    // l'écho.
    track.tilt_degrees = static_cast<std::int16_t>(rang);
    track.azimuth_degrees = cap;
    track.value[kGroundGustSlotOfSpeed] = kGroundGustStartSpeed;
    // ⚠ UN TIRAGE PAR PISTE, ET UN SEUL : la hauteur. Les quatre pistes ne
    // flottent donc pas à la même altitude, et sauter les tirages des échos
    // décalerait toute la suite du générateur.
    const float descent = static_cast<float>(DrawBelow(generator, kGroundGustLiftSpread));
    track.anchor = {anchor.x, anchor.y + kGroundGustLiftAboveNode - descent, anchor.z};
    track.mark[kGroundGustMarkOfUnused] = 0;
    // ⚠ LE BÂTISSEUR NE POSE QUE LA TEINTE ZÉRO — le bleu. Les autres teintes
    // du dessinateur appartiennent aux sous-types 122 et 125 (voir
    // `effect_homing_gust_seed.h`) ; les prêter à celui-ci peindrait une
    // bourrasque que le client ne montre pas.
    track.mark[kGroundGustMarkOfColor] = 0;
  }
  return tracks;
}

PooledEffectNode SowEffectGroundGustNode(const EffectGroundGustPattern& pattern, glm::vec3 anchor,
                                         glm::vec3 toward_target, std::int32_t effect_life_ticks,
                                         std::uint32_t& generator) {
  PooledEffectNode pooled = PooledNodeAt(PooledNodeShape::kGroundGust, anchor);
  pooled.node.life_ticks = effect_life_ticks;
  pooled.resource_paths = {std::string(pattern.texture)};
  pooled.tracks = SeedEffectGroundGustTracks(anchor, toward_target, generator);
  return pooled;
}

}  // namespace ro::render
