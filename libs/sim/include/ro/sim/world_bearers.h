#pragma once

#include <cstdint>

#include "ro/sim/world_fwd.h"

namespace ro::sim {

// CE QUE LE MONDE TIENT ENCORE SOUS UN IDENTIFIANT — acteur ou unité au sol.
//
// 🔴 LES DEUX TABLES SONT DISTINCTES, ET LEURS IDENTIFIANTS VIENNENT DU MÊME
// ESPACE. Le serveur numérote une unité au sol comme il numérote un acteur, si
// bien que « existe-t-il ? » ne se répond pas sur une seule des deux.
//
// ⚠ ET C'EST CETTE QUESTION QUI DÉCIDE DE CE QU'UN EFFET EMPORTE EN PARTANT. Un
// nœud accroché à son porteur ne s'efface que si l'on sait dire que ce porteur
// est parti ; ne consulter que les acteurs laissait le mur de Safety Wall à
// l'écran longtemps après que le serveur l'avait levé, que sa durée se soit
// achevée ou que son compte de coups se soit épuisé.
//
// ⚠ ELLE NE VIT PAS DANS `World` — qui touche son plafond —, et elle n'a pas à y
// vivre : elle ne lit que ce que le monde expose déjà. Une question SUR le monde
// n'est pas une pièce DU monde.
[[nodiscard]] bool WorldHolds(const World& world, std::uint32_t id) noexcept;

}  // namespace ro::sim
