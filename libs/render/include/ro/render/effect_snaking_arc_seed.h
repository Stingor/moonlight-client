#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include <glm/glm.hpp>

#include "ro/render/effect_node_pool_fwd.h"
#include "ro/render/effect_node_seed.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LE SEMIS DE L'ARC QUI SERPENTE, sous-type 70.
//
// UN SEUL bâtisseur et TROIS sites, un par identifiant. Chaque site lui passe
// un octet, et cet octet est le style que la figure lit pour sa teinte : les
// trois valent zéro, deux et huit, soit exactement les trois branches du
// dessinateur.
//
// ⚠ IL SÈME QUATRE FOIS, AUX QUATRE PREMIERS TICS. Son test n'est pas « au
// premier tic » mais « tant que le tic de l'instance ne dépasse pas trois » :
// quatre nœuds naissent, donc seize pistes. N'en semer qu'un donnerait le quart
// de l'orage.

// JUSQU'À QUEL TIC LE BÂTISSEUR SÈME.
inline constexpr std::int32_t kSnakingArcLastSowTick = 3;

// LE RAYON DU TUBE, le même pour les trois sites.
inline constexpr float kSnakingArcTubeRadius = 0.1F;

// CE QUE LE BÂTISSEUR TIRE, et entre quelles bornes : un retard, un alpha, un
// cap, et la longueur du premier segment.
inline constexpr std::int32_t kSnakingArcDelaySpread = 26;
inline constexpr std::int32_t kSnakingArcAlphaSpread = 51;
inline constexpr std::int16_t kSnakingArcAlphaFloor = 125;
inline constexpr std::int32_t kSnakingArcLengthSpread = 8;
inline constexpr float kSnakingArcLengthFloor = 6.0F;

// UN ORAGE qu'un effet lève.
struct EffectSnakingArcPattern {
  std::uint32_t effect_id = 0;
  std::string_view client_function;
  std::string_view texture;
  // L'OCTET DU SITE, que la figure lit comme son style de teinte.
  std::int16_t style = 0;
  EffectNodeSchedule schedule;
};

[[nodiscard]] std::span<const EffectSnakingArcPattern> EffectSnakingArcPatternTable();

[[nodiscard]] std::span<const EffectSnakingArcPattern> EffectSnakingArcPatternsFor(
    std::uint32_t effect_id);

// LES QUATRE PISTES d'un nœud, toutes vives et toutes en retard.
//
// ⚠ LEUR HISTORIQUE NAÎT VIDE : les vingt et une phases et les vingt et un
// rangs sont à zéro, si bien que les vingt segments se superposent à l'ancre.
// C'est le mouvement qui déplie l'arc, cran par cran.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedEffectSnakingArcTracks(
    const EffectSnakingArcPattern& pattern, glm::vec3 anchor, std::uint32_t& generator);

// LE NŒUD qu'un patron décrit, prêt à rejoindre la troupe.
[[nodiscard]] PooledEffectNode SowEffectSnakingArcNode(const EffectSnakingArcPattern& pattern,
                                                       glm::vec3 anchor,
                                                       std::int32_t effect_life_ticks,
                                                       std::uint32_t& generator);

}  // namespace ro::render
