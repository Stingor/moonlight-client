#pragma once

#include <cstdint>

#include "ro/sim/inventory.h"

namespace ro::sim {

// LA PIÈCE PORTÉE À LAQUELLE UNE DESCRIPTION COMPARE L'ITEM DÉCRIT.
//
// Le client ouvre à côté de la description d'une pièce celle de la pièce qu'on
// porte au même emplacement. 🔴 SEULEMENT pour une pièce qui n'est pas
// elle-même portée, et seulement pour les masques d'emplacement qu'il connaît :
// la correspondance est EXACTE, masque par masque, pas un « et » de bits.
//
//   masque de l'item décrit                      emplacement porté comparé
//   un seul emplacement (coiffe basse, cape,     le même
//     armure, bouclier, chaussures, costumes,
//     pièces d'ombre)
//   arme, ou arme à deux mains                   l'arme
//   les deux accessoires                         l'accessoire droit
//   coiffe haute, haute + basse, haute + milieu, la coiffe haute
//     les trois
//   coiffe du milieu, milieu + basse             la coiffe du milieu
//   costume haut (seul ou combiné)               le costume haut
//   costume du milieu, milieu + bas              le costume du milieu
//
// ⚠ TOUT AUTRE MASQUE NE COMPARE PAS — un accessoire d'un seul côté non plus.
// C'est la table du client, reprise telle quelle : l'étendre au bon sens
// montrerait une comparaison que le jeu ne montre jamais.
//
// LES MUNITIONS SONT À PART, hors de la table : elles arrivent dans la liste
// des objets ordinaires, où le champ d'emplacement dit où elles POURRAIENT se
// porter. Une munition qui n'est pas celle qu'on a posée se compare à la
// munition posée — l'unique objet du sac porté à l'emplacement des munitions —,
// et à rien quand aucune ne l'est.

// L'emplacement comparé, en un bit ; zéro quand le masque n'en a pas.
[[nodiscard]] std::uint32_t ComparedEquipSlot(std::uint32_t wearable_at) noexcept;

// La pièce portée à comparer à `described`, ou nul : pièce déjà portée,
// masque sans comparaison, ou rien de porté à cet emplacement.
[[nodiscard]] const InventoryItem* EquippedToCompare(const Inventory& bag,
                                                     const InventoryItem& described) noexcept;

}  // namespace ro::sim
