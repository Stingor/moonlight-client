// CE QU'ON FAIT D'UN OBJET TIRÉ D'UNE FENÊTRE À L'AUTRE.
//
// 🔴 DEUX LÂCHERS QUI NE SE RESSEMBLENT PAS. Le premier TRAVERSE une frontière —
// du sac vers la charrette, du coffre vers le sac — et demande un paquet au
// serveur, qui peut refuser. Le second ne quitte pas le sac : il range une case,
// le serveur n'en sait rien, et c'est notre fichier qui s'en souvient.
//
// 🔴 ILS ONT QUITTÉ `game.cpp` PARCE QU'IL PASSAIT LE PLAFOND, et ce sont eux
// qui sont partis parce qu'ils forment un tout : le même geste de souris finit
// dans l'un ou dans l'autre, et c'est l'ORIGINE de la charge qui tranche.

#include <cstddef>
#include <cstdint>
#include <span>

#include "ro/core/log.h"
#include "ro/net/inventory.h"
#include "ro/sim/inventory.h"

#include "game/game.h"

namespace moonclient {

// LE PLACEMENT LIBRE : UN LÂCHER QUI NE PARLE PAS AU SERVEUR.
//
// 🔴 C'EST LE SEUL GESTE DE CETTE FENÊTRE QUI N'ÉMET RIEN. Le serveur ne connaît
// aucune position d'objet : la grille est à NOUS, et ce qu'on y range vit dans
// notre fichier. C'est aussi pourquoi il ne passe pas par `InventoryView::Apply`,
// qui n'existe que pour émettre.
//
// ⚠ L'OCCUPANT SE LIT DANS LA GRILLE DESSINÉE, seule à savoir ce qui était à
// cette case-là : la table, elle, ne connaît que les objets QU'ON A PLACÉS, et
// un objet qui remplissait un trou n'y figure pas.
void Game::PlaceDraggedItem(const ro::ui::InventoryWindowAction& asked) {
  if (asked.placed_cell < 0 || asked.placed_slot == 0) {
    return;
  }
  const ro::sim::InventoryItem* const dragged = world_.inventory().Find(asked.placed_slot);
  if (dragged == nullptr) {
    return;
  }
  const std::span<const ro::ui::InventoryCellView> cells = bag_->cells();
  const auto cell = static_cast<std::size_t>(asked.placed_cell);
  // ⚠ LA CASE PEUT AVOIR DISPARU entre le dessin et ici — un objet consommé
  // raccourcit la grille —, et une case vide n'a pas d'occupant.
  const std::uint32_t sitting_there = cell < cells.size() ? cells[cell].item_id : 0;
  if (sitting_there == dragged->item_id) {
    return;  // posé sur lui-même : rien n'a bougé
  }
  settings_->PlaceItem(dragged->item_id, asked.placed_cell, sitting_there);
}

void Game::ApplyItemTransfer(const ro::ui::ItemTransferAsk& asked) {
  using ro::ui::ItemHolder;
  if (asked.client_slot == 0) {
    return;
  }
  // COMBIEN. ⚠ LA PILE SE DEMANDE, comme au menu : le prompt vit dans la fenêtre
  // d'ORIGINE, qui est la seule à montrer l'objet et à connaître sa taille.
  const ro::sim::Inventory* holder = nullptr;
  switch (asked.from) {
    case ItemHolder::kInventory:
      holder = &world_.inventory();
      break;
    case ItemHolder::kCart:
      holder = &world_.cart();
      break;
    case ItemHolder::kStorage:
      holder = &world_.storage();
      break;
    case ItemHolder::kEquipment:
      // Le mannequin ne transfère pas : il déshabille, et le sac s'en charge.
      return;
    case ItemHolder::kViewedEquipment:
      // L'équipement d'un autre ne se prend pas.
      return;
    case ItemHolder::kCardAlbum:
      // Une pochette n'est pas une place : son retrait a sa propre charge, que
      // le sac reçoit à part.
      return;
    case ItemHolder::kChatLink:
      // Un lien ne se possède pas.
      return;
  }
  const ro::sim::InventoryItem* const item = holder->Find(asked.client_slot);
  if (item == nullptr) {
    return;
  }
  // 🔴 LE COFFRE OUVERT MANGE L'ÉCHANGE SAC <-> CHARRETTE : `pc_cant_act2`
  // compte `storage_flag`, et le serveur jette le paquet sans un mot.
  //
  // ⚠ LE JOUEUR A DÉJÀ ÉTÉ PRÉVENU : le fantôme du glisser le lui a dit en
  // rouge tant qu'il survolait la cible, et le menu grise son entrée. Ici on se
  // contente de ne rien émettre — répéter dans la conversation ferait un
  // troisième avertissement pour un geste qu'on vient de refuser deux fois.
  const bool between_bag_and_cart =
      (asked.from == ItemHolder::kInventory && asked.to == ItemHolder::kCart) ||
      (asked.from == ItemHolder::kCart && asked.to == ItemHolder::kInventory);
  if (between_bag_and_cart && world_.storage_open()) {
    return;
  }
  const bool ask_how_many = item->amount > 1;

  switch (asked.from) {
    case ItemHolder::kInventory: {
      const ro::ui::ItemVerb verb =
          asked.to == ItemHolder::kCart ? ro::ui::ItemVerb::kToCart : ro::ui::ItemVerb::kToStorage;
      if (ask_how_many) {
        windows_.inventory.asking_slot = asked.client_slot;
        windows_.inventory.asked_amount = 1;
        windows_.inventory.asking_verb = verb;
      } else {
        ro::ui::InventoryWindowAction moved;
        moved.verb = verb;
        moved.slot = asked.client_slot;
        moved.amount = 1;
        bag_->Apply(moved, link_, world_);
      }
      break;
    }
    case ItemHolder::kCart: {
      if (ask_how_many) {
        windows_.cart.ask.slot = asked.client_slot;
        windows_.cart.ask.amount = 1;
        windows_.cart.ask_to_storage = asked.to == ItemHolder::kStorage;
      } else {
        ro::ui::CartWindowAction moved;
        moved.verb = asked.to == ItemHolder::kStorage ? ro::ui::CartVerb::kToStorage
                                                      : ro::ui::CartVerb::kToInventory;
        moved.slot = asked.client_slot;
        moved.amount = 1;
        cart_->Apply(moved, link_, world_);
      }
      break;
    }
    case ItemHolder::kStorage: {
      if (ask_how_many) {
        windows_.storage.ask.slot = asked.client_slot;
        windows_.storage.ask.amount = 1;
        windows_.storage.ask_to_cart = asked.to == ItemHolder::kCart;
      } else {
        ro::ui::StorageWindowAction moved;
        moved.verb = asked.to == ItemHolder::kCart ? ro::ui::StorageVerb::kToCart
                                                   : ro::ui::StorageVerb::kToInventory;
        moved.slot = asked.client_slot;
        moved.amount = 1;
        storage_->Apply(moved, link_, world_);
      }
      break;
    }
    case ItemHolder::kEquipment:
    case ItemHolder::kViewedEquipment:
    case ItemHolder::kCardAlbum:
    case ItemHolder::kChatLink:
      break;
  }
}

OptionTargets Game::OptionsTargets() {
  return OptionTargets{.audio = &audio_,
                       .music = &music_,
                       .interface = interface_,
                       .world = &world_,
                       .chat = chat_tabs_.has_value() ? &*chat_tabs_ : nullptr,
                       .fixed_camera = &aim_.tracking.fixed,
                       .item_drop = &drops_.settings(),
                       .item_obtain_grip = &toast_grip_,
                       .display = gl_,
                       .effects = effects_.has_value() ? &*effects_ : nullptr,
                       .afk_try_now = &afk_requested_,
                       .craft_atlas = &windows_.craft_atlas.state,
                       .file = &settings_->file()};
}

}  // namespace moonclient
