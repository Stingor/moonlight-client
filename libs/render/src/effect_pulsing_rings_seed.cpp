#include "ro/render/effect_pulsing_rings_seed.h"

#include <string>

#include "ro/core/sorted_table.h"
#include "ro/render/effect_node.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_track.h"

namespace ro::render {
namespace {

// CE QUE LES TROIS ANNEAUX PARTAGENT CHEZ LES CINQ BÂTISSEURS.
//
// Ils naissent SANS RAYON et SANS ALPHA : c'est le mouvement qui les ouvre et
// les allume. Les semer visibles les ferait paraître d'un coup, à la taille
// qu'ils n'atteignent qu'au bout d'une seconde.
constexpr float kRadiusAtBirth = 0.0F;
constexpr std::int16_t kStartsInvisible = 0;

// ⚠ LES VINGT ET UNE MARQUES VALENT UN, et les vingt et une valeurs zéro. Les
// cinq bâtisseurs écrivent les deux séries à la suite, dans la même boucle.
constexpr std::uint8_t kMarkAtBirth = 1;
constexpr float kValueAtBirth = 0.0F;

}  // namespace

std::span<const EffectPulsingRingsPattern> EffectPulsingRingsPatternsFor(std::uint32_t effect_id) {
  return core::RowsWithKey(EffectPulsingRingsPatternTable(), effect_id,
                           &EffectPulsingRingsPattern::effect_id);
}

std::array<EffectTrack, kEffectTracksPerNode> SeedEffectPulsingRingsTracks(
    const EffectPulsingRingsPattern& pattern) {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  for (std::size_t rang = 0; rang < kEffectRadialBurstRingCount; ++rang) {
    EffectTrack& track = tracks[rang];
    track.active = true;
    // ⚠ LES TROIS PARTENT L'UN APRÈS L'AUTRE, et c'est cela la pulsation : le
    // deuxième attend un pas, le troisième deux. Les semer ensemble n'en
    // montrerait qu'un, trois fois plus épais.
    track.ticks = -pattern.delay_step_ticks * static_cast<std::int32_t>(rang);
    track.degrees = kPulsingRingSweepDegrees;
    track.lift_key = kPulsingRingLiftKey;
    track.azimuth_degrees = kPulsingRingAzimuthDegrees[rang];
    track.tilt_degrees = kPulsingRingTiltDegrees[rang];
    track.radius_at_anchor = kRadiusAtBirth;
    track.style = pattern.track_style;
    track.alpha = kStartsInvisible;
    track.value.fill(kValueAtBirth);
    track.mark.fill(kMarkAtBirth);
  }

  // LE REGISTRE, et il reste ÉTEINT. Le mouvement ne le parcourt pas ; il y lit
  // le mode et y tient l'âge global. L'allumer le ferait dessiner un quatrième
  // anneau que le client ne montre jamais.
  EffectTrack& registre = tracks[kEffectRadialBurstRegisterTrack];
  registre.active = false;
  registre.ticks = 0;
  registre.style = pattern.mode;
  return tracks;
}

PooledEffectNode SowEffectPulsingRingsNode(const EffectPulsingRingsPattern& pattern,
                                           glm::vec3 anchor, std::int32_t effect_life_ticks) {
  PooledEffectNode pooled = PooledNodeAt(PooledNodeShape::kPulsingRings, anchor);
  pooled.node.life_ticks = effect_life_ticks;
  pooled.resource_paths = {std::string(pattern.texture)};
  pooled.trail_variant = pattern.trail_variant;
  pooled.tracks = SeedEffectPulsingRingsTracks(pattern);
  return pooled;
}

}  // namespace ro::render
