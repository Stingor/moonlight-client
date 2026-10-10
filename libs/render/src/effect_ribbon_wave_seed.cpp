#include "ro/render/effect_ribbon_wave_seed.h"

#include <string>

#include "ro/render/effect_life_table.h"
#include "ro/render/effect_node.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_track.h"

namespace ro::render {
namespace {

// CE QUE LE BÂTISSEUR ÉCRIT EN DUR dans son unique piste vive.

// LE BALAYAGE FAIT LE TOUR : l'anneau est fermé, et sa figure referme
// exactement le cercle à son dernier segment.
constexpr std::int16_t kSweepDegrees = 360;
// LA CLÉ, que la figure du ruban prend telle quelle comme ordonnée de ses deux
// bords, et que le mouvement prend comme AMPLITUDE de son onde. Deux sens pour
// un champ, et c'est l'ADR 0017 : chacun appartient à qui le lit.
constexpr float kLiftKey = 15.0F;
constexpr float kStartingRadius = 2.6F;
constexpr std::int16_t kStartingTilt = 80;

// ⚠ L'ALPHA DE DÉPART SE TIRE SUR CENT, et c'est ce qui décale le cycle d'un
// anneau à l'autre. Le mouvement le fait descendre de deux par tic jusqu'à
// zéro, puis tout renaît : deux anneaux semés au même alpha respireraient d'un
// seul cœur.
constexpr std::int32_t kStartingAlphaBound = 100;

// L'UNIQUE PISTE VIVE : le bâtisseur éteint les quatre puis rallume la
// première, comme celui du carré teinté.
constexpr std::size_t kLivingTrack = 0;

[[nodiscard]] std::int32_t DrawBelow(std::uint32_t& generator, std::int32_t bound) {
  return static_cast<std::int32_t>(NextEffectNodeDraw(generator) %
                                   static_cast<std::uint32_t>(bound));
}

}  // namespace

std::array<EffectTrack, kEffectTracksPerNode> SeedEffectRibbonWaveTracks(std::uint32_t& generator) {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  EffectTrack& living = tracks[kLivingTrack];
  living.active = true;
  living.ticks = 0;
  living.degrees = kSweepDegrees;
  living.lift_key = kLiftKey;
  living.radius_at_anchor = kStartingRadius;
  living.tilt_degrees = kStartingTilt;

  // L'ORDRE DES TIRAGES EST CELUI DU CLIENT : l'azimut, puis l'alpha.
  living.azimuth_degrees =
      static_cast<std::int16_t>(DrawBelow(generator, kEffectTrackFullTurnDegrees));
  living.alpha = static_cast<std::int16_t>(DrawBelow(generator, kStartingAlphaBound));

  // Les longueurs et les marques naissent NULLES : c'est le mouvement qui les
  // écrit, dès son premier tic. Un ruban semé avec des longueurs inventées
  // montrerait une forme que le client ne montre jamais.
  return tracks;
}

PooledEffectNode SowEffectRibbonWaveNode(const EffectRibbonWavePattern& pattern,
                                         glm::vec3 ground_position, std::uint32_t& generator) {
  PooledEffectNode pooled;
  pooled.shape = PooledNodeShape::kRibbonWave;
  pooled.ground_position = ground_position;
  pooled.resource_paths = {std::string(pattern.texture)};
  pooled.trail_variant = pattern.trail_variant;
  // 🔴 LA VIE EST CELLE DE L'INSTANCE, que le bâtisseur recopie et que la
  // table des durées porte (ADR 0042) : vingt-neuf mille tics. Le défaut de
  // cent tics éteindrait au bout d'une seconde et demie un anneau que le
  // mouvement rallume sans fin.
  pooled.node.life_ticks = EffectNodeLifeFor(pattern.effect_id);
  // Le nœud naît là où il est semé ; c'est `app` qui dira où son porteur s'est
  // rendu, et le mouvement l'y suivra.
  pooled.owner_position = ground_position;
  pooled.tracks = SeedEffectRibbonWaveTracks(generator);
  return pooled;
}

}  // namespace ro::render
