#pragma once

// LA TABLE DES OBJETS TELLE QUE LA RECHERCHE D'ÉCHOPPES LA PARCOURT : chaque
// objet avec son nom, et, pour une carte, le mot qu'elle pose. Se juge sur un
// banc, sans fenêtre ni réseau.

#include <vector>

#include "ro/sim/store_search.h"
#include "ro/vfs/card_illustrations.h"
#include "ro/vfs/card_item_list.h"
#include "ro/vfs/item_table.h"
#include "ro/vfs/item_words.h"

namespace moonclient {

// LES TABLES QUI DISENT QU'UN OBJET EST UNE CARTE, en plus de celle des mots.
// Chacune est facultative : nulle ou vide, elle ne désigne aucune carte.
struct StoreSearchCardTables {
  const ro::vfs::CardIllustrations* illustrations = nullptr;
  const ro::vfs::CardItemList* listed = nullptr;
};

// Le catalogue, trié par identifiant comme la recherche l'exige.
//
// UNE CARTE est un objet qui a une illustration, un mot de carte, OU figure
// dans la liste des cartes : le champ « carte » accepte ainsi une carte qui
// n'a ni dessin ni mot.
//
// ⚠ LES VUES POINTENT DANS LA TABLE ET DANS LES MOTS : le catalogue ne leur
// survit pas.
[[nodiscard]] std::vector<ro::sim::StoreSearchCatalogueEntry> StoreSearchCatalogueOf(
    const ro::vfs::ItemTable& table, const ro::vfs::ItemWords& words,
    const StoreSearchCardTables& cards);

}  // namespace moonclient
