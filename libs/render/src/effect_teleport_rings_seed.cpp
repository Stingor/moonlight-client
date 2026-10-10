#include "ro/render/effect_teleport_rings_seed.h"

#include <algorithm>
#include <string>

#include "ro/render/effect_node.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_track.h"

namespace ro::render {
namespace {

// LES QUATRE PISTES, dans l'ordre où le bâtisseur les pose.
//
// ⚠ L'ALPHA DESCEND DE VINGT PAR RANG et le rayon MONTE : l'anneau le plus
// intérieur est le plus vif. Les inverser mettrait la lumière au mauvais bout.
constexpr std::array<EffectTeleportRingSeed, kEffectTracksPerNode> kTracks = {{
    {100, 180, 90, 100.0F, 1.5F},
    {80, 270, 89, 70.0F, 3.0F},
    {60, 0, 88, 40.0F, 4.0F},
    {40, 90, 87, 15.0F, 5.0F},
}};

// LE BALAYAGE : le tour entier, pour les quatre.
constexpr std::int16_t kSweepDegrees = kEffectTrackFullTurnDegrees;

}  // namespace

std::span<const EffectTeleportRingSeed> EffectTeleportRingSeeds() { return kTracks; }

std::span<const EffectTeleportRingPattern> EffectTeleportRingPatternsFor(std::uint32_t effect_id) {
  const std::span<const EffectTeleportRingPattern> table = EffectTeleportRingPatternTable();
  const auto first =
      std::ranges::lower_bound(table, effect_id, {}, &EffectTeleportRingPattern::effect_id);
  const auto last =
      std::ranges::upper_bound(table, effect_id, {}, &EffectTeleportRingPattern::effect_id);
  return table.subspan(static_cast<std::size_t>(first - table.begin()),
                       static_cast<std::size_t>(last - first));
}

std::array<EffectTrack, kEffectTracksPerNode> SeedEffectTeleportRingTracks(
    const EffectTeleportRingPattern& pattern) {
  const float scale = static_cast<float>(pattern.scale);
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  for (std::size_t rang = 0; rang < tracks.size(); ++rang) {
    const EffectTeleportRingSeed& seed = kTracks[rang];
    EffectTrack& track = tracks[rang];
    track.active = true;
    track.alpha = seed.alpha;
    track.degrees = kSweepDegrees;
    track.azimuth_degrees = seed.azimuth_degrees;
    track.tilt_degrees = seed.tilt_degrees;
    track.ticks = 0;
    track.lift_key = seed.lift_key + scale * kTeleportRingKeyPerScale;
    track.radius_at_anchor = seed.radius_at_anchor - scale * kTeleportRingRadiusPerScale;
    // ⚠ L'AMPLITUDE EST UN OCTET, ET LE CLIENT LA TRONQUE. Il range dans la
    // marque la partie entière de la clé, sans se garder du débordement : une
    // clé de plus de cent vingt-sept y passe négative, et l'anneau respire à
    // l'envers. Le conversion en `std::int8_t` reproduit exactement cela.
    track.mark[kTeleportRingMarkOfSwitch] = 0;
    track.mark[kTeleportRingMarkOfAmplitude] =
        static_cast<std::uint8_t>(static_cast<std::int32_t>(track.lift_key));
    // ⚠ LES VINGT ET UNE CASES NAISSENT À ZÉRO, et le bâtisseur les y met
    // explicitement : un anneau sans longueur n'a pas d'aire, et c'est le
    // premier tic du mouvement qui lui en donne une.
    track.value = {};
  }
  return tracks;
}

PooledEffectNode SowEffectTeleportRingNode(const EffectTeleportRingPattern& pattern,
                                           glm::vec3 anchor, std::int32_t effect_life_ticks) {
  PooledEffectNode pooled;
  pooled.shape = PooledNodeShape::kTeleportRings;
  pooled.ground_position = anchor;
  pooled.owner_position = anchor;
  pooled.node.life_ticks = effect_life_ticks;
  pooled.resource_paths = {std::string(pattern.texture)};
  pooled.tracks = SeedEffectTeleportRingTracks(pattern);
  return pooled;
}

}  // namespace ro::render
