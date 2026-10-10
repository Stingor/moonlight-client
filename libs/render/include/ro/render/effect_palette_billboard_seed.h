#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include <glm/glm.hpp>

#include "ro/render/effect_node_pool_fwd.h"
#include "ro/render/effect_node_seed.h"
#include "ro/render/effect_palette_billboard.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LE SEMIS DU BILLBOARD DE PALETTE, et les effets qui l'emploient par le
// sous-type 132.
//
// Deux bâtisseurs jumeaux, une image par site et un octet — l'octet est la
// PALETTE que la figure relit dans la piste : de deux à huit pour les sept
// éclats de gel, zéro pour le cercle de 1122, qui ne s'éteint donc jamais. Les
// deux bâtisseurs ne diffèrent que par l'alpha semé.
//
// ⚠ LA NUANCE EST LE RANG DE LA PISTE, pas un paramètre. Le bâtisseur y écrit
// zéro, un, deux et trois, si bien que les quatre nuances d'une palette
// paraissent ENSEMBLE sur un même nœud. Les semer identiques montrerait un seul
// éclat quatre fois.
//
// ⚠ ET LES QUATRE PISTES SONT RETARDÉES l'une après l'autre : leurs compteurs
// partent à zéro, moins vingt, moins quarante et moins soixante. Trois sur
// quatre attendent donc leur tour avant de paraître, et c'est cet échelonnement
// qui fait la spirale.
//
// ⚠ ET LE BÂTISSEUR SÈME DEUX FOIS, à l'âge zéro puis à l'âge dix de l'effet :
// il est rappelé à chaque tic, et ses deux branches posent le même nœud, avec
// les mêmes tirages. C'est `kPaletteBillboardSchedule`.

// LES DEUX VAGUES : un nœud au tic zéro, un second au tic dix.
inline constexpr EffectNodeSchedule kPaletteBillboardSchedule = {0, 10, 10, 0};

// UN ÉCLAT QU'UN EFFET POSE : ce que son site d'appel passe au bâtisseur.
struct EffectPaletteBillboardPattern {
  std::uint32_t effect_id = 0;
  // Le nom de la fonction du client qui pose cet éclat.
  std::string_view client_function;
  std::string_view texture;
  // La palette, posée telle quelle dans les quatre pistes.
  std::uint8_t palette = 0;
  // L'ALPHA DE DÉPART des quatre pistes : il dépend du BÂTISSEUR, pas du site.
  // Cent pour les éclats de gel, deux cents pour le cercle de 1122.
  // ⚠ EN QUEUE : la table engendrée remplit cette structure par position.
  std::int16_t alpha = 100;
};

// Tous les éclats connus, par identifiant d'effet.
[[nodiscard]] std::span<const EffectPaletteBillboardPattern> EffectPaletteBillboardPatternTable();

// TOUS les éclats d'un effet, ou un span VIDE.
[[nodiscard]] std::span<const EffectPaletteBillboardPattern> EffectPaletteBillboardPatternsFor(
    std::uint32_t effect_id);

// LES QUATRE PISTES D'UN ÉCLAT, toutes vives et trois retardées.
//
// ⚠ LE BÂTISSEUR TIRE DEUX FOIS PAR PISTE — un balayage et un azimut, sur le
// tour entier — et l'ordre compte : deux éclats semés à la suite doivent différer
// comme chez le client, pas autrement.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedEffectPaletteBillboardTracks(
    const EffectPaletteBillboardPattern& pattern, std::uint32_t& generator);

// LE NŒUD qu'un patron décrit, pistes semées, prêt à rejoindre la troupe.
[[nodiscard]] PooledEffectNode SowEffectPaletteBillboardNode(
    const EffectPaletteBillboardPattern& pattern, glm::vec3 anchor, std::int32_t effect_life_ticks,
    std::uint32_t& generator);

}  // namespace ro::render
