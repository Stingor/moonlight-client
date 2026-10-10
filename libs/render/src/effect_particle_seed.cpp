#include "ro/render/effect_particle_seed.h"

#include <string>
#include <utility>

#include "ro/render/effect_node.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_render_word.h"
#include "ro/render/effect_track.h"

namespace ro::render {
namespace {

// CE QUE LE BÂTISSEUR ÉCRIT EN DUR, identique pour ses quatre pistes.

// Le rayon de départ de la sphère. Le pas dont il croît à chaque tic vient du
// patron : le bâtisseur le range en première case, et le mouvement l'y relit.
constexpr float kStartingRadius = 12.0F;

// LE RETARD DE CHAQUE PISTE, tiré au sort entre ces deux bornes et posé en
// NÉGATIF : tant que le compteur l'est, la piste attend.
constexpr std::int32_t kShortestDelayTicks = 18;
constexpr std::int32_t kDelaySpreadTicks = 30;

// Les cases où le bâtisseur range le pas de croissance, la demi-diagonale et
// l'angle de rotation ; et la marque qui porte le cas de couleur.
constexpr std::size_t kSlotOfRadiusGrowth = 0;
constexpr std::size_t kSlotOfHalfDiagonal = 1;
constexpr std::size_t kSlotOfSpinDegrees = 2;
constexpr std::size_t kMarkOfColorCase = 0;

// LE MOT DE RENDU que les deux bâtisseurs écrivent : celui du constructeur plus
// le mélange PROPRE — le seau garde celui de chaque particule. Voir
// `effect_render_word.h`.
constexpr std::uint32_t kRenderWordOfRisingParticles =
    kEffectRenderWordOfConstructor | kEffectRenderWordOwnBlend;

// Un tirage entier dans [0, bound), comme le modulo du client.
[[nodiscard]] std::int32_t DrawBelow(std::uint32_t& generator, std::int32_t bound) {
  return static_cast<std::int32_t>(NextEffectNodeDraw(generator) %
                                   static_cast<std::uint32_t>(bound));
}

}  // namespace

std::array<EffectTrack, kEffectTracksPerNode> SeedEffectRisingParticleTracks(
    float half_diagonal, std::uint8_t color_case, glm::vec3 anchor, std::uint32_t& generator,
    float radius_growth_per_tick) {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  for (EffectTrack& track : tracks) {
    track.active = true;
    track.anchor = anchor;
    track.alpha = 0;
    track.radius_at_anchor = kStartingRadius;
    track.value[kSlotOfRadiusGrowth] = radius_growth_per_tick;
    track.value[kSlotOfHalfDiagonal] = half_diagonal;
    track.mark[kMarkOfColorCase] = color_case;

    track.ticks = -kShortestDelayTicks - DrawBelow(generator, kDelaySpreadTicks);
    track.tilt_degrees =
        static_cast<std::int16_t>(DrawBelow(generator, kEffectTrackFullTurnDegrees));
    track.degrees = static_cast<std::int16_t>(DrawBelow(generator, kEffectTrackFullTurnDegrees));
    track.azimuth_degrees =
        static_cast<std::int16_t>(DrawBelow(generator, kEffectTrackFullTurnDegrees));
    track.value[kSlotOfSpinDegrees] =
        static_cast<float>(DrawBelow(generator, kEffectTrackFullTurnDegrees));
  }
  return tracks;
}

PooledEffectNode SowEffectRisingParticleNode(const EffectRisingParticlePattern& pattern,
                                             glm::vec3 anchor, std::uint32_t& generator) {
  PooledEffectNode pooled;
  pooled.shape = PooledNodeShape::kRisingParticles;
  pooled.ground_position = anchor;
  pooled.resource_paths = {std::string(pattern.texture)};
  pooled.node.life_ticks = kEffectRisingParticleLifeTicks;
  pooled.node.render_word = kRenderWordOfRisingParticles;
  pooled.tracks = SeedEffectRisingParticleTracks(pattern.half_diagonal, pattern.color_case, anchor,
                                                 generator, pattern.radius_growth_per_tick);
  return pooled;
}

}  // namespace ro::render
