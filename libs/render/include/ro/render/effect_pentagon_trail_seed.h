#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include <glm/glm.hpp>

#include "ro/render/effect_node_pool_fwd.h"
#include "ro/render/effect_pentagon_trail.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LE SEMIS DE LA TRAÎNÉE PENTAGONALE, et les sept effets qui l'emploient par le
// sous-type 66.
//
// Quatre couches empilées. Sept identifiants, un bâtisseur, et DEUX arguments —
// une image et un octet.
//
// ⚠ LES QUATRE PISTES SONT QUATRE COUCHES, pas quatre copies. Leurs rayons
// croissent d'une couche à l'autre et leurs azimuts tournent de vingt-cinq
// degrés, si bien que les cinq côtés de chacune ne se superposent pas à ceux de
// la précédente. Les semer identiques rendrait un seul tube.
//
// ⚠ ET LA QUATRIÈME EST À PART. Elle naît avec cent quatre-vingts tics déjà
// écoulés, et son rayon ne suit pas la suite des trois autres — le client
// l'écrit en constante. C'est la couche de tête, celle qui est déjà loin quand
// les autres partent.

// UNE TRAÎNÉE QU'UN EFFET POSE : ce que son site d'appel passe au bâtisseur.
struct EffectPentagonTrailPattern {
  std::uint32_t effect_id = 0;
  // LA FONCTION DU CLIENT qui pose cette traînée.
  std::string_view client_function;
  std::string_view texture;
  // L'octet, et il décide de QUATRE choses : l'alpha, la suite des rayons, le
  // rayon de la couche de tête, et la distance que la traînée parcourt.
  std::uint8_t builder_argument = 0;
};

// Toutes les traînées connues, par identifiant d'effet.
[[nodiscard]] std::span<const EffectPentagonTrailPattern> EffectPentagonTrailPatternTable();

// TOUTES les traînées d'un effet, ou un span VIDE.
[[nodiscard]] std::span<const EffectPentagonTrailPattern> EffectPentagonTrailPatternsFor(
    std::uint32_t effect_id);

// LES QUATRE COUCHES, toutes vives.
//
// `origin` est la position de l'effet : le bâtisseur la recopie dans le premier
// centre de chaque piste, et pose le second à une distance fixe de là.
//
// ⚠ AUCUN TIRAGE : le bâtisseur ne consulte aucun générateur.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedEffectPentagonTrailTracks(
    const EffectPentagonTrailPattern& pattern, glm::vec3 origin);

// LE NŒUD qu'un patron décrit, pistes semées, prêt à rejoindre la troupe.
[[nodiscard]] PooledEffectNode SowEffectPentagonTrailNode(const EffectPentagonTrailPattern& pattern,
                                                          glm::vec3 anchor,
                                                          std::int32_t effect_life_ticks);

}  // namespace ro::render
