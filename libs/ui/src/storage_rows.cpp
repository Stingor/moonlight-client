// Une ligne du coffre. Voir l'en-tête.

#include "storage_rows.h"

#include <array>
#include <cmath>
#include <cstdio>
#include <string>

#include "ro/core/i18n.h"
#include "ro/ui/skin_popup.h"

#include "item_grid.h"
#include "transfer_freeze.h"

namespace ro::ui {
namespace {

// L'ÉTOILE D'UN FAVORI, posée sur le coin haut-gauche de son icône.
//
// ⚠ DESSINÉE ET NON PEINTE : l'installation n'a pas d'image d'étoile à cette
// taille. Cinq branches, un
// contour sombre pour qu'elle se détache d'une icône claire.
constexpr ImU32 kFavoriteStarBody = IM_COL32(255, 205, 40, 255);
constexpr ImU32 kFavoriteStarEdge = IM_COL32(60, 40, 0, 220);
constexpr float kFavoriteStarRadius = 5.0F;
constexpr int kStarPoints = 5;
// Le creux d'une étoile à cinq branches : la moitié du rayon, à peu de chose
// près — c'est ce rapport qui la fait lire comme une étoile et non comme une
// fleur.
constexpr float kStarInnerRatio = 0.5F;

void DrawFavoriteStar(ImDrawList* into, ImVec2 center, float radius) {
  std::array<ImVec2, kStarPoints * 2> points{};
  // ⚠ LA PREMIÈRE POINTE EN HAUT : un quart de tour en arrière, sinon l'étoile
  // est posée sur la pointe et se lit de travers.
  constexpr float kQuarterTurn = 1.57079632679F;
  for (int rank = 0; rank < kStarPoints * 2; ++rank) {
    const float angle = static_cast<float>(rank) * 3.14159265358979F / kStarPoints - kQuarterTurn;
    const float reach = rank % 2 == 0 ? radius : radius * kStarInnerRatio;
    points[static_cast<std::size_t>(rank)] =
        ImVec2(center.x + std::cos(angle) * reach, center.y + std::sin(angle) * reach);
  }
  into->AddConvexPolyFilled(points.data(), static_cast<int>(points.size()), kFavoriteStarBody);
  into->AddPolyline(points.data(), static_cast<int>(points.size()), kFavoriteStarEdge,
                    ImDrawFlags_Closed, 1.0F);
}

// Ce qu'une ligne réserve à son icône. Calé sur la hauteur d'une rangée de
// texte : une icône plus haute ferait respirer la table
// d'une ligne sur deux.
constexpr float kRowIconPixels = 22.0F;

// CE QU'ON ÉCRIT QUAND LE SERVEUR N'A RIEN DIT DU PRIX. ⚠ PAS TRADUIT : c'est un
// signe de ponctuation, et le traduire n'aurait aucun sens.
constexpr const char* kUnknownValue = "--";

// OÙ PART CE QU'ON RETIRE SANS LE DIRE — le clic gauche, le retrait rapide.
//
// 🔴 VERS UNE FENÊTRE OUVERTE, ET SEULEMENT VERS ELLE. Un geste sans libellé ne
// doit jamais aboutir là où le joueur ne regarde pas : retirer vers un sac
// fermé donne l'impression que rien ne s'est passé, et il faut aller ouvrir la
// fenêtre pour découvrir où l'objet est allé.
//
// ⚠ L'INVENTORY GAGNE QUAND LES DEUX SONT OUVERTES : c'est la destination que le
// client donne à ce geste depuis toujours, et la charrette reste atteignable par
// son nom — le menu, ou le glisser.
//
// ⚠ ET RIEN NE PART QUAND LES DEUX SONT FERMÉES. `kNothing` le dit ; l'appelant
// en informe le joueur plutôt que de laisser le clic tomber dans le vide.
[[nodiscard]] StorageVerb WithdrawalTarget(const StorageWindowView& view) {
  if (view.inventory_open) {
    return StorageVerb::kToInventory;
  }
  if (view.cart_open && view.cart_available) {
    return StorageVerb::kToCart;
  }
  return StorageVerb::kNothing;
}

// Le menu d'une ligne : les deux sens sortants, et rien d'autre.
void DrawRowMenu(const InventoryCellView& cell, StorageWindowState& state,
                 const StorageWindowView& view, StorageWindowAction& action) {
  if (!BeginSkinPopupContextItem("menu")) {
    return;
  }
  ImGui::TextDisabled(" %s ", std::string(cell.name).c_str());
  ImGui::Separator();
  if (ImGui::MenuItem(cell.favorite ? core::Tr("Retirer des favoris")
                                    : core::Tr("Ajouter aux favoris"))) {
    action.toggle_favorite_id = cell.item_id;
  }
  ImGui::Separator();
  if (ImGui::MenuItem(core::Tr("Description"))) {
    action.describe_id = cell.item_id;
    action.describe_slot = cell.client_slot;
  }
  if (SeeInAlbumMenuItem(CardAlbumEntryOf(view.album_links, cell.item_id, cell.card))) {
    action.see_in_album_id = cell.item_id;
  }
  ImGui::Separator();

  // UNE ÉCHOPPE EN PRÉPARATION : les entrées restent, grisées sous le bandeau
  // qui dit pourquoi.
  DrawTransfersFrozenBanner(view.transfers_frozen);
  ImGui::BeginDisabled(view.transfers_frozen);
  // ⚠ LA DESTINATION EST NOMMÉE, jamais « Retirer » seul : la charrette est une
  // destination aussi légitime que le sac, et le mot est ce qui les sépare.
  if (ImGui::MenuItem(core::Tr("Vers l'inventory (1)"))) {
    action.verb = StorageVerb::kToInventory;
    action.slot = cell.client_slot;
    action.amount = 1;
  }
  if (cell.amount > 1) {
    std::array<char, 64> label = {};
    std::snprintf(label.data(), label.size(), core::Tr("Vers l'inventory (tout : %d)"),
                  cell.amount);
    if (ImGui::MenuItem(label.data())) {
      action.verb = StorageVerb::kToInventory;
      action.slot = cell.client_slot;
      action.amount = cell.amount;
    }
    if (ImGui::MenuItem(core::Tr("Vers l'inventory..."))) {
      state.ask.slot = cell.client_slot;
      state.ask.amount = 1;
      state.ask_to_cart = false;
    }
  }

  // ⚠ ET LA CHARRETTE EST OFFERTE MÊME COFFRE OUVERT : le serveur n'oppose son
  // refus silencieux qu'aux échanges entre le sac et la charrette. Ces deux
  // sens-ci ne passent pas par `pc_cant_act2`.
  if (view.cart_available) {
    ImGui::Separator();
    if (ImGui::MenuItem(core::Tr("Vers le cart (1)"))) {
      action.verb = StorageVerb::kToCart;
      action.slot = cell.client_slot;
      action.amount = 1;
    }
    if (cell.amount > 1) {
      std::array<char, 64> label = {};
      std::snprintf(label.data(), label.size(), core::Tr("Vers le cart (tout : %d)"), cell.amount);
      if (ImGui::MenuItem(label.data())) {
        action.verb = StorageVerb::kToCart;
        action.slot = cell.client_slot;
        action.amount = cell.amount;
      }
      if (ImGui::MenuItem(core::Tr("Vers le cart..."))) {
        state.ask.slot = cell.client_slot;
        state.ask.amount = 1;
        state.ask_to_cart = true;
      }
    }
  }
  ImGui::EndDisabled();
  EndSkinPopup();
}

}  // namespace

// Une ligne.
void DrawStorageRow(const InventoryCellView& cell, StorageWindowState& state,
                    const StorageWindowView& view, StorageWindowAction& action) {
  ImGui::TableNextRow();
  // 🔴 CHAQUE CELLULE VISE SA COLONNE PAR SON RANG, jamais « la suivante » :
  // l'ordre d'affichage appartient au joueur, l'ordre de déclaration au code.
  // Le retour dit si la colonne est rendue — masquée, il n'y a rien à poser.
  if (ImGui::TableSetColumnIndex(StorageColumnRank(StorageColumn::kSlot))) {
    ImGui::Text("%u", static_cast<unsigned>(cell.client_slot));
  }

  ImGui::TableSetColumnIndex(StorageColumnRank(StorageColumn::kItem));
  const ImVec2 icon_at = ImGui::GetCursorScreenPos();
  ImGui::Dummy(ImVec2(kRowIconPixels, kRowIconPixels));
  DrawIcon(ImGui::GetWindowDrawList(), cell, icon_at, kRowIconPixels);
  // ⚠ APRÈS L'ICÔNE, donc PAR-DESSUS : la même liste de dessin, et c'est l'ordre
  // qui décide de ce qui recouvre quoi.
  if (cell.favorite) {
    DrawFavoriteStar(ImGui::GetWindowDrawList(),
                     ImVec2(icon_at.x + kFavoriteStarRadius, icon_at.y + kFavoriteStarRadius),
                     kFavoriteStarRadius);
  }
  ImGui::SameLine();

  ImGui::PushID(static_cast<int>(cell.client_slot));
  // ⚠ MAJ OU PILE D'UN SEUL : la ligne part entière. Sinon le menu s'ouvre, qui
  // est le seul endroit où l'on choisit une part — comme dans le client.
  if (ImGui::Selectable(std::string(cell.name).c_str())) {
    if (ImGui::GetIO().KeyCtrl) {
      // CTRL : (dé)favori. ⚠ C'EST LE GESTE DU SAC À UNE
      // TOUCHE PRÈS — là-bas c'est Maj + clic DROIT —, et l'écart vient du
      // client : une ligne de coffre se clique à gauche, une case de sac se
      // double-clique.
      action.toggle_favorite_id = cell.item_id;
    } else if (ImGui::GetIO().KeyShift || cell.amount <= 1) {
      // ⚠ LE VERBE DÉCIDE MÊME QUAND IL EST `kNothing` : l'appelant lit alors
      // une place NULLE, et sait qu'il n'y avait nulle part où retirer.
      action.verb = WithdrawalTarget(view);
      action.slot = action.verb == StorageVerb::kNothing ? 0 : cell.client_slot;
      action.amount = cell.amount;
      action.nowhere_to_take = action.verb == StorageVerb::kNothing;
    } else {
      ImGui::OpenPopup("menu");
    }
  }
  // ⚠ AVANT L'INFOBULLE ET LE MENU : ImGui rattache la source au DERNIER objet
  // posé. La marge du fantôme est celle du style courant — la table du coffre ne
  // met pas la sienne à zéro, contrairement aux grilles.
  // ⚠ LE SEUL REFUS À ANNONCER EST LE GEL : le serveur n'oppose son silence
  // qu'aux échanges entre le sac et la charrette, et les deux sens du coffre
  // n'en sont pas.
  DragItemFrom(cell, ItemHolder::kStorage, ImGui::GetStyle().WindowPadding,
               view.transfers_frozen ? TransfersFrozenText() : nullptr);
  if (ImGui::IsItemHovered()) {
    action.hovered_slot = cell.client_slot;
    action.hovered_id = cell.item_id;
  }
  // ALT OU MAJ + CLIC DROIT : le retrait rapide de la pile entière vers
  // l'inventory. ⚠ LES DEUX TOUCHES, et c'est le client : le coffre n'a pas de
  // favori à basculer au Maj, la touche est donc libre et sert le même geste.
  //
  // ⚠ AVANT LE MENU, qui s'ouvre sur le même bouton et prendrait le clic.
  // 🔴 AU RELÂCHEMENT, JAMAIS À L'ENFONCEMENT. La fenêtre de description naît
  // à l'image suivante : partie de l'enfoncement, elle naissait pendant que
  // le bouton était encore tenu sur CETTE fenêtre-ci, qui reprenait le
  // premier plan — et la description s'ouvrait DERRIÈRE elle.
  //
  // ⚠ ET C'EST LE BORD DU MENU. `BeginPopupContextItem` s'ouvre lui aussi au
  // relâchement : sur l'enfoncement, les deux gestes ne se départageaient
  // plus par leur touche mais par leur instant.
  const ImGuiIO& keys = ImGui::GetIO();
  const bool right_clicked =
      ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Right);
  if (right_clicked && keys.KeyCtrl) {
    action.describe_id = cell.item_id;
    action.describe_slot = cell.client_slot;
  } else if (right_clicked && (keys.KeyAlt || keys.KeyShift)) {
    action.verb = WithdrawalTarget(view);
    action.slot = action.verb == StorageVerb::kNothing ? 0 : cell.client_slot;
    action.amount = cell.amount;
    action.nowhere_to_take = action.verb == StorageVerb::kNothing;
  } else {
    DrawRowMenu(cell, state, view, action);
  }
  ImGui::PopID();

  if (ImGui::TableSetColumnIndex(StorageColumnRank(StorageColumn::kItemId))) {
    ImGui::Text("%u", cell.item_id);
  }
  if (ImGui::TableSetColumnIndex(StorageColumnRank(StorageColumn::kCardSlots))) {
    if (cell.card_slots > 0) {
      ImGui::Text("%d", static_cast<int>(cell.card_slots));
    } else {
      ImGui::TextDisabled("-");
    }
  }
  if (ImGui::TableSetColumnIndex(StorageColumnRank(StorageColumn::kAmount))) {
    ImGui::Text("%d", cell.amount);
  }
  if (ImGui::TableSetColumnIndex(StorageColumnRank(StorageColumn::kValue))) {
    // ⚠ UN TIRET ET NON UN ZÉRO quand le serveur n'a rien dit : « zéro zeny »
    // est un prix, « je ne sais pas » n'en est pas un. Les deux se ressemblent
    // à l'œil et n'ont pas le même sens devant un coffre qu'on trie.
    if (cell.value > 0) {
      const std::string amount = GroupThousands(cell.value);
      ImGui::Text("%s%s", amount.c_str(), std::string(kZenySuffix).c_str());
    } else {
      ImGui::TextDisabled("%s", kUnknownValue);
    }
  }
}

}  // namespace ro::ui
