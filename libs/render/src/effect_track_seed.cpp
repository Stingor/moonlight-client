#include "ro/render/effect_track_seed.h"

#include <string>
#include <utility>

#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_track.h"

namespace ro::render {
namespace {

// CE QUE LE BÂTISSEUR ÉCRIT EN DUR, identique pour les vingt-trois identifiants
// du sous-type 30 ; ce qui dépend du style est en table engendrée, jamais ici.

// Le tour entier : les quatre anneaux se referment.
constexpr std::int16_t kRingSweepDegrees = 360;

// L'inclinaison de départ, forte — c'est elle qui fait la jupe, et le mouvement
// la couche d'un degré par tic jusqu'à son plancher.
constexpr std::int16_t kRingTiltDegrees = 80;

// Le rayon du cercle intérieur à la naissance.
constexpr float kRingRadiusAtAnchor = 4.0999999F;

// Où chaque piste commence sur le cercle. Ce n'est pas un pas régulier : le
// bâtisseur pose ces quatre valeurs une à une.
constexpr std::array<std::int16_t, kEffectTracksPerNode> kRingAzimuthDegrees = {180, 270, 0, 90};

// Ce que chaque piste ajoute à l'alpha de départ. Les quatre rubans ne sont
// donc jamais à la même transparence, et c'est ce qui leur donne de la
// profondeur là où le client ne dessine qu'un anneau.
constexpr std::array<std::int16_t, kEffectTracksPerNode> kRingAlphaAboveStart = {135, 90, 45, 0};

// La vie d'instance au-delà de laquelle les pistes naissent visibles. En deçà,
// le bâtisseur les éteint et les retarde.
constexpr std::int32_t kLifeTicksThatLightTheTracks = 60;

// Le retard que chaque piste prend sur la précédente, en tics, quand le semis
// naît éteint.
constexpr std::int32_t kDelayTicksPerRank = -5;

}  // namespace

std::array<EffectTrack, kEffectTracksPerNode> SeedEffectRingTracks(std::int16_t start_alpha,
                                                                   std::uint8_t style,
                                                                   std::int32_t effect_life_ticks) {
  const EffectRingStyle choisi = EffectRingStyleFor(style);
  const bool nait_visible = effect_life_ticks > kLifeTicksThatLightTheTracks;

  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  for (std::size_t rang = 0; rang < kEffectTracksPerNode; ++rang) {
    EffectTrack& track = tracks[rang];
    track.active = true;
    track.style = static_cast<std::int16_t>(style);
    track.degrees = kRingSweepDegrees;
    track.tilt_degrees = kRingTiltDegrees;
    track.radius_at_anchor = kRingRadiusAtAnchor;
    track.azimuth_degrees = kRingAzimuthDegrees[rang];
    track.lift_key = choisi.lift_key[rang];
    track.alpha =
        nait_visible ? static_cast<std::int16_t>(start_alpha + kRingAlphaAboveStart[rang]) : 0;
    track.ticks = nait_visible ? 0 : static_cast<std::int32_t>(rang) * kDelayTicksPerRank;
  }
  return tracks;
}

PooledEffectNode SowEffectRingNode(const EffectRingNodePattern& pattern, glm::vec3 ground_position,
                                   std::int32_t instance_life_ticks) {
  PooledEffectNode pooled;
  pooled.shape = PooledNodeShape::kRingTracks;
  pooled.ground_position = ground_position;
  pooled.resource_paths = {std::string(pattern.texture)};
  pooled.trail_variant = EffectRingStyleFor(pattern.style).trail_variant;
  // LA VIE DU NŒUD EST CELLE DE L'INSTANCE, relevée au plancher, et le
  // bâtisseur la lui recopie. C'est elle qui décide du rallumage : une piste
  // éteinte ne repart que s'il lui reste assez de temps pour s'ouvrir. Et
  // c'est elle aussi qui décide si les pistes naissent éteintes.
  const std::int32_t life = EffectRingInstanceLifeTicks(instance_life_ticks);
  pooled.node.life_ticks = life;
  pooled.tracks = SeedEffectRingTracks(pattern.start_alpha, pattern.style, life);
  return pooled;
}

}  // namespace ro::render
