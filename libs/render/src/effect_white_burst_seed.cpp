#include "ro/render/effect_white_burst_seed.h"

#include <string>

#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_track.h"

#include "effect_draw_below.h"

namespace ro::render {
namespace {

// CE QUE LE BÂTISSEUR ÉCRIT EN DUR, identique pour ses quatre pistes.
//
// ⚠ LE RAYON DE DÉPART EST CELUI DE L'AÎNÉ, la croissance NON. Douze et douze,
// mais un et demi contre un : les deux bâtisseurs partagent le premier nombre
// et pas le second, et les prendre en bloc aurait fait monter cette bouffée-ci
// d'un tiers trop lentement.
constexpr float kStartingRadius = 12.0F;
constexpr float kRadiusGrowthPerTick = 1.5F;

// Les cases où le bâtisseur range le pas de croissance, la demi-diagonale et
// l'angle de rotation ; et la marque qui porte le cas de couleur. Ce sont
// celles que la figure et le mouvement du sous-type 43 relisent.
constexpr std::size_t kSlotOfRadiusGrowth = 0;
constexpr std::size_t kSlotOfHalfDiagonal = 1;
constexpr std::size_t kSlotOfSpinDegrees = 2;
constexpr std::size_t kMarkOfColorCase = 0;

// UN TIRAGE QUI N'A QU'UNE ISSUE N'EST PAS UN TIRAGE. Le client écrit alors sa
// constante sans toucher au générateur, et l'imiter est la seule façon de garder
// les tirages suivants alignés sur les siens.
[[nodiscard]] std::int32_t DrawBelowOrNothing(std::uint32_t& generator, std::int32_t bound) {
  return bound > 1 ? DrawBelow(generator, bound) : 0;
}

}  // namespace

std::array<EffectTrack, kEffectTracksPerNode> SeedEffectWhiteBurstTracks(
    const EffectWhiteBurstPattern& pattern, glm::vec3 anchor, std::uint32_t& generator) {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  for (EffectTrack& track : tracks) {
    track.active = true;
    track.anchor = anchor;
    track.alpha = 0;
    track.radius_at_anchor = kStartingRadius;
    track.value[kSlotOfRadiusGrowth] = kRadiusGrowthPerTick;
    track.value[kSlotOfHalfDiagonal] = pattern.half_diagonal;

    // L'ORDRE DES TIRAGES EST CELUI DU CLIENT : le retard, la
    // marque quand elle se tire, puis les quatre angles. Les intervertir
    // donnerait quatre carrés ailleurs.
    track.ticks = -pattern.shortest_delay_ticks -
                  pattern.delay_step_ticks * DrawBelow(generator, pattern.delay_draw_bound);
    track.mark[kMarkOfColorCase] = static_cast<std::uint8_t>(
        pattern.lowest_mark + DrawBelowOrNothing(generator, pattern.mark_draw_bound));

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

PooledEffectNode SowEffectWhiteBurstNode(const EffectWhiteBurstPattern& pattern, glm::vec3 anchor,
                                         std::uint32_t& generator) {
  PooledEffectNode pooled;
  // LA FORME EST CELLE DE LA PARTICULE MONTANTE, et ce n'est pas un raccourci :
  // c'est le même sous-type, donc la même figure et le même mouvement. Lui en
  // donner une à elle aurait obligé à recopier les deux.
  pooled.shape = PooledNodeShape::kRisingParticles;
  pooled.ground_position = anchor;
  pooled.resource_paths = {std::string(pattern.texture)};
  pooled.node.life_ticks = pattern.life_ticks;
  pooled.tracks = SeedEffectWhiteBurstTracks(pattern, anchor, generator);
  return pooled;
}

}  // namespace ro::render
