// Le menu d'une case du sac. Voir l'en-tête.

#include "inventory_cell_menu.h"

#include <string>
#include <string_view>

#include "ro/core/i18n.h"
#include "ro/ui/skin_popup.h"

#include "item_grid.h"
#include "transfer_freeze.h"

namespace ro::ui {
namespace {

// LE SERTISSAGE RAPIDE : les cartes du sac que le serveur accepterait sur cette
// pièce, et un clic sertit sans ouvrir de fenêtre.
//
// ⚠ LA QUESTION PART À L'OUVERTURE DU SOUS-MENU, pas avant : elle coûte un
// aller-retour, et la plupart des menus s'ouvrent pour autre chose. Tant que la
// réponse n'est pas là, le sous-menu le dit.
void DrawQuickInsertMenu(const InventoryCellView& cell, InventoryWindowAction& action,
                         const InventoryWindowView& view) {
  if (!ImGui::BeginMenu(core::Tr("Sertissage rapide"))) {
    return;
  }
  action.compatible_asked = cell.client_slot;
  const bool answered = view.quick_insert_equip == cell.client_slot && view.quick_insert_answered;
  if (!answered) {
    ImGui::TextDisabled("%s", core::Tr("Chargement…"));
  } else if (view.quick_insert_cards.empty()) {
    ImGui::TextDisabled("%s", core::Tr("Aucune carte compatible"));
  }
  if (answered) {
    for (const QuickInsertCardView& card : view.quick_insert_cards) {
      ImGui::PushID(card.card_slot);
      const std::string label(card.name.empty() ? std::string_view(core::Tr("(carte)"))
                                                : card.name);
      if (ImGui::MenuItem(label.c_str())) {
        action.verb = ItemVerb::kQuickInsert;
        action.slot = cell.client_slot;
        action.insert_card_slot = card.card_slot;
      }
      ImGui::PopID();
    }
  }
  ImGui::EndMenu();
}

}  // namespace

void DrawInventoryCellMenu(const InventoryCellView& cell, ImVec2 menu_padding, ImVec2 menu_spacing,
                           InventoryWindowAction& action, InventoryWindowState& state,
                           const InventoryWindowView& view) {
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, menu_padding);
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, menu_spacing);
  if (BeginSkinPopupContextItem("menu")) {
    // ⚠ EN TÊTE, ET SÉPARÉE DU RESTE : décrire ne TOUCHE à rien. Mêlée aux
    // gestes qui déplacent ou consomment, on la cliquerait par erreur en visant
    // son voisin.
    if (ImGui::MenuItem(core::Tr("Description"))) {
      action.describe_id = cell.item_id;
      action.describe_slot = cell.client_slot;
    }
    // LE MARCHÉ, ouvert sur cet item : lui non plus ne touche à rien.
    if (ImGui::MenuItem(core::Tr("Rechercher sur le Marché"))) {
      action.market_item_id = cell.item_id;
    }
    if (ImGui::MenuItem(core::Tr("Ouvrir dans la base de données"))) {
      action.database_item_id = cell.item_id;
    }
    // UNE CARTE : sa pochette dans l'album, qui ne touche à rien non plus.
    if (SeeInAlbumMenuItem(CardAlbumEntryOf(view.album_links, cell.item_id, cell.card))) {
      action.see_in_album_id = cell.item_id;
    }
    ImGui::Separator();
    if (cell.worn) {
      if (ImGui::MenuItem(core::Tr("Retirer"))) {
        action.verb = ItemVerb::kTakeOff;
        action.slot = cell.client_slot;
      }
    } else if (cell.wearable || cell.pet_accessory) {
      if (ImGui::MenuItem(core::Tr("Équiper"))) {
        action.verb = ItemVerb::kUseOrWear;
        action.slot = cell.client_slot;
      }
    } else if (cell.usable) {
      if (ImGui::MenuItem(core::Tr("Utiliser"))) {
        action.verb = ItemVerb::kUseOrWear;
        action.slot = cell.client_slot;
      }
    } else if (cell.card) {
      // Une carte ne s'utilise ni ne se porte : le même geste la SERTIT.
      if (ImGui::MenuItem(core::Tr("Sertir"))) {
        action.verb = ItemVerb::kInsertCard;
        action.slot = cell.client_slot;
      }
    }
    if (cell.socket_free) {
      DrawQuickInsertMenu(cell, action, view);
    }
    // ⚠ JETER NE SE PROPOSE PAS SUR CE QU'ON PORTE : le serveur refuserait, et
    // une entrée qui ne marche jamais use la confiance qu'on met dans les
    // autres.
    if (!cell.worn) {
      // ⚠ GRISÉE PLUTÔT QU'ABSENTE, à la différence des autres. « Pas cet
      // objet » se dit en n'offrant rien ; « pas tant que le verrou est mis »
      // demande à être VU, sinon l'entrée manquante passe pour un défaut.
      if (state.drop_locked) {
        ImGui::BeginDisabled();
        ImGui::MenuItem(core::Tr("Jeter — verrouillé"));
        ImGui::EndDisabled();
      } else if (ImGui::MenuItem(core::Tr("Jeter"))) {
        if (cell.amount > 1) {
          // Une pile demande COMBIEN. La question s'ouvre à l'image suivante :
          // on ne peut pas ouvrir un surgissant depuis celui qu'on referme.
          state.asking_slot = cell.client_slot;
          state.asked_amount = 1;
          state.asking_verb = ItemVerb::kDrop;
        } else {
          action.verb = ItemVerb::kDrop;
          action.slot = cell.client_slot;
          action.amount = 1;
        }
      }
    }
    // 🔴 ET CE QU'ON PORTE SE RANGE AUSSI. Le serveur ne s'y oppose pas : ni
    // `pc_putitemtocart` ni `storage_canAddItem` ne regardent le drapeau
    // d'équipement, et `pc_delitem` DÉSÉQUIPE la pièce en la retirant du sac.
    // L'écarter ici retirait au joueur un geste que le client d'origine offre —
    // et le commentaire qui l'expliquait inventait une règle serveur.
    //
    // ⚠ Le seul refus de ce genre porte sur `equipSwitch`, le registre
    // d'échange rapide d'équipement, que nous ne modélisons pas encore.
    //
    // L'entrée disparaît en revanche entièrement sans charrette : il n'y a alors
    // pas de règle à expliquer, seulement un contenant qui n'existe pas.
    //
    // UNE ÉCHOPPE EN PRÉPARATION grise les deux sens sous le bandeau du gel.
    DrawTransfersFrozenBanner(view.transfers_frozen && (view.cart_available || view.storage_open));
    ImGui::BeginDisabled(view.transfers_frozen);
    if (view.cart_available) {
      // ⚠ GRISÉE PENDANT QU'UN COFFRE EST OUVERT, ET SOUS SON PROPRE NOM. Le
      // libellé ne change pas — c'est la même entrée, momentanément hors
      // d'atteinte —, et c'est l'infobulle qui porte la règle ET la manœuvre.
      // Un nom rallongé d'un motif dirait la moitié de ce qu'il faut savoir.
      const bool blocked = view.storage_open;
      if (cell.amount <= 1) {
        if (ImGui::MenuItem(core::Tr("Vers le cart"), nullptr, false, !blocked)) {
          action.verb = ItemVerb::kToCart;
          action.slot = cell.client_slot;
          action.amount = 1;
        }
      } else if (ImGui::MenuItem(core::Tr("Vers le cart..."), nullptr, false, !blocked)) {
        state.asking_slot = cell.client_slot;
        state.asked_amount = 1;
        state.asking_verb = ItemVerb::kToCart;
      }
      if (blocked) {
        StorageBlocksTransferTooltip();
      }
    }
    // 🔴 ET LE COFFRE OUVRE CE QUE LA CHARRETTE VIENT DE FERMER. C'est le geste
    // central du coffre — le remplir — et il n'existe que pendant sa session :
    // le serveur n'a nulle part où ranger le reste du temps.
    if (view.storage_open) {
      if (cell.amount <= 1) {
        if (ImGui::MenuItem(core::Tr("Vers le storage"))) {
          action.verb = ItemVerb::kToStorage;
          action.slot = cell.client_slot;
          action.amount = 1;
        }
      } else if (ImGui::MenuItem(core::Tr("Vers le storage..."))) {
        state.asking_slot = cell.client_slot;
        state.asked_amount = 1;
        state.asking_verb = ItemVerb::kToStorage;
      }
    }
    ImGui::EndDisabled();

    // L'ALBUM DE CARTES OUVERT : une CARTE peut y aller. C'est l'album qui décide
    // de la suite — dépôt, ou sacrifice si sa pochette est scellée — et qui
    // demande la quantité : d'où l'absence d'invite ici.
    if (cell.card && view.album_open && !cell.worn) {
      if (ImGui::MenuItem(core::Tr("Vers l'album"))) {
        action.to_album_slot = cell.client_slot;
      }
    }

    // LE COURRIER EN COURS D'ÉCRITURE : même politique de quantité que le
    // rangement — une pile demande combien. Ce qu'on porte ne s'envoie pas.
    if (view.mail_writing && !cell.worn) {
      if (cell.amount <= 1) {
        if (ImGui::MenuItem(core::Tr("Joindre au courrier"))) {
          action.verb = ItemVerb::kToMail;
          action.slot = cell.client_slot;
          action.amount = 1;
        }
      } else if (ImGui::MenuItem(core::Tr("Joindre au courrier..."))) {
        state.asking_slot = cell.client_slot;
        state.asked_amount = 1;
        state.asking_verb = ItemVerb::kToMail;
      }
    }

    // ⚠ SÉPARÉ DES AUTRES : ce n'est pas un geste sur le monde mais un rangement,
    // et le serveur le retient d'une session à l'autre.
    ImGui::Separator();
    if (ImGui::MenuItem(cell.favorite ? core::Tr("Retirer des favoris")
                                      : core::Tr("Mettre en favori"))) {
      action.verb = ItemVerb::kToggleFavorite;
      action.slot = cell.client_slot;
    }
    EndSkinPopup();
  }
  ImGui::PopStyleVar(2);
}

}  // namespace ro::ui
