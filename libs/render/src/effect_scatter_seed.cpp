#include "ro/render/effect_scatter_seed.h"

#include <algorithm>
#include <string>

#include "ro/render/effect_node.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_track.h"

namespace ro::render {
namespace {

// LES CASES QUE LE BÂTISSEUR REMPLIT. Les six premières sont TROIS COUPLES
// d'angles errants — un courant, puis sa cible —, que le mouvement rapproche
// d'un degré par tic. Voir `AdvanceScatteringParticleTracks`.
constexpr std::size_t kWanderPairCount = 3;
constexpr std::size_t kSlotOfFirstWander = 0;
constexpr std::size_t kSlotOfHeightGate = 6;
constexpr std::size_t kSlotOfStyle = 7;
constexpr std::size_t kSlotOfSpinDegrees = 8;
constexpr std::size_t kMarkOfTexture = 0;
constexpr std::size_t kMarkOfDrawing = 2;

// CE QUE LES QUATORZE STYLES PARTAGENT, écrit en dur dans le bâtisseur.
constexpr std::int16_t kStartingDegrees = 45;

// LA HAUTEUR AU-DESSUS DE LAQUELLE un style change de comportement, dans le
// repère du CLIENT, qui descend : cent cinquante-deux unités au-dessus de
// l'origine. Un seul style la consulte — celui dont l'alpha ne monte que si son
// effet se tient plus haut qu'elle —, et le mouvement y ramène la hauteur du
// nœud avant de comparer.
constexpr float kHeightGate = -152.0F;

// LA DURÉE DE VIE D'UNE PARTICULE, tirée entre ces bornes et rangée dans
// l'AZIMUT — qui n'est donc pas un angle ici. Le client tire un reste sur deux
// cents et y ajoute trois cents.
constexpr std::int32_t kShortestLifeTicks = 300;
constexpr std::int32_t kLifeSpreadTicks = 200;

// ⚠ LE COMPTEUR NAÎT À LA PLUS COURTE DES VIES, et c'est ce qui échelonne la
// nuée. Une particule dont la vie tirée vaut quatre cents reste donc cent tics
// sans rien montrer, puis meurt, renaît à zéro et vit alors normalement. Le
// poser à zéro ferait paraître les quatre ensemble.
//
// Le client ne l'écrit que si son effet se tient plus haut que la porte
// ci-dessus — ce qui est le cas de tout effet au-dessus du sol, et le parcours
// en avant ne peut pas trancher la comparaison.
constexpr std::int32_t kStartingTicks = kShortestLifeTicks;

// LA TAILLE D'UNE PARTICULE, et le style au sol a la sienne : le client tire
// « trente-cinq plus un reste sur dix » pour la brume, « trente plus un reste
// sur vingt » pour tout le reste.
constexpr std::uint8_t kStyleOnTheGround = 3;
constexpr float kSmallestGroundParticle = 35.0F;
constexpr std::int32_t kGroundSizeSpread = 10;
constexpr float kSmallestSkyParticle = 30.0F;
constexpr std::int32_t kSkySizeSpread = 20;

// Un tirage entier dans [0, bound), comme le modulo du client.
[[nodiscard]] std::int32_t DrawBelow(std::uint32_t& generator, std::int32_t bound) {
  return static_cast<std::int32_t>(NextEffectNodeDraw(generator) %
                                   static_cast<std::uint32_t>(bound));
}

}  // namespace

const EffectScatterStyleSeed* EffectScatterStyleSeedFor(std::uint8_t style) {
  const std::span<const EffectScatterStyleSeed> table = EffectScatterStyleSeedTable();
  const auto found = std::ranges::find(table, style, &EffectScatterStyleSeed::style);
  return found == table.end() ? nullptr : &*found;
}

std::array<EffectTrack, kEffectTracksPerNode> SeedEffectScatterTracks(
    const EffectScatterStyleSeed& seed, std::uint32_t& generator) {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  for (EffectTrack& track : tracks) {
    track.active = true;
    // L'alpha naît NUL : c'est le mouvement qui le monte, et rien ne se montre
    // avant lui.
    track.alpha = 0;
    track.degrees = kStartingDegrees;
    track.ticks = kStartingTicks;
    track.value[kSlotOfHeightGate] = kHeightGate;
    track.value[kSlotOfStyle] = static_cast<float>(seed.style);
    track.mark[kMarkOfTexture] = 0;
    track.mark[kMarkOfDrawing] = seed.draw_mark;

    // ⚠ CHAQUE PISTE TIRE SA PROPRE VIE, puis ses angles. L'ordre des tirages
    // est celui du client : le changer donnerait une autre nuée, également
    // plausible et différente de la sienne.
    track.azimuth_degrees =
        static_cast<std::int16_t>(kShortestLifeTicks + DrawBelow(generator, kLifeSpreadTicks));
    for (std::size_t pair = 0; pair < kWanderPairCount; ++pair) {
      const std::size_t current = kSlotOfFirstWander + 2 * pair;
      track.value[current] = static_cast<float>(DrawBelow(generator, kEffectTrackFullTurnDegrees));
      track.value[current + 1] =
          static_cast<float>(DrawBelow(generator, kEffectTrackFullTurnDegrees));
    }
    // 🔴 LA TAILLE, ET ELLE MANQUAIT. Sans elle le rayon vaut zéro, et la figure
    // compose quatre coins confondus : des quads bien comptés, bien mélangés,
    // et rigoureusement invisibles. C'est ce qui rendait muette toute la
    // famille — douze effets, dont la brume d'einbroch et les nuages de gonryun.
    //
    // ⚠ ELLE SE TIRE À LA PLACE QUE LE CLIENT LUI DONNE, entre les angles
    // errants et l'angle propre : chaque tirage consomme le générateur, et le
    // déplacer donnerait une autre nuée.
    track.radius_at_anchor =
        seed.style == kStyleOnTheGround
            ? kSmallestGroundParticle + static_cast<float>(DrawBelow(generator, kGroundSizeSpread))
            : kSmallestSkyParticle + static_cast<float>(DrawBelow(generator, kSkySizeSpread));
    track.value[kSlotOfSpinDegrees] =
        static_cast<float>(DrawBelow(generator, kEffectTrackFullTurnDegrees));
  }
  return tracks;
}

PooledEffectNode SowEffectScatterNode(const EffectScatterNodePattern& pattern, glm::vec3 anchor,
                                      std::int32_t effect_life_ticks, std::uint32_t& generator) {
  PooledEffectNode pooled;
  pooled.shape = PooledNodeShape::kScatterField;
  pooled.ground_position = anchor;
  pooled.node.life_ticks = effect_life_ticks;
  const EffectScatterStyleSeed* style_seed = EffectScatterStyleSeedFor(pattern.style);
  if (style_seed == nullptr) {
    return pooled;
  }
  for (const std::string_view path : EffectScatterStyleTextures(*style_seed)) {
    pooled.resource_paths.emplace_back(path);
  }
  pooled.tracks = SeedEffectScatterTracks(*style_seed, generator);
  return pooled;
}

}  // namespace ro::render
