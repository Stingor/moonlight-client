#pragma once

// L'OBJET QU'UN GESTE DÉSIGNE, d'où qu'il vienne : un lien de la conversation,
// une pièce de l'équipement montré, un butin, une ligne de l'Atlas.
//
// ⚠ TOUS LES GESTES SUR UN OBJET SE JOUENT PAR `Game::FollowItemGesture`. Les
// surfaces ne diffèrent que par ce qui est rangé ici : d'où l'objet sort, et ce
// que son lien emporte.

#include <cstdint>

#include "ro/ui/shortcut_bar.h"
#include "ro/vfs/item_link.h"

namespace moonclient {

struct ItemGestureTarget {
  // L'objet décrit, cherché au Marché, interrogé. Zéro : aucun geste.
  std::uint32_t item_id = 0;
  // LA PLACE ET LE CONTENANT d'où la description le décore. Zéro et le sac pour
  // un objet de catalogue, qu'on ne tient pas. ⚠ `kChatLink` : la description
  // se compose de ce que `link` porte, car on ne possède pas forcément ce qu'un
  // autre joueur a lié.
  std::uint16_t from_slot = 0;
  ro::ui::ItemHolder from_holder = ro::ui::ItemHolder::kInventory;
  // CE QUE LE MAJ+CLIC REPOSE DANS LA SAISIE : l'objet ENTIER quand on le
  // connaît — raffinement, cartes, grade —, son seul identifiant sinon.
  ro::vfs::ItemLink link;
};

}  // namespace moonclient
