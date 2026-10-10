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

// LE SEMIS DES PÉTALES QUI S'ÉCARTENT, sous-type 94.
//
// Un seul bâtisseur, que huit sites appellent — quatre pour l'effet 411, quatre
// pour l'effet 1150. Chaque effet l'appelle dans une boucle de QUATRE tours dont
// le tour donne le quadrant : zéro, 90, 180 et 270 degrés. Seize nœuds par effet, à quatre images —
// `peong1`, `peong2`, `peong3` puis de nouveau `peong2` —, chacun à quatre pistes vives.
//
// ⚠ LE MÊME CAS BÂTIT D'ABORD TRENTE GRAINS QUI DÉRIVENT (sous-type 96), portés à
// part — voir `effect_drifting_mote_seed.h`.
//
// ⚠ LE TROISIÈME ARGUMENT EST LE TIC DU SEMIS, par la même règle que le grain :
// zéro sème au tic TRENTE-CINQ, un au tic ZÉRO, toute autre valeur à ce tic
// même. Le bâtisseur est rappelé à chaque tic et ne sème qu'à l'égalité avec
// l'âge de l'instance.
//
// ⚠ LA POSITION NE SUIT PAS LE PORTEUR : le nœud naît sur l'instance avec un écart
// nul, et son mouvement ne recopie jamais la place du
// propriétaire.

// LE TIC DU SEMIS par l'argument du site, et ses deux exceptions.
inline constexpr std::int16_t kRadialPetalLateVariant = 0;
inline constexpr std::int16_t kRadialPetalAtOnceVariant = 1;
inline constexpr std::int32_t kRadialPetalLateSowsAtTick = 35;
[[nodiscard]] std::int32_t RadialPetalSowTick(std::int16_t variant);

// LE MOT DE RENDU écrit en dur, cinq.
inline constexpr std::uint32_t kRadialPetalRenderWord = 5;

// CE QUE LE BÂTISSEUR TIRE POUR CHAQUE PISTE, dans l'ordre de ses `rand()` :
//   - la demi-diagonale : `rand() % 11 · 0,2 + 2,5`, de deux et demi à quatre
//     et demi ;
//   - le cap : l'argument du site plus `rand() % 90`, replié une fois sur le
//     tour — un pétale reste dans son quadrant ;
//   - le retard : `−(rand() % 11)` ;
//   - quatre angles `rand() % 360`, que ni le mouvement ni la figure ne lisent.
inline constexpr std::int32_t kRadialPetalSizeSpread = 11;
inline constexpr float kRadialPetalSizeStep = 0.2F;
inline constexpr float kRadialPetalSizeFloor = 2.5F;
inline constexpr std::int32_t kRadialPetalHeadingSpread = 90;
inline constexpr std::int32_t kRadialPetalDelaySpread = 11;
inline constexpr std::size_t kRadialPetalUnreadAngles = 4;

// CE QU'IL POSE EN DUR :
//   - la vitesse, 0,03 ;
//   - le cercle de départ : la piste naît à QUATRE unités du nœud, le long de
//     son cap (cos · 4, sin · 4), à hauteur nulle ;
//   - la hauteur de base du saut, −4,0 en repère du client : QUATRE unités
//     AU-DESSUS du nœud ;
//   - l'alpha nul : c'est le mouvement qui l'allume.
inline constexpr float kRadialPetalSpeed = 0.03F;
inline constexpr float kRadialPetalStartRing = 4.0F;
inline constexpr float kRadialPetalBaseDepthBelow = -4.0F;

// UN SITE, pour UN quadrant : un nœud.
struct EffectRadialPetalPattern {
  std::uint32_t effect_id = 0;
  std::string_view client_function;
  std::string_view texture;
  // LE DEUXIÈME ARGUMENT : le quadrant, en degrés.
  std::int16_t base_heading_degrees = 0;
  // LE TROISIÈME : zéro ou un, qui choisit le tic du semis.
  std::int16_t variant = 0;
  // QUAND ce patron sème, en tics de l'instance.
  EffectNodeSchedule schedule;
};

// Tous les patrons, par identifiant croissant puis dans l'ordre des appels.
[[nodiscard]] std::span<const EffectRadialPetalPattern> EffectRadialPetalPatternTable();

// Les patrons d'un effet, ou un span VIDE.
[[nodiscard]] std::span<const EffectRadialPetalPattern> EffectRadialPetalPatternsFor(
    std::uint32_t effect_id);

// LES QUATRE PISTES d'un nœud, toutes vives.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedRadialPetalTracks(
    std::int16_t base_heading_degrees, std::uint32_t& generator);

// LE NŒUD qu'un patron décrit.
//
// ⚠ LA VIE EST CELLE DE L'INSTANCE, et elle
// court depuis la naissance du nœud.
[[nodiscard]] PooledEffectNode SowRadialPetalNode(const EffectRadialPetalPattern& pattern,
                                                  glm::vec3 anchor, std::int32_t effect_life_ticks,
                                                  std::uint32_t& generator);

}  // namespace ro::render
