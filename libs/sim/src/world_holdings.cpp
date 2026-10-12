// CE QUE LE PERSONNAGE TIENT : les contenants et les coffres qu'il peut ouvrir.
// Voir `WorldHoldings` ; la banque, elle, a sa classe : voir `BankSession`.

#include "ro/sim/world_holdings.h"

#include <cstdint>
#include <utility>
#include <vector>

namespace ro::sim {

void WorldHoldings::TellStorages(std::vector<StorageShelf> shelves, std::uint8_t open_id) {
  storage_shelves_ = std::move(shelves);
  open_storage_id_ = open_id;
}

void WorldHoldings::CloseStorage() {
  storage_open_ = false;
  // ⚠ LA LISTE RESTE, LE MARQUEUR PART : les coffres qu'on PEUT ouvrir ne
  // dépendent pas de celui qui l'est. Garder le numéro laisserait un onglet
  // marqué sur un coffre refermé, et le serveur, lui, renvoie sa liste avec
  // 0xFF à la prochaine occasion.
  open_storage_id_ = kNoStorageOpen;
  storage_.Clear();
}

Inventory& WorldHoldings::HolderFor(ItemHolder holder) noexcept {
  switch (holder) {
    case ItemHolder::kCart:
      return cart_;
    case ItemHolder::kStorage:
      return storage_;
    case ItemHolder::kInventory:
      break;
  }
  return inventory_;
}

}  // namespace ro::sim
