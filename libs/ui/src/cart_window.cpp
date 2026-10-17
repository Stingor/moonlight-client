#include "ro/ui/cart_window.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cstdio>
#include <string>

#include "ro/core/i18n.h"
#include "ro/ui/skin_palette.h"
#include "ro/ui/skin_popup.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"
#include "ro/ui/window_name.h"

#include "item_grid.h"
#include "transfer_freeze.h"

namespace ro::ui {
namespace {

using render::SkinPiece;

// La taille d'ouverture. La charrette tient cent objets là où le sac en tient
// deux cents : elle s'ouvre plus courte, et le joueur la garde souvent à côté.
// ── CE QUE LA FENÊTRE SAIT FAIRE, ÉCRIT NOIR SUR BLANC ──────────────────────
//
// 🔴 ELLE NE LISTE QUE CE QUI MARCHE. Un geste annoncé qui ne répond pas est
// pire qu'un geste absent : le joueur le réessaie, doute de sa souris, puis du
// reste. Le lien de chat au Maj+clic gauche et la description au Ctrl+clic
// droit n'y figurent donc pas : ils n'existent pas encore, et rejoindront la
// liste avec ce qu'ils décrivent, pas avant.
//
// ⚠ UNE LIGNE PAR ENTRÉE DE CATALOGUE : un pavé écrit d'un bloc n'entrerait
// dans aucune traduction. `TrText` les marque ici, `SkinHelpMarker` les traduit
// et les recolle au survol.
constexpr const char* kCartHelpTitle = core::TrText("Raccourcis cart");
constexpr std::array<const char*, 4> kCartShortcuts = {
    core::TrText("- Double-clic gauche : vers le storage s'il est ouvert, sinon l'inventory"),
    core::TrText("- Clic droit : menu contextuel"),
    core::TrText("- Alt + clic droit : transfert rapide (storage si ouvert, sinon inventory)"),
    core::TrText("- Glisser : lâcher sur l'inventory ou le storage pour y transférer"),
};

constexpr float kFirstWidthPixels = 260.0F;
constexpr float kFirstHeightPixels = 270.0F;

// LES SIX ONGLETS, ET IL EN MANQUE UN AU SAC.
//
// ⚠ PAS DE FAVORIS. L'onglet des favoris est un rangement du SAC, que le serveur
// retient par objet ; une charrette n'en a pas, et lui en dessiner un montrerait
// une case toujours vide.
constexpr std::array<ItemTab, 6> kCartTabs = {ItemTab::kAll,       ItemTab::kConsumable,
                                              ItemTab::kEquipment, ItemTab::kAmmo,
                                              ItemTab::kEtc,       ItemTab::kCard};

// Le menu d'une case : deux destinations, et elles se relaient.
//
// 🔴 LA PREMIÈRE NE DISPARAÎT JAMAIS, elle se GRISE. Tant qu'un storage est
// ouvert, le serveur refuse en silence le retour vers le sac ; l'entrée reste
// sous son propre nom, hors d'atteinte, et c'est l'infobulle qui porte la règle
// ET la manœuvre — passer par le storage. Une entrée escamotée ferait croire à
// un défaut, et un libellé rallongé d'un motif ne dirait pas comment s'en
// sortir.
void DrawCellMenu(const InventoryCellView& cell, ImVec2 menu_padding, ImVec2 menu_spacing,
                  CartWindowAction& action, CartWindowState& state, const CartWindowView& view) {
  const bool blocked = view.storage_open;
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, menu_padding);
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, menu_spacing);
  if (BeginSkinPopupContextItem("menu")) {
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
    // ⚠ LES TROIS POINTS SONT UNE PROMESSE : ils disent qu'une question vient.
    // Une pile s'en sort en partie, un objet seul n'a rien à demander.
    if (cell.amount <= 1) {
      if (ImGui::MenuItem(core::Tr("Vers l'inventory"), nullptr, false, !blocked)) {
        action.verb = CartVerb::kToInventory;
        action.slot = cell.client_slot;
        action.amount = 1;
      }
    } else if (ImGui::MenuItem(core::Tr("Vers l'inventory..."), nullptr, false, !blocked)) {
      state.ask.slot = cell.client_slot;
      state.ask.amount = 1;
      state.ask_to_storage = false;
    }
    if (blocked) {
      StorageBlocksTransferTooltip();
    }
    // Et la seconde n'existe QUE pendant la session : le serveur n'a nulle part
    // où ranger le reste du temps. Elle s'ouvre donc à l'instant même où l'autre
    // se ferme.
    if (view.storage_open) {
      if (cell.amount <= 1) {
        if (ImGui::MenuItem(core::Tr("Vers le storage"))) {
          action.verb = CartVerb::kToStorage;
          action.slot = cell.client_slot;
          action.amount = 1;
        }
      } else if (ImGui::MenuItem(core::Tr("Vers le storage..."))) {
        state.ask.slot = cell.client_slot;
        state.ask.amount = 1;
        state.ask_to_storage = true;
      }
    }
    ImGui::EndDisabled();
    EndSkinPopup();
  }
  ImGui::PopStyleVar(2);
}

// LE PIED : le poids, puis le compte. Deux lignes, et pas de bourse.
//
// ⚠ L'ORDRE EST CELUI DU SAC, pas celui du client. Le client d'origine met les
// deux sur une seule ligne, le compte d'abord ; sur deux lignes il n'y a donc
// pas d'ordre natif à respecter, et le même que le sac est ce qui permet de lire
// les deux fenêtres côte à côte sans changer d'habitude.
void DrawFooter(const SkinArt& art, CartTally tally, ImVec2 top_left, ImVec2 bottom_right) {
  ImDrawList* const into = ImGui::GetWindowDrawList();
  DrawFooterBar(into, art, top_left, bottom_right);

  const float line_height = ImGui::GetTextLineHeightWithSpacing();
  const float first_y = top_left.y + line_height * 0.5F;
  const float second_y = first_y + line_height;
  const ImU32 ordinary = art.look().text;

  FooterHelp(kCartHelpTitle, kCartShortcuts,
             ImVec2(bottom_right.x - kResizeGripMarginPixels - FooterHelpWidth(), second_y));

  std::array<char, 64> line = {};
  // 🔴 ROUGE QUAND ELLE EST PLEINE, et ce n'est pas le seuil du sac. Le sac
  // rougit à une PART de son maximum, que le serveur annonce ; la charrette n'a
  // pas de seuil — elle est pleine ou elle ne l'est pas.
  const bool heavy = tally.max_weight > 0 && tally.weight >= tally.max_weight;
  std::snprintf(line.data(), line.size(), "%lld/%lld",
                static_cast<long long>(tally.weight / kWeightTenths),
                static_cast<long long>(tally.max_weight / kWeightTenths));
  float x = top_left.x + kFooterInsetPixels;
  x += DrawFooterIcon(into, art.Get(SkinPiece::kWeightIcon), x, first_y) + kFooterIconGapPixels;
  into->AddText(ImVec2(x, first_y - line_height * 0.5F), heavy ? kItemOverweightText : ordinary,
                line.data());

  const bool full = tally.capacity > 0 && tally.item_count >= tally.capacity;
  std::snprintf(line.data(), line.size(), "%d/%d", tally.item_count, tally.capacity);
  x = top_left.x + kFooterInsetPixels;
  x += DrawFooterIcon(into, art.Get(SkinPiece::kCountIcon), x, second_y) + kFooterIconGapPixels;
  into->AddText(ImVec2(x, second_y - line_height * 0.5F), full ? kItemOverweightText : ordinary,
                line.data());
  static_cast<void>(bottom_right);
}

// L'invite de la charrette : la question partagée, avec le verbe qu'elle
// confirme. ⚠ CELUI QUI ÉTAIT OFFERT QUAND ON A DEMANDÉ, et non celui qui l'est
// maintenant : le coffre a pu se fermer entre la question et la réponse, et
// changer la destination sous le doigt du joueur serait pire que refuser.
void DrawTakePrompt(std::span<const InventoryCellView> cells, CartWindowState& state,
                    CartWindowAction& action, const SkinArt& art) {
  const char* const confirm =
      state.ask_to_storage ? core::Tr("Vers le storage") : core::Tr("Retirer");
  const QuantityAnswer answer = DrawQuantityPrompt(cells, state.ask, confirm, art);
  if (answer.slot != 0) {
    action.verb = state.ask_to_storage ? CartVerb::kToStorage : CartVerb::kToInventory;
    action.slot = answer.slot;
    action.amount = answer.amount;
  }
}

}  // namespace

std::span<const ItemTab> CartTabs() { return kCartTabs; }

CartWindowAction DrawCartWindow(CartWindowState& state, const CartWindowView& view,
                                std::span<const InventoryCellView> cells, CartTally tally,
                                const SkinArt& art) {
  CartWindowAction action;
  if (!state.open) {
    return action;
  }

  // ⚠ POSÉ AVANT LE `Begin`, comme les `SetNextWindow…` d'ImGui : la barre de
  // titre se peint DANS l'ouverture, et rien ne doit rester à garder entre deux
  // images.
  // Affiché [Storage][Inventory], donc posé dans l'autre sens. Voir le sac.
  if (state.storage_button) {
    SetNextWindowTitleAction(
        core::Tr("Inventory"),
        view.inventory_open ? core::Tr("Refermer cette fenêtre") : core::Tr("Ouvrir cette fenêtre"),
        true);
    SetNextWindowTitleAction(
        core::Tr("Storage"),
        view.storage_open ? core::Tr("Refermer le storage") : core::Tr("Ouvrir le storage"), true);
  }
  SetNextWindowBulletClickable(core::Tr("Réglages de cette fenêtre"));
  SetNextWindowPinnable(&state.pinned);
  static WindowName name(kCartWindowTitle, kCartWindowId);
  ImGui::SetNextWindowSize(ImVec2(kFirstWidthPixels, kFirstHeightPixels), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSizeConstraints(ImVec2(0.0F, 0.0F), ImVec2(FLT_MAX, FLT_MAX), SnapToCells,
                                      &state.chrome);
  const ImGuiWindowFlags flags =
      state.size_locked ? ImGuiWindowFlags_NoResize : ImGuiWindowFlags_None;
  const bool body_shown =
      BeginSkinnedWindow(name.Get(), &state.open, art, SkinnedTitle::kBar, flags);
  // 🔴 LA PUCE SE LIT AVANT LE DEMI-TOUR, et c'est la seule façon qu'elle
  // fonctionne REPLIÉE. `BeginSkinnedWindow` rend FAUX sur une fenêtre repliée —
  // son corps ne se dessine pas —, et l'on repartait alors sans jamais demander
  // ce que la barre de titre avait répondu. Or elle, elle répond : ses boutons
  // lisent la souris à la main, précisément pour survivre au repli.
  if (TitleBulletClicked()) {
    action.settings_asked = true;
  }
  if (TitleActionClicked(0)) {
    action.toggle_inventory = true;
  }
  if (TitleActionClicked(1)) {
    action.toggle_storage = true;
  }
  if (!body_shown) {
    EndSkinnedWindow();
    return action;
  }

  const float footer_height =
      static_cast<float>(kFooterLines) * ImGui::GetTextLineHeightWithSpacing();

  if (state.filter_shown) {
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputTextWithHint("##filtre", core::Tr("Filtrer..."), state.filter,
                             sizeof(state.filter));
  }
  DrawTransfersFrozenBanner(view.transfers_frozen);

  const TabStripLook tabs_look{.lying = !state.tabs_vertical, .images = true};
  const float tabs_thickness = ItemTabsThickness(art, kCartTabs, tabs_look);
  const TabStripResult strip =
      DrawItemTabs(kCartTabs, state.tab, art, tabs_look, tabs_thickness, -footer_height);
  state.tab = strip.chosen;
  // ⚠ SEULE LA BANDE DEBOUT PARTAGE SA LIGNE avec la grille ; la rangée a pris
  // la sienne.
  if (state.tabs_vertical) {
    ImGui::SameLine(0.0F, 0.0F);
  }

  const ImVec2 menu_padding = ImGui::GetStyle().WindowPadding;
  const ImVec2 menu_spacing = ImGui::GetStyle().ItemSpacing;
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(kCellGapPixels, kCellGapPixels));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0F, 0.0F));
  // 🔴 LA GRILLE PORTE UNE BORDURE, et c'est le dessin du client : un liseré d'un
  // pixel autour des cases, qui sépare le fond pavé du corps de la fenêtre. Sans
  // elle la grille flotte, et surtout le pont de l'onglet actif n'a plus rien à
  // manger — les sept onglets paraissent alors tous fermés.
  const bool grid_open =
      ImGui::BeginChild("grille", ImVec2(0.0F, -footer_height), ImGuiChildFlags_Borders,
                        GridScrollFlags(state.size_locked));
  // ⚠ DEDANS, ET AVANT LE DEMI-TOUR : une grille entièrement rognée ne dessine
  // rien mais garde sa gouttière, et un chrome mesuré sans elle ferait sauter la
  // fenêtre d'une case à l'image suivante.
  const float scrollbar_gutter = GridScrollbarGutter();
  if (grid_open) {
    ImDrawList* const into = ImGui::GetWindowDrawList();
    const float cell_side = kItemCellPixels;
    const int columns =
        ItemGridColumns(ImGui::GetContentRegionAvail().x, cell_side, kCellGapPixels);
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 corner(origin.x + ImGui::GetContentRegionAvail().x,
                        ImGui::GetWindowPos().y + ImGui::GetWindowSize().y);
    DrawTiledBackground(into, art.Get(SkinPiece::kItemTile), origin,
                        ImVec2(ImGui::GetWindowPos().x, ImGui::GetWindowPos().y), corner,
                        art.look());

    int column = 0;
    for (const InventoryCellView& cell : cells) {
      if (column > 0) {
        ImGui::SameLine(0.0F, kCellGapPixels);
      }
      ImGui::PushID(static_cast<int>(cell.client_slot));
      const ImVec2 top_left = ImGui::GetCursorScreenPos();
      ImGui::InvisibleButton("case", ImVec2(cell_side, cell_side),
                             ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
      const ImVec2 bottom_right(top_left.x + cell_side, top_left.y + cell_side);
      if (ImGui::IsItemHovered()) {
        action.hovered_slot = cell.client_slot;
        action.hovered_id = cell.item_id;
        into->AddRectFilled(top_left, bottom_right, kItemHoverVeil, 0.0F);
      }
      // ⚠ AVANT LE DESSIN ET AVANT L'INFOBULLE : ImGui rattache la source au
      // DERNIER objet posé, et une infobulle est une fenêtre.
      // Le même refus, dans l'autre sens : voir le sac. Le gel passe devant.
      const char* refusal = view.transfers_frozen ? TransfersFrozenText()
                            : view.storage_open && MouseOverWindow(kInventoryWindowId)
                                ? core::Tr("Storage ouvert : vers l'inventory impossible")
                                : nullptr;
      DragItemFrom(cell, ItemHolder::kCart, menu_padding, refusal);
      DrawIcon(into, cell, top_left, cell_side);
      DrawBadge(into, cell, top_left, bottom_right);
      if (ImGui::IsItemHovered()) {
        if (!state.hover_preview) {
          DrawCellTooltip(cell, menu_padding);
        }
        // ⚠ LA PILE ENTIÈRE, SANS QUESTION. C'est le geste du client : le
        // double-clic vide la case, et c'est le menu qui demande une part.
        // 🔴 LE GESTE RAPIDE, LUI, BASCULE — et c'est le client d'origine qui le
        // fait ainsi : son transfert rapide vise le storage dès qu'il est
        // ouvert. Un double-clic n'a pas de libellé à lire ; il doit donc mener
        // là où quelque chose peut arriver, plutôt que de ne rien faire.
        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
          action.verb = view.storage_open ? CartVerb::kToStorage : CartVerb::kToInventory;
          action.slot = cell.client_slot;
          action.amount = cell.amount;
        }
      }
      // ALT + CLIC DROIT : le transfert rapide, pile entière et sans question.
      // Il vise le storage dès qu'il est ouvert, l'inventory sinon — le même
      // ordre que le double-clic, et pour la même raison.
      //
      // ⚠ AVANT LE MENU, qui s'ouvre sur le même bouton et prendrait le clic.
      // MAJ + CLIC GAUCHE : le lien de l'objet dans la saisie du chat.
      //
      // ⚠ À L'ENFONCEMENT, contrairement aux gestes du bouton droit, et c'est
      // celui du client : rien ne s'ouvre ici, donc il n'y a pas de fenêtre à
      // faire naître derrière celle-ci.
      if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && ImGui::GetIO().KeyShift) {
        action.link_slot = cell.client_slot;
      }
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
      } else if (right_clicked && keys.KeyAlt) {
        action.verb = view.storage_open ? CartVerb::kToStorage : CartVerb::kToInventory;
        action.slot = cell.client_slot;
        action.amount = cell.amount;
      } else {
        DrawCellMenu(cell, menu_padding, menu_spacing, action, state, view);
      }
      ImGui::PopID();
      if (++column >= columns) {
        column = 0;
      }
    }
    PaintWindowScrollbar(art);
  }
  ImGui::EndChild();
  // CE QU'ON RANGE DANS LA CHARRETTE : ce qui vient du sac ou du coffre.
  //
  // ⚠ APRÈS `EndChild` : la cible se pose sur le DERNIER élément soumis, et
  // l'enfant n'en est un qu'une fois refermé.
  action.received = AcceptItemDrop(ItemHolder::kCart);
  // 🔴 CE QUI N'EST PAS LA GRILLE SE MESURE SUR LA GRILLE, et non sur ce qui
  // RESTE sous elle. `GetContentRegionAvail` après l'enfant ne rend que la
  // bande laissée au pied — quelques pixels —, si bien que le « chrome » valait
  // presque la fenêtre entière. Le calage en déduisait qu'il n'y avait plus de
  // place pour une seule rangée, tombait sur son plancher de cinq, et
  // REDEMANDAIT une fenêtre plus haute — à chaque image, jusqu'au plafond des
  // 90 % d'écran. La charrette s'ouvrait donc toujours géante.
  //
  // 🔴 ET LA GOUTTIÈRE DE L'ASCENSEUR EST DU CHROME, elle aussi : le rectangle
  // de l'enfant la contient, si bien qu'une fenêtre calée sur sept cases n'en
  // logeait que six et gardait contre l'ascenseur une bande de fond pavé trop
  // étroite pour une case.
  //
  // ⚠ APRÈS LA FERMETURE, jamais avant : l'enfant n'est un ITEM de la fenêtre
  // qu'une fois refermé. Et AVANT le pied, qui pose maintenant des éléments.
  const ImVec2 window_size = ImGui::GetWindowSize();
  state.chrome = ImVec2(window_size.x - ImGui::GetItemRectSize().x + scrollbar_gutter,
                        window_size.y - ImGui::GetItemRectSize().y);
  ImGui::PopStyleVar(2);
  // ⚠ APRÈS L'ENFANT, jamais dedans : c'est la bordure QU'IL VIENT DE PEINDRE
  // qu'il s'agit de manger, et un pont posé avant passerait dessous.
  DrawActiveTabBridge(ImGui::GetWindowDrawList(), strip, art.look());

  const ImVec2 window_at = ImGui::GetWindowPos();
  DrawFooter(art, tally, ImVec2(window_at.x, window_at.y + window_size.y - footer_height),
             ImVec2(window_at.x + window_size.x, window_at.y + window_size.y));
  DrawTakePrompt(cells, state, action, art);
  EndSkinnedWindow();
  // ⚠ UN FILTRE MASQUÉ SE VIDE, comme dans le sac : sinon il continuerait de
  // cacher des objets sans que rien ne l'explique.
  if (!state.filter_shown) {
    state.filter[0] = '\0';
  }
  return action;
}

}  // namespace ro::ui
