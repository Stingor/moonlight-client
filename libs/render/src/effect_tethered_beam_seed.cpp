#include "ro/render/effect_tethered_beam_seed.h"

#include <string>

#include "ro/core/sorted_table.h"
#include "ro/render/effect_node.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_tethered_beam.h"
#include "ro/render/effect_track.h"

namespace ro::render {

std::span<const EffectTetheredBeamPattern> EffectTetheredBeamPatternsFor(std::uint32_t effect_id) {
  return core::RowsWithKey(EffectTetheredBeamPatternTable(), effect_id,
                           &EffectTetheredBeamPattern::effect_id);
}

std::array<EffectTrack, kEffectTracksPerNode> SeedEffectTetheredBeamTracks(
    const EffectTetheredBeamPattern& pattern, glm::vec3 anchor,
    const std::optional<glm::vec3>& target) {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  EffectTrack& track = tracks.front();
  // ⚠ SANS CIBLE, LA PISTE NAÎT ÉTEINTE : le bâtisseur cherche l'acteur visé
  // et ne garde la piste allumée que s'il le trouve. Rien ne se dessine alors,
  // pas même un trait sans longueur.
  track.active = target.has_value();
  track.ticks = 0;
  track.alpha = pattern.alpha;
  track.lift_key = pattern.core_radius;
  // ⚠ LE RAYON À L'ANCRE RESTE NUL : cette figure prend le sien dans la clé, et
  // les trois bâtisseurs posent zéro ici. Le nom du champ ne dit pas son emploi
  // (ADR 0017).
  track.radius_at_anchor = 0.0F;
  track.value[kTetheredBeamSlotOfVariant] = pattern.variant;
  track.tail[kTetheredBeamTailOfNearEnd] = anchor.x;
  track.tail[kTetheredBeamTailOfNearEnd + 1] = anchor.y + kTetheredBeamLift;
  track.tail[kTetheredBeamTailOfNearEnd + 2] = anchor.z;
  const glm::vec3 far_point = target.value_or(anchor);
  track.tail[kTetheredBeamTailOfFarEnd] = far_point.x;
  track.tail[kTetheredBeamTailOfFarEnd + 1] = far_point.y + kTetheredBeamLift;
  track.tail[kTetheredBeamTailOfFarEnd + 2] = far_point.z;
  return tracks;
}

PooledEffectNode SowEffectTetheredBeamNode(const EffectTetheredBeamPattern& pattern,
                                           glm::vec3 anchor, const std::optional<glm::vec3>& target,
                                           std::int32_t effect_life_ticks) {
  PooledEffectNode pooled = PooledNodeAt(PooledNodeShape::kTetheredBeam, anchor);
  pooled.target_position = target;
  pooled.node.life_ticks = effect_life_ticks;
  pooled.resource_paths = {std::string(pattern.texture)};
  pooled.tracks = SeedEffectTetheredBeamTracks(pattern, anchor, target);
  return pooled;
}

}  // namespace ro::render
