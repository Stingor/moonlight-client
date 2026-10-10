#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include <glm/glm.hpp>

#include "ro/render/effect_node_pool_fwd.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LE SEMIS DU CARRÉ CONCENTRIQUE, sous-type 32. UN seul bâtisseur, et huit
// identifiants.
//
// ⚠ UN NŒUD ENTIER PAR TOUR DE BOUCLE, et non quatre pistes d'un même nœud.
// C'est une GERBE : le bâtisseur en construit autant que le compte le dit, et
// chacun tremble pour son compte.
//
// ⚠ CE SOUS-TYPE NE DÉPEND D'AUCUN FICHIER .STR : aucun de ses huit
// identifiants n'en a. Le compte vient
// d'un ARGUMENT DE CRÉATION — le nombre de sphères qu'un sort a convoquées —, et
// `render` ne le connaît pas : l'appelant le donne, comme il donne la cible de
// la colonne et la position d'écran du porteur.

// COMBIEN DE NŒUDS UNE GERBE PORTE AU PLUS.
//
// ⚠ CETTE BORNE NE VAUT QUE POUR LA BOUCLE. Le bâtisseur relit ensuite le compte
// BRUT, non borné, pour trancher la marque de chaque nœud — voir
// `EffectConcentricSquareMarkFor`. Les confondre ferait croire que deux marques
// sur quatre ne sont jamais posées.
inline constexpr std::int32_t kConcentricSquareMaxNodes = 5;

// UNE GERBE DE CARRÉS QU'UN EFFET POSE.
struct EffectConcentricSquarePattern {
  std::uint32_t effect_id = 0;
  std::string_view client_function;
  // ⚠ L'IMAGE DÉPEND DE L'OCTET : `thunder_center.bmp` n'est que la branche
  // par défaut, et cinq des huit ont une autre image.
  std::string_view texture;
  // L'OCTET DU SITE, qui choisit l'image et sert de marque à un des cas.
  std::uint8_t style = 0;
};

[[nodiscard]] std::span<const EffectConcentricSquarePattern> EffectConcentricSquarePatternTable();

[[nodiscard]] std::span<const EffectConcentricSquarePattern> EffectConcentricSquarePatternsFor(
    std::uint32_t effect_id);

// LA MARQUE D'UN NŒUD, selon l'octet du site, son rang dans la gerbe et le
// compte BRUT.
//
// L'enfant n'en porte aucune, les pièces la marque un, et un élément son propre
// octet. Pour les autres, ce sont les PREMIERS NŒUDS D'UNE GRANDE GERBE qui
// portent les halos : vingt et un quand le compte dépasse dix et qu'il reste au
// moins onze nœuds après celui-ci, vingt quand il dépasse cinq et qu'il en
// reste six. Une petite gerbe n'en a aucun.
[[nodiscard]] std::uint8_t EffectConcentricSquareMarkFor(std::int32_t raw_count, std::int32_t rank,
                                                         std::uint8_t style);

// LE CAP D'UN NŒUD dans sa gerbe, en degrés, avant le décalage tiré d'un
// élément.
//
// Soixante-douze degrés par rang, quel que soit le compte. Seules les pièces,
// quand elles sont cinq au plus, se répartissent sur le TOUR ENTIER.
[[nodiscard]] float EffectConcentricSquareHeadingFor(std::int32_t raw_count, std::int32_t rank,
                                                     std::uint8_t style);

// LES QUATRE PISTES d'UN nœud de la gerbe. Seules les deux premières servent.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedEffectConcentricSquareTracks(
    const EffectConcentricSquarePattern& pattern, std::int32_t raw_count, std::int32_t rank,
    std::uint32_t& generator);

// UN NŒUD DE LA GERBE, prêt à rejoindre la troupe.
[[nodiscard]] PooledEffectNode SowEffectConcentricSquareNode(
    const EffectConcentricSquarePattern& pattern, std::int32_t raw_count, std::int32_t rank,
    glm::vec3 anchor, std::int32_t effect_life_ticks, std::uint32_t& generator);

}  // namespace ro::render
