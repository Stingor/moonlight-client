// Les transferts gelés. Voir l'en-tête.

#include "items/transfer_hold.h"

namespace moonclient {
namespace {

using ro::ui::ItemHolder;

// Un glisser reçu par une fenêtre, venu d'un des trois contenants. Ceux du
// mannequin, de l'album ou d'un lien ne sont pas des transferts.
[[nodiscard]] bool HoldDrop(ro::ui::ItemTransferAsk& received) {
  if (received.client_slot == 0) {
    return false;
  }
  const bool between_containers = received.from == ItemHolder::kInventory ||
                                  received.from == ItemHolder::kCart ||
                                  received.from == ItemHolder::kStorage;
  if (!between_containers) {
    return false;
  }
  received = ro::ui::ItemTransferAsk();
  return true;
}

}  // namespace

bool HoldTransfers(ro::ui::InventoryWindowAction& inventory, ro::ui::CartWindowAction& cart,
                   ro::ui::StorageWindowAction& storage) {
  bool held = false;
  if (inventory.verb == ro::ui::ItemVerb::kToCart ||
      inventory.verb == ro::ui::ItemVerb::kToStorage) {
    inventory.verb = ro::ui::ItemVerb::kNothing;
    held = true;
  }
  if (cart.verb != ro::ui::CartVerb::kNothing) {
    cart.verb = ro::ui::CartVerb::kNothing;
    held = true;
  }
  if (storage.verb == ro::ui::StorageVerb::kToInventory ||
      storage.verb == ro::ui::StorageVerb::kToCart) {
    storage.verb = ro::ui::StorageVerb::kNothing;
    held = true;
  }
  held = HoldDrop(inventory.received) || held;
  held = HoldDrop(cart.received) || held;
  held = HoldDrop(storage.received) || held;
  return held;
}

ro::ui::ItemTransferAsk ReceivedTransfer(const ro::ui::InventoryWindowAction& inventory,
                                         const ro::ui::CartWindowAction& cart,
                                         const ro::ui::StorageWindowAction& storage) {
  if (inventory.received.client_slot != 0) {
    return inventory.received;
  }
  if (cart.received.client_slot != 0) {
    return cart.received;
  }
  return storage.received;
}

}  // namespace moonclient
