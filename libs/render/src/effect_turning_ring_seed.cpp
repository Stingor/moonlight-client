#include "ro/render/effect_turning_ring_seed.h"

#include <string>

#include "ro/render/effect_life_table.h"
#include "ro/render/effect_node.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_track.h"

namespace ro::render {
namespace {

// CE QUE LE BÂTISSEUR ÉCRIT EN DUR dans son unique piste vive.

// ⚠ LE BALAYAGE NE FAIT PAS LE TOUR, à la différence de celui du frère : trois
// cent quinze degrés. L'anneau est donc OUVERT d'un huitième, et le refermer
// lui ôterait sa fente.
constexpr std::int16_t kSweepDegrees = 315;
constexpr std::int16_t kStartingTilt = 90;

// LA CLÉ, que la figure prend comme ordonnée de ses bords et le mouvement comme
// amplitude de son onde. Vingt-cinq plus un tirage sur dix.
constexpr float kLowestLiftKey = 25.0F;
constexpr std::int32_t kLiftKeyDrawBound = 10;

// L'ALPHA part HAUT et ne bouge plus : ce mouvement-ci n'y touche pas. Cinquante
// moins un tirage sur dix, soit de quarante et un à cinquante.
constexpr std::int16_t kHighestAlpha = 50;
constexpr std::int32_t kAlphaDrawBound = 10;

// L'UNIQUE PISTE VIVE : le bâtisseur éteint les quatre puis rallume la première.
constexpr std::size_t kLivingTrack = 0;

[[nodiscard]] std::int32_t DrawBelow(std::uint32_t& generator, std::int32_t bound) {
  return static_cast<std::int32_t>(NextEffectNodeDraw(generator) %
                                   static_cast<std::uint32_t>(bound));
}

}  // namespace

std::array<EffectTrack, kEffectTracksPerNode> SeedEffectTurningRingTracks(
    std::uint32_t& generator) {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  EffectTrack& living = tracks[kLivingTrack];
  living.active = true;
  living.ticks = 0;
  living.degrees = kSweepDegrees;
  living.tilt_degrees = kStartingTilt;

  // L'ORDRE DES TIRAGES EST CELUI DU CLIENT : la clé, l'azimut, l'alpha, le
  // style. Les intervertir donnerait un autre anneau à la même graine.
  living.lift_key = kLowestLiftKey + static_cast<float>(DrawBelow(generator, kLiftKeyDrawBound));
  living.azimuth_degrees =
      static_cast<std::int16_t>(DrawBelow(generator, kEffectTrackFullTurnDegrees));
  living.alpha = static_cast<std::int16_t>(kHighestAlpha - DrawBelow(generator, kAlphaDrawBound));

  // ⚠ LE STYLE EST LE COMPTEUR DU POULS, et il se TIRE. C'est lui que le
  // mouvement fait avancer d'un degré par tic pour en prendre le sinus : deux
  // anneaux semés ensemble ne pulsent donc pas d'accord. Le poser à zéro les
  // ferait battre d'un seul cœur. Voir `AdvanceTurningRingTracks`.
  living.style = static_cast<std::int16_t>(DrawBelow(generator, kEffectTrackFullTurnDegrees));

  // ⚠ ET LE RAYON N'EST PAS POSÉ. Le mouvement l'écrit dès son premier tic, en
  // sinus du style ; lui donner une valeur de naissance inventerait une taille
  // d'un tic que le client ne montre jamais.
  return tracks;
}

PooledEffectNode SowEffectTurningRingNode(const EffectTurningRingPattern& pattern,
                                          glm::vec3 ground_position, std::uint32_t& generator) {
  PooledEffectNode pooled;
  pooled.shape = PooledNodeShape::kTurningRing;
  pooled.ground_position = ground_position;
  pooled.resource_paths = {std::string(pattern.texture)};
  pooled.trail_variant = pattern.trail_variant;
  // 🔴 LA VIE EST CELLE DE L'INSTANCE, que le bâtisseur recopie
  // et que la table des durées porte depuis l'ADR 0042 : vingt-neuf mille
  // tics. Un défaut de cent tics éteindrait l'anneau qui tourne bien avant.
  pooled.node.life_ticks = EffectNodeLifeFor(pattern.effect_id);
  pooled.owner_position = ground_position;
  pooled.tracks = SeedEffectTurningRingTracks(generator);
  return pooled;
}

}  // namespace ro::render
