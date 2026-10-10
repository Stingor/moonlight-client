#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include <glm/glm.hpp>

#include "ro/render/effect_node_pool_fwd.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LES ANNEAUX EN GERBE : le semis et le mouvement du sous-type 107.
//
// UN identifiant — 487, la gerbe noire qu'un rang du Diable pose au sol —, et
// DEUX nœuds que son bâtisseur sème à un tic d'écart, chacun de quatre pistes
// décalées d'un quart de tour et d'un quart de cycle. Le second nœud part un
// degré et deux dixièmes de cycle plus loin que le premier.
//
// ⚠ SA FIGURE EST DÉJÀ PORTÉE : c'est la jupe d'anneau, `Ring_0`, celle du
// quatuor (sous-type 41). Ce qui manquait n'était pas un dessinateur mais un
// MOUVEMENT : ici le cercle intérieur s'élargit d'un dixième par tic, et le
// ruban s'ouvre puis se referme sur un cycle de dix unités pendant que la jupe
// se couche.

inline constexpr std::int16_t kRingStarburstFullSweep = 360;
inline constexpr std::int16_t kRingStarburstDegreesPerTrack = 90;
// D'UN QUART DE CYCLE l'une après l'autre : les quatre rubans ne s'ouvrent pas
// ensemble.
inline constexpr float kRingStarburstCyclePerTrack = 2.5F;
// LE CYCLE : le cercle grandit d'un dixième par tic depuis trois unités ; à
// treize, il repart de trois. Le ruban s'ouvre sur la première moitié du cycle
// et se referme sur la seconde.
inline constexpr float kRingStarburstGrowthPerTick = 0.1F;
inline constexpr float kRingStarburstInnerRadius = 3.0F;
inline constexpr float kRingStarburstOuterRadius = 13.0F;
inline constexpr float kRingStarburstHalfCycle = 5.0F;
inline constexpr float kRingStarburstFullCycle = 10.0F;
// L'ALPHA MONTE DE DEUX PAR TIC tant que le ruban s'ouvre, et retombe de deux
// tant qu'il se referme.
inline constexpr std::int16_t kRingStarburstAlphaStep = 2;
// Une piste qui boucle son cycle après le cinquantième tic s'éteint.
inline constexpr std::int32_t kRingStarburstDiesAfterTick = 50;
// LA JUPE SE COUCHE : droite quand le cercle repart, cinq degrés de moins par
// unité gagnée.
inline constexpr float kRingStarburstUprightDegrees = 90.0F;
inline constexpr float kRingStarburstTiltPerUnit = 5.0F;
// LE RUBAN EST DEUX FOIS LARGE comme la part du cycle parcourue.
inline constexpr float kRingStarburstWidthPerUnit = 2.0F;

// UNE GERBE D'ANNEAUX qu'un effet sème : un nœud, à son tic.
struct EffectRingStarburstPattern {
  std::uint32_t effect_id = 0;
  std::string_view client_function;
  std::string_view texture;
  // LE TIC DU SEMIS : le bâtisseur sème son premier nœud au tic un, le second au
  // tic deux.
  std::int32_t sown_at_tick = 1;
  // OÙ LE CYCLE DE LA PREMIÈRE PISTE COMMENCE ; chacune gagne un quart de cycle.
  float first_cycle = 2.5F;
  // L'AZIMUT DE LA PREMIÈRE PISTE, en degrés ; chacune gagne un quart de tour.
  std::int16_t first_azimuth_degrees = 270;
  // LES INCLINAISONS DU SEMIS, que le premier mouvement remplace avant que le
  // ruban n'ait de largeur.
  std::array<std::int16_t, kEffectTracksPerNode> seed_tilt_degrees = {53, 60, 55, 50};
  // LA VARIANTE DE TEINTE DU RUBAN : douze, un gris sombre mélangé par son alpha.
  float trail_variant = 12.0F;
};

[[nodiscard]] std::span<const EffectRingStarburstPattern> EffectRingStarburstPatternTable();

[[nodiscard]] std::span<const EffectRingStarburstPattern> EffectRingStarburstPatternsFor(
    std::uint32_t effect_id);

// LE DERNIER TIC où cet effet sème une gerbe, zéro sans gerbe.
[[nodiscard]] std::int32_t EffectRingStarburstLastSowTick(std::uint32_t effect_id);

[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedEffectRingStarburstTracks(
    const EffectRingStarburstPattern& pattern);

[[nodiscard]] PooledEffectNode SowEffectRingStarburstNode(const EffectRingStarburstPattern& pattern,
                                                          glm::vec3 anchor,
                                                          std::int32_t effect_life_ticks);

// LE MOUVEMENT DU SOUS-TYPE 107.
//
// ⚠ LES VINGT ET UNE LONGUEURS REÇOIVENT LA MÊME VALEUR, celle des segments
// dont la marque est nulle — et le bâtisseur les met toutes à zéro. La jupe est
// UNIFORME.
void AdvanceRingStarburstTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks);

}  // namespace ro::render
