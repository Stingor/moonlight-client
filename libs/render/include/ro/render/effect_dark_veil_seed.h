#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include <glm/glm.hpp>

#include "ro/render/effect_dark_veil.h"
#include "ro/render/effect_node_pool_fwd.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LE SEMIS DU VOILE, et les onze effets qui l'emploient par le sous-type 71.
//
// C'est le plus court des semis : un nœud, deux images, UNE piste vive, un
// rayon, une marque. Les onze identifiants se distinguent par UN SEUL octet,
// qui court de zéro à dix.
//
// ⚠ ET LE VOILE NAÎT INVISIBLE. Le bâtisseur pose l'alpha à ZÉRO, et c'est le
// mouvement qui le monte d'une unité par tic. Le semer plein ferait tomber la
// nuit d'un coup au lieu de la laisser venir — et un voile absent au tic zéro
// n'est pas un voile manquant. Voir `AdvanceDarkVeilTracks`.

// UN VOILE QU'UN EFFET POSE : ce que son site d'appel passe au bâtisseur.
struct EffectDarkVeilPattern {
  std::uint32_t effect_id = 0;
  // Le nom de référence du bâtisseur qui pose ce voile : une étiquette de
  // traçabilité, sans effet sur le rendu.
  std::string_view client_function;
  // LE CŒUR puis LE CADRE, dans l'ordre où le bâtisseur les charge.
  std::array<std::string_view, 2> textures;
  // ⚠ L'UNIQUE OCTET, et il décide de DEUX choses à la fois : le rayon du voile
  // et sa profondeur de noir. Zéro n'y est pas le premier d'une suite mais un
  // cas À PART — son rayon ne suit pas la règle des dix autres, et lui seul
  // donne le noir profond.
  std::uint8_t builder_argument = 0;
};

// Tous les voiles connus, par identifiant d'effet.
[[nodiscard]] std::span<const EffectDarkVeilPattern> EffectDarkVeilPatternTable();

// TOUS les voiles d'un effet, ou un span VIDE.
[[nodiscard]] std::span<const EffectDarkVeilPattern> EffectDarkVeilPatternsFor(
    std::uint32_t effect_id);

// LE RAYON ET LA MARQUE qu'un octet donne. Rendus ensemble parce que le client
// les décide dans la même branche.
struct EffectDarkVeilShape {
  float radius_at_anchor = 0.0F;
  std::uint8_t shade_mark = 0;
};

[[nodiscard]] EffectDarkVeilShape EffectDarkVeilShapeFor(std::uint8_t builder_argument);

// LES QUATRE PISTES D'UN VOILE : une seule vive, et son alpha à zéro.
//
// ⚠ AUCUN TIRAGE : le bâtisseur ne consulte aucun générateur.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedEffectDarkVeilTracks(
    const EffectDarkVeilPattern& pattern);

// LE NŒUD qu'un patron décrit, piste semée, prêt à rejoindre la troupe.
//
// ⚠ LE BÂTISSEUR ÉLÈVE SON EFFET DE NEUF UNITÉS avant de poser le nœud, et ce
// geste-là appartient au semis : le voile ne se pose pas au sol mais un peu
// au-dessus. La vie, elle, vient de l'instance — l'appelant doit la donner.
[[nodiscard]] PooledEffectNode SowEffectDarkVeilNode(const EffectDarkVeilPattern& pattern,
                                                     glm::vec3 anchor,
                                                     std::int32_t effect_life_ticks);

}  // namespace ro::render
