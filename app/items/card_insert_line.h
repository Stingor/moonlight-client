#pragma once

#include <functional>
#include <string>

#include "ro/sim/card_composition.h"
#include "ro/sim/inventory.h"
#include "ro/vfs/item_table.h"
#include "ro/vfs/item_words.h"

namespace moonclient {

// LA LIGNE QU'ÉCRIT L'ACCUSÉ D'UN SERTISSAGE : quelle carte, dans quelle pièce,
// et à quel emplacement.
//
//   « <Poring Card> sertie dans <Knife [3]> (emplacement 1/3). »
//   « <Poring Card> n'a pas pu être sertie dans <Knife [3]>. »
//
// Les deux noms sont des LIENS D'OBJET : la ligne porte leurs balises, que la
// conversation décore et rend cliquables comme celles d'un joueur.
//
// ⚠ LES DEUX OBJETS SONT CEUX D'AVANT : la pièce se nomme sans la carte
// qu'elle vient de recevoir (voir `ro::sim::CardInsertReport`).
//
// 🔴 SANS DEMANDE QUI LUI RÉPONDE, LA LIGNE EST LA PHRASE GÉNÉRALE, « L'insertion
// de la carte a réussi. » : un accusé qu'on ne relie à rien ne sait rien nommer,
// et inventer les noms serait pire que ne pas les dire. De même si la pièce n'a
// plus d'emplacement libre à nommer, ou si un objet ne donne pas de lien.

// Le lien d'un objet du sac, tel que la conversation le relira.
using ItemTagOf = std::function<std::string(const ro::sim::InventoryItem& item)>;

// LE RANG, À PARTIR DE UN, DE L'EMPLACEMENT QUE LA CARTE VA REMPLIR : le
// premier vide parmi les `total_slots` de la pièce, état d'avant. C'est la
// règle du serveur, et celle du sac à l'accusé (`Inventory::InsertCard`).
// Zéro : aucun emplacement libre — pièce pleine, forgée, ou sans emplacement.
[[nodiscard]] int CardInsertSlotRank(const ro::sim::InventoryItem& equip_before, int total_slots);

[[nodiscard]] std::string CardInsertLine(const ro::sim::CardInsertReport& report,
                                         const ro::vfs::ItemTable* items,
                                         const ro::vfs::ItemWords& words, const ItemTagOf& tag_of);

}  // namespace moonclient
