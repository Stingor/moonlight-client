#include "items/cart_view.h"

#include <algorithm>

#include "ro/core/log.h"
#include "ro/net/inventory.h"
#include "ro/sim/world.h"

#include "items/inventory_view.h"

namespace moonclient {

CartView::CartView(const ro::vfs::ItemTable* items, const ro::vfs::ItemWords* words,
                   ro::render::ItemIcons* icons)
    : items_(items), words_(words), icons_(icons) {}

void CartView::Refresh(bool open, const ro::sim::World& world, ro::ui::ItemTab tab,
                       std::string_view filter) {
  const ro::sim::Inventory& cart = world.cart();
  tally_ = ro::ui::CartTally{
      .weight = cart.weight(),
      .max_weight = cart.max_weight(),
      .item_count = static_cast<int>(cart.size()),
      // 🔴 C'EST LA CAPACITÉ QUI DIT SI LA CHARRETTE EXISTE. Un personnage sans
      // charrette et une charrette vide ont tous deux zéro objet ; seul le
      // serveur qui annonce une capacité distingue les deux, et `Forget` la
      // reprend quand la charrette part.
      .capacity = cart.capacity(),
  };
  if (!open) {
    cells_.clear();
    return;
  }
  // ⚠ PAS D'ONGLET DE FAVORIS : la charrette n'en a pas, et sans le dire un
  // objet que le serveur y aurait marqué — le même octet sert aux trois
  // contenants — disparaîtrait de tous ses onglets.
  BuildItemCells(cart, ItemCellsAsk{.tab = tab, .filter = filter, .creators = creators_}, items_,
                 words_, icons_, names_, cells_);
}

void CartView::Apply(const ro::ui::CartWindowAction& asked, ro::net::Connection& link,
                     const ro::sim::World& world) {
  if (asked.verb == ro::ui::CartVerb::kNothing || asked.slot == 0) {
    return;
  }
  // 🔴 LA PLACE SE REVÉRIFIE, on ne se fie pas au clic. Une invite de quantité
  // laisse tout le temps de vider la charrette autrement.
  const ro::sim::InventoryItem* item = world.cart().Find(asked.slot);
  if (item == nullptr) {
    return;
  }
  const int amount = std::clamp(asked.amount, 1, static_cast<int>(item->amount));
  // ⚠ LA CASE EST CELLE DE LA CHARRETTE dans les deux sens : le serveur lui
  // retranche deux comme à celle du sac, et c'est le contenant d'où l'objet PART
  // qui la donne.
  const ro::core::Result<void> sent =
      asked.verb == ro::ui::CartVerb::kToStorage
          ? ro::net::RequestCartToStorage(link, item->client_slot, amount)
          : ro::net::RequestCartToInventory(link, item->client_slot, amount);
  if (!sent) {
    ro::core::LogWarning("charrette : demande non émise — {}", ro::core::Describe(sent.error()));
  }
}

}  // namespace moonclient
