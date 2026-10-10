#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include <glm/glm.hpp>

#include "ro/render/effect_node_pool_fwd.h"
#include "ro/render/effect_pie_wedge.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LE SEMIS DU QUARTIER DE TARTE, et les effets du sous-type 162.
//
// Une image, quatre pistes vives, et un nœud NEUF À CHAQUE TIC pendant trois
// tics.
//
// ⚠ DOUZE QUARTIERS COUVRENT LE TOUR, ET C'EST LE TIC QUI LES DISPERSE.
// L'inclinaison d'une piste vaut trente degrés fois « son rang plus quatre fois
// le tic » : les quatre du premier tic se posent à zéro, trente, soixante et
// quatre-vingt-dix, ceux du deuxième reprennent à cent vingt, et ceux du
// troisième ferment le cercle à trois cent trente. Semer les trois nœuds au même
// tic superposerait trois quarts de cercle au lieu d'en dessiner un entier.
//
// ⚠ ET LE NŒUD NAÎT TROIS FOIS, pas une. C'est un horaire, pas un semis unique :
// le bâtisseur se rappelle tant que le compteur de tics de l'effet ne dépasse
// pas deux.

// COMBIEN DE TICS D'AFFILÉE le bâtisseur pose un nœud, le premier compris.
inline constexpr std::int32_t kPieWedgeSowingTicks = 3;

// L'ÉCART ANGULAIRE DE DEUX QUARTIERS VOISINS, en degrés. Douze fois trente font
// le tour complet.
inline constexpr std::int32_t kPieWedgeDegreesPerStep = 30;

// UN ÉCLAT DE QUARTIERS qu'un effet pose : ce que son site passe au bâtisseur.
struct EffectPieWedgePattern {
  std::uint32_t effect_id = 0;
  // Le nom de référence du bâtisseur qui pose cet éclat.
  std::string_view client_function;
  std::string_view texture;
};

// Tous les éclats connus, par identifiant d'effet.
[[nodiscard]] std::span<const EffectPieWedgePattern> EffectPieWedgePatternTable();

// TOUS les éclats d'un effet, ou un span VIDE.
[[nodiscard]] std::span<const EffectPieWedgePattern> EffectPieWedgePatternsFor(
    std::uint32_t effect_id);

// LES QUATRE QUARTIERS D'UN NŒUD, tous vifs.
//
// `tick` est le tic de l'effet où ce nœud naît : c'est lui qui décide du quart
// de cercle que ces quatre-là couvrent.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedEffectPieWedgeTracks(
    std::int32_t tick);

// LE NŒUD qu'un patron décrit, pistes semées, prêt à rejoindre la troupe.
[[nodiscard]] PooledEffectNode SowEffectPieWedgeNode(const EffectPieWedgePattern& pattern,
                                                     glm::vec3 anchor, std::int32_t tick,
                                                     std::int32_t effect_life_ticks);

}  // namespace ro::render
