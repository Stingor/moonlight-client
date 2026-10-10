#include "ro/render/effect_rain_particle_seed.h"

#include <algorithm>
#include <string>

#include "ro/render/effect_node.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_rain_particle.h"
#include "ro/render/effect_track.h"

namespace ro::render {

std::span<const EffectRainParticlePattern> EffectRainParticlePatternsFor(std::uint32_t effect_id) {
  const std::span<const EffectRainParticlePattern> table = EffectRainParticlePatternTable();
  const auto first =
      std::ranges::lower_bound(table, effect_id, {}, &EffectRainParticlePattern::effect_id);
  const auto last =
      std::ranges::upper_bound(table, effect_id, {}, &EffectRainParticlePattern::effect_id);
  return table.subspan(static_cast<std::size_t>(first - table.begin()),
                       static_cast<std::size_t>(last - first));
}

std::array<EffectTrack, kEffectTracksPerNode> SeedEffectRainParticleTracks(
    std::uint32_t& generator) {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  // ⚠ LES QUATRE PISTES SONT VIVES, et chacune tire ses six nombres pour elle
  // seule : c'est ce qui disperse les quatre grains d'une même bouffée. Les
  // tirer une fois pour les quatre en ferait une colonne.
  for (EffectTrack& track : tracks) {
    track.active = true;
    track.ticks = 0;
    // ⚠ L'ALPHA PART À ZÉRO : rien ne se voit tant que le mouvement n'a pas
    // tourné, et c'est lui qui allume le grain en neuf tics.
    track.alpha = 0;
    track.lift_key = kRainParticleLiftFloor +
                     static_cast<float>(NextEffectNodeDraw(generator) % kRainParticleLiftSpread) *
                         kRainParticleLiftStep;
    track.radius_at_anchor =
        kRainParticleRadiusFloor +
        static_cast<float>(NextEffectNodeDraw(generator) % kRainParticleRadiusSpread) *
            kRainParticleRadiusStep;
    track.degrees = static_cast<std::int16_t>(
        NextEffectNodeDraw(generator) % static_cast<std::uint32_t>(kEffectTrackFullTurnDegrees));
    track.value[kSlotOfRainParticleOrbitRadius] =
        static_cast<float>(NextEffectNodeDraw(generator) % kRainParticleOrbitSpread);
    // ⚠ SEULE LA PROFONDEUR SE SÈME. L'abscisse et la cote restent nulles ici :
    // le mouvement les recalcule au premier tic depuis l'azimut et le rayon de
    // disque. Les poser ici serait les poser deux fois, et se tromper une fois.
    track.anchor.y = kRainParticleDepthFloor +
                     static_cast<float>(NextEffectNodeDraw(generator) % kRainParticleDepthSpread);
    track.mark[kMarkOfRainParticleTint] =
        static_cast<std::uint8_t>(NextEffectNodeDraw(generator) % kRainParticleTintSpread);
  }
  return tracks;
}

PooledEffectNode SowEffectRainParticleNode(const EffectRainParticlePattern& pattern,
                                           glm::vec3 anchor, std::int32_t effect_life_ticks,
                                           std::uint32_t& generator) {
  PooledEffectNode pooled;
  pooled.shape = PooledNodeShape::kRainParticle;
  pooled.ground_position = anchor;
  pooled.owner_position = anchor;
  pooled.node.life_ticks = effect_life_ticks;
  pooled.resource_paths = {std::string(pattern.texture)};
  pooled.tracks = SeedEffectRainParticleTracks(generator);
  return pooled;
}

}  // namespace ro::render
