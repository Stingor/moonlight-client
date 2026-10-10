#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include <glm/glm.hpp>

#include "ro/render/effect_node_pool_fwd.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LE SEMIS DE LA COLONNE QUI TOURNE, sous-type 137.
//
// Deux bâtisseurs, et ils sont le MÊME à trois flottants près : le second, le
// DIRIGÉ, écrit dans les trois dernières cases de la queue le vecteur qui va de
// l'effet à sa CIBLE, là où le premier les met à zéro.
//
// ⚠ TOUT DÉPEND D'UN OCTET que le site d'appel passe. Il choisit la variante de
// teinte, l'alpha, la hauteur, le retard et la dispersion — cinq valeurs qui ne
// se déduisent pas les unes des autres.

// LES QUATRE COLONNES d'un nœud, et de combien leurs azimuts s'écartent.
inline constexpr std::int16_t kSpiralColumnDegreesPerTrack = 90;

// UN NŒUD DE COLONNES QU'UN EFFET POSE.
struct EffectSpiralColumnPattern {
  std::uint32_t effect_id = 0;
  std::string_view client_function;
  std::string_view texture;
  // L'OCTET DU SITE, dont tout le reste découle.
  std::uint8_t style = 0;
  // ⚠ VRAI QUAND LA COLONNE PENCHE VERS UNE CIBLE. Le vecteur, lui, ne vient pas
  // de la table : `render` ne sait pas où vise un sort (CLAUDE.md), et c'est
  // l'appelant qui le donne au semis.
  bool aims_at_target = false;
  // 🔴 COMBIEN DE NŒUDS LE SITE SÈME : chaque site appelle son bâtisseur en
  // boucle — trois, six ou sept fois —, et chaque appel tire sa variante et ses
  // pistes à la suite du précédent. Un seul nœud montrait une colonne là où le
  // client en dresse plusieurs. ⚠ EN QUEUE.
  std::uint16_t copies = 1;
};

// Tous les nœuds de colonnes connus, par identifiant d'effet.
[[nodiscard]] std::span<const EffectSpiralColumnPattern> EffectSpiralColumnPatternTable();

// TOUS les nœuds d'un effet, ou un span VIDE.
[[nodiscard]] std::span<const EffectSpiralColumnPattern> EffectSpiralColumnPatternsFor(
    std::uint32_t effect_id);

// LA VARIANTE DE TEINTE que l'octet choisit, tirée au sort pour deux d'entre eux.
//
// ⚠ QUATRE EST LA BRANCHE DES PETITS OCTETS, zéro et un : selon l'octet, la
// variante vaut aussi cinq, six, sept, ou se tire au sort, et un octet que le
// client ne nomme pas garde le UN du constructeur. Voir l'ADR 0019.
//
// ⚠ ET LES DEUX BÂTISSEURS NE NOMMENT PAS LES MÊMES : celui qui vise une cible
// (`aims_at_target`) n'a pas la branche du douze.
[[nodiscard]] std::uint8_t EffectSpiralColumnVariantFor(std::uint8_t style, bool aims_at_target,
                                                        std::uint32_t& generator);

// LES QUATRE PISTES d'un nœud, tirages faits.
//
// `aim` est le vecteur qui va de l'effet à sa cible, en unités du monde ; il est
// ignoré quand le patron ne vise pas. Le générateur AVANCE : deux nœuds semés à
// la suite ne tirent pas les mêmes retards.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedEffectSpiralColumnTracks(
    const EffectSpiralColumnPattern& pattern, glm::vec3 aim, std::uint32_t& generator);

// LE NŒUD qu'un patron décrit, prêt à rejoindre la troupe.
[[nodiscard]] PooledEffectNode SowEffectSpiralColumnNode(const EffectSpiralColumnPattern& pattern,
                                                         glm::vec3 anchor, glm::vec3 aim,
                                                         std::int32_t effect_life_ticks,
                                                         std::uint32_t& generator);

}  // namespace ro::render
