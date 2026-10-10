#pragma once

#include <string>

#include "ro/sim/repair_list.h"
#include "ro/vfs/item_table.h"
#include "ro/vfs/item_words.h"

#include "items/creator_names.h"

namespace moonclient {

// LE NOM D'UNE PIÈCE DE LA LISTE DE RÉPARATION, tel que la fenêtre le montre :
// raffinement et cartes compris, et le forgeron d'une arme forgée — demandé au
// serveur tant qu'il n'est pas connu, « Sans nom » d'ici là.
//
// `weapon_level` tranche entre arme (positif) et armure (nul ou négatif) : la
// liste du serveur ne dit pas le type de l'objet.
[[nodiscard]] std::string RepairPieceName(const ro::sim::RepairableItem& item, int weapon_level,
                                          const ro::vfs::ItemTable& items,
                                          const ro::vfs::ItemWords& words,
                                          const CreatorNames& creators);

}  // namespace moonclient
