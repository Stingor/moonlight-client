#include "ro/render/effect_swirl_trail_seed.h"

#include <algorithm>
#include <cstddef>
#include <string>

#include "ro/render/effect_node.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_swirl_trail.h"
#include "ro/render/effect_track.h"

#include "effect_track_angles.h"

namespace ro::render {
namespace {

// LA MENEUSE, seule à recevoir autre chose que son allumage.
constexpr std::size_t kLeadTrack = 0;

// LA MARQUE DE TEINTE que le carré teinté lit sur la meneuse (zéro) : le blanc,
// en mélange ordinaire. Voir `EffectTintedQuadStyleFor`.
constexpr std::size_t kMarkOfTint = 2;
constexpr std::uint8_t kWhiteOrdinaryTint = 0;

}  // namespace

std::span<const EffectSwirlTrailPattern> EffectSwirlTrailPatternsFor(std::uint32_t effect_id) {
  const std::span<const EffectSwirlTrailPattern> table = EffectSwirlTrailPatternTable();
  const auto first =
      std::ranges::lower_bound(table, effect_id, {}, &EffectSwirlTrailPattern::effect_id);
  const auto last =
      std::ranges::upper_bound(table, effect_id, {}, &EffectSwirlTrailPattern::effect_id);
  return table.subspan(static_cast<std::size_t>(first - table.begin()),
                       static_cast<std::size_t>(last - first));
}

std::array<EffectTrack, kEffectTracksPerNode> SeedEffectSwirlTrailTracks(glm::vec3 owner_position,
                                                                         std::uint32_t& generator) {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  // ⚠ LES QUATRE PISTES SONT ALLUMÉES, et les trois échos à alpha nul : ils
  // n'ont rien à montrer avant leur première recopie.
  for (EffectTrack& track : tracks) {
    track.active = true;
    track.alpha = 0;
  }

  EffectTrack& leader = tracks[kLeadTrack];
  leader.ticks = 0;
  leader.radius_at_anchor = kSwirlTrailRadius;
  // L'angle propre, nul : le mouvement le recopie sans
  // l'avancer, si bien que le rayon ne respire jamais.
  leader.value[kSwirlTrailSlotOfSpinDegrees] = 0.0F;
  leader.alpha = kSwirlTrailAlphaAtBirth;
  leader.azimuth_degrees =
      static_cast<std::int16_t>(NextEffectNodeDraw(generator) % kEffectTrackFullTurnDegrees);
  leader.value[kSwirlTrailSlotOfSpeed] = kSwirlTrailSpeed;
  // L'angle du carré, nul : c'est lui que le mouvement fait tourner.
  leader.degrees = 0;
  leader.mark[kSwirlTrailMarkOfTexture] = 0;
  leader.mark[kMarkOfTint] = kWhiteOrdinaryTint;

  // L'ANCRE EST UN POINT DU MONDE : la place du porteur, relevée de huit, puis
  // reculée de quinze pas contre le cap. Même cosinus et même sinus que le
  // mouvement, qui la fera avancer d'un pas par tic.
  const float recul = kSwirlTrailSpeed * kSwirlTrailBackStepCount;
  leader.anchor = {owner_position.x - recul * CosOfDegrees(leader.azimuth_degrees),
                   owner_position.y + kSwirlTrailLiftAboveOwner,
                   owner_position.z - recul * SinOfDegrees(leader.azimuth_degrees)};
  return tracks;
}

PooledEffectNode SowEffectSwirlTrailNode(const EffectSwirlTrailPattern& pattern,
                                         glm::vec3 owner_position, std::int32_t life_ticks,
                                         std::uint32_t& generator) {
  PooledEffectNode pooled;
  pooled.shape = PooledNodeShape::kSwirlTrail;
  // ⚠ LA PLACE DU NŒUD N'ENTRE DANS AUCUN COIN : le carré teinté pose ses
  // coins sur l'ancre de la piste, déjà dans le monde. Le mouvement du client
  // y recopie pourtant le porteur à chaque tic — une écriture que ni ce
  // mouvement ni la figure ne relisent.
  pooled.ground_position = owner_position;
  pooled.owner_position = owner_position;
  pooled.node.life_ticks = std::max(life_ticks, 1);
  pooled.node.render_word = kSwirlTrailRenderWord;
  pooled.resource_paths = {std::string(pattern.texture)};
  pooled.tracks = SeedEffectSwirlTrailTracks(owner_position, generator);
  return pooled;
}

}  // namespace ro::render
