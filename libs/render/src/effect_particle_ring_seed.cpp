#include "ro/render/effect_particle_ring_seed.h"

#include <algorithm>
#include <string>

#include "ro/render/effect_life_table.h"
#include "ro/render/effect_node.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_track.h"

namespace ro::render {
namespace {

// CE QUE LES CINQ STYLES PARTAGENT, posé par leur bâtisseur commun.
//
// ⚠ L'AZIMUT ET LE RAYON SE DÉDUISENT DU RANG DE LA PISTE. Le client écrit
// `90 × i` et `1 + i` dans une boucle : quatre couronnes aux quatre points
// cardinaux, de plus en plus larges. Le même azimut les superposerait, le même
// rayon les confondrait.
constexpr std::int16_t kAzimuthStepPerTrack = 90;
constexpr float kRadiusOfFirstTrack = 1.0F;
constexpr float kRadiusGrowthPerTrack = 1.0F;

// Le balayage est le TOUR ENTIER pour les quatre.
constexpr std::int16_t kSweepDegrees = 360;

}  // namespace

const EffectParticleRingStyleSeed* EffectParticleRingStyleSeedAt(std::size_t rang) {
  const std::span<const EffectParticleRingStyleSeed> table = EffectParticleRingStyleSeedTable();
  return rang < table.size() ? &table[rang] : nullptr;
}

const EffectParticleRingStyleSeed* EffectParticleRingStyleSeedFor(std::string_view client_function,
                                                                  std::uint8_t style) {
  for (const EffectParticleRingStyleSeed& style_seed : EffectParticleRingStyleSeedTable()) {
    if (style_seed.client_function == client_function && style_seed.style == style) {
      return &style_seed;
    }
  }
  return nullptr;
}

std::array<EffectTrack, kEffectTracksPerNode> SeedEffectParticleRingTracks(
    const EffectParticleRingStyleSeed& seed) {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  for (std::size_t rang = 0; rang < tracks.size(); ++rang) {
    EffectTrack& track = tracks[rang];
    track.active = true;
    track.ticks = 0;
    track.degrees = kSweepDegrees;
    track.azimuth_degrees =
        static_cast<std::int16_t>(kAzimuthStepPerTrack * static_cast<std::int16_t>(rang));
    track.tilt_degrees = seed.tilt_degrees;
    track.lift_key = seed.lift_key;
    track.radius_at_anchor = kRadiusOfFirstTrack + kRadiusGrowthPerTrack * static_cast<float>(rang);
    // ⚠ L'ALPHA NAÎT NUL : l'anneau ne se voit pas au tic du semis.
    track.alpha = 0;
    // Le style que le site d'appel a passé, tel quel — le client le range là.
    track.style = seed.style;
  }
  return tracks;
}

PooledEffectNode SowEffectParticleRingNode(const EffectParticleRingNodePattern& pattern,
                                           glm::vec3 ground_position) {
  PooledEffectNode pooled;
  // ⚠ MÊME FIGURE QUE LES DEUX AUTRES ANNEAUX, MOUVEMENT DIFFÉRENT. Le
  // dessinateur est le même, mais le répartiteur de mouvement donne au trente
  // et un sa propre cible, le mouvement de gerbe à deux modes. La forme porte
  // les deux.
  pooled.shape = PooledNodeShape::kDualModeSprayRings;
  pooled.ground_position = ground_position;
  // Le nœud naît là où il est semé ; le mouvement suivra son porteur dès que la
  // fenêtre saura le dire.
  pooled.owner_position = ground_position;
  pooled.resource_paths = {std::string(pattern.texture)};
  const EffectParticleRingStyleSeed* style_seed =
      EffectParticleRingStyleSeedAt(pattern.style_index);
  if (style_seed != nullptr) {
    pooled.trail_variant = style_seed->trail_variant;
    pooled.spray_mode = style_seed->spray_mode;
    pooled.tracks = SeedEffectParticleRingTracks(*style_seed);
  }
  // 🔴 LA VIE EST CELLE DE L'INSTANCE, que le bâtisseur recopie et que la table
  // des durées porte (ADR 0042). Un défaut de cent tics éteindrait net, au
  // centième tic, un anneau dont l'alpha vaut encore vingt (l'effet 261 vit
  // trois cents tics).
  pooled.node.life_ticks = EffectNodeLifeFor(pattern.effect_id);
  return pooled;
}

}  // namespace ro::render
