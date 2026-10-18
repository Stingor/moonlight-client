#include "ro/render/effect_electric_arc_seed.h"

#include <cmath>
#include <cstddef>
#include <string>

#include "ro/core/sorted_table.h"
#include "ro/render/effect_lightning_fan.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_track.h"

#include "effect_draw_below.h"

namespace ro::render {
namespace {

// L'ALPHA DE NAISSANCE D'UNE PISTE, selon son rang.
[[nodiscard]] std::int16_t BirthAlpha(std::size_t rang) {
  if (rang == 0) {
    return kElectricArcLeadAlpha;
  }
  return static_cast<std::int16_t>(kElectricArcTrailingAlphaBase -
                                   kElectricArcTrailingAlphaStep * static_cast<std::int16_t>(rang));
}

}  // namespace

std::span<const EffectElectricArcPattern> EffectElectricArcPatternsFor(std::uint32_t effect_id) {
  return core::RowsWithKey(EffectElectricArcPatternTable(), effect_id,
                           &EffectElectricArcPattern::effect_id);
}

std::int16_t ElectricArcStartTiltDegrees(float owner_facing_degrees) {
  constexpr auto kTour = static_cast<float>(kEffectTrackFullTurnDegrees);
  // L'ANGLE DU CORPS, rangé par le client dans [0, 360).
  float body =
      std::fmod(static_cast<float>(kEffectTrackHalfTurnDegrees) - owner_facing_degrees, kTour);
  if (body < 0.0F) {
    body += kTour;
  }
  // ⚠ LE CLIENT TRONQUE LE FLOTTANT, vers zéro : un
  // cap de moins quatre-vingt-neuf et demi donne moins quatre-vingt-neuf.
  auto cap =
      static_cast<std::int16_t>(static_cast<std::int32_t>(body - kElectricArcFacingOffsetDegrees));
  if (cap < 0) {
    cap = static_cast<std::int16_t>(cap + kEffectTrackFullTurnDegrees);
  }
  return cap;
}

std::array<EffectTrack, kEffectTracksPerNode> SeedEffectElectricArcTracks(
    const EffectElectricArcPattern& pattern, glm::vec3 anchor, float owner_facing_degrees,
    std::uint32_t& generator) {
  const std::int16_t cap = ElectricArcStartTiltDegrees(owner_facing_degrees);
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  for (std::size_t rang = 0; rang < tracks.size(); ++rang) {
    EffectTrack& track = tracks[rang];
    track.active = true;
    track.ticks = 0;
    track.lift_key = kElectricArcTubeRadius;
    track.alpha = BirthAlpha(rang);
    // ⚠ LES QUATRE PISTES PARTENT DU MÊME CAP : c'est le retard de rotation,
    // dix tics par rang, qui les écarte ensuite.
    track.tilt_degrees = cap;
    track.radius_at_anchor = kElectricArcReach;
    track.value[0] = 0.0F;
    // ⚠ UN SEUL TIRAGE PAR PISTE : le facteur de bombement.
    track.tail[kLightningFanTailOfBow] =
        static_cast<float>(DrawBelow(generator, kElectricArcBowSpread)) * kElectricArcBowStep;
    track.anchor = anchor + glm::vec3{0.0F, kElectricArcAnchorLift, 0.0F};
    track.tail[kLightningFanTailOfSegments] = kElectricArcSegments;
    track.style = pattern.spin_style;
    // LE BALAYAGE EST NUL : le tube prend le dégradé.
    track.degrees = 0;
  }
  return tracks;
}

PooledEffectNode SowEffectElectricArcNode(const EffectElectricArcPattern& pattern, glm::vec3 anchor,
                                          float owner_facing_degrees,
                                          std::int32_t effect_life_ticks,
                                          std::uint32_t& generator) {
  PooledEffectNode pooled = PooledNodeAt(PooledNodeShape::kElectricArc, anchor);
  pooled.owner_facing_degrees = owner_facing_degrees;
  // LA VIE DU NŒUD EST CELLE DE L'INSTANCE, relue par la queue commune du
  // répartiteur.
  pooled.node.life_ticks = effect_life_ticks;
  pooled.resource_paths = {std::string(pattern.texture)};
  pooled.tracks = SeedEffectElectricArcTracks(pattern, anchor, owner_facing_degrees, generator);
  return pooled;
}

}  // namespace ro::render
