#include "ro/ui/storage_window.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cstdio>
#include <numeric>
#include <string>

#include <imgui_internal.h>

#include "ro/core/i18n.h"
#include "ro/ui/skin_palette.h"
#include "ro/ui/skin_popup.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"

#include "item_grid.h"
#include "storage_rows.h"
#include "storage_shelves.h"
#include "transfer_freeze.h"

namespace ro::ui {
namespace {

using render::SkinPiece;

// LES DIX ONGLETS DU COFFRE.
//
// 🔴 CE NE SONT PAS LES SEPT DU SAC. Le coffre est la fenêtre où l'on CHERCHE :
// six cents lignes oubliées, et « Équipement » y mêle les dagues, les armures et
// les accessoires. Il sépare donc les ARMES des ARMURES, isole l'APPARAT — qui
// n'est pas un type mais un emplacement — et sort le CASH des consommables.
//
// ⚠ LES FAVORIS SONT EN SECOND, juste après « Tout », et non en queue comme dans
// le sac — un favori de
// coffre est ce qu'on vient chercher, pas un rangement de fin de liste.
//
// ⚠ ET IL N'Y A PAS D'ONGLET « ÉQUIPEMENT » : les trois qui le remplacent le
// couvrent en entier, et le garder ferait voir chaque pièce deux fois.
//
// 🔴 LES FAVORIS SONT À NOUS. Le serveur n'en marque aucun dans un coffre — sa
// structure n'a pas le champ —, et sans cela l'onglet n'aurait rien à montrer.
// La liste est dans un fichier du joueur.
constexpr std::array<ItemTab, 10> kStorageTabs = {
    ItemTab::kAll,     ItemTab::kFavorite, ItemTab::kConsumable, ItemTab::kWeapon, ItemTab::kArmor,
    ItemTab::kCostume, ItemTab::kCard,     ItemTab::kAmmo,       ItemTab::kCash,   ItemTab::kEtc};

// L'identité de la table, gardée à part de la fenêtre : c'est elle qui porte
// l'ordre et la largeur des colonnes dans `imgui.ini`.
constexpr const char* kTableId = "lignes";

// Les largeurs de départ des colonnes étroites, en pixels d'art. La colonne de
// l'objet, elle, prend ce qui reste.
constexpr float kSlotColumnPixels = 54.0F;
constexpr float kItemIdColumnPixels = 60.0F;
constexpr float kAmountColumnPixels = 36.0F;
// La valeur d'une pile tient sept chiffres et son « z » : une pile de mille
// pièces d'un objet cher passe le million.
constexpr float kValueColumnPixels = 72.0F;

// Le mélange d'une empreinte. ⚠ PAS UN HACHAGE POUR COMPARER DES OBJETS : c'est
// un témoin de CHANGEMENT, et une collision ne coûte qu'un tri différé d'une
// image. Comparer les champs serait ici comparer six cents lignes par image.
constexpr std::uint64_t kFingerprintSeed = 1469598103934665603ULL;
constexpr std::uint64_t kFingerprintPrime = 1099511628211ULL;

void MixInto(std::uint64_t& into, std::uint64_t value) {
  into = (into ^ value) * kFingerprintPrime;
}

// Ce qui sépare deux lignes pour le tri : négatif, nul ou positif.
[[nodiscard]] int CompareRows(const InventoryCellView& left, const InventoryCellView& right,
                              int column) {
  const auto by = [](auto a, auto b) { return a < b ? -1 : (a > b ? 1 : 0); };
  if (column == StorageColumnRank(StorageColumn::kSlot)) {
    return by(left.client_slot, right.client_slot);
  }
  if (column == StorageColumnRank(StorageColumn::kItemId)) {
    return by(left.item_id, right.item_id);
  }
  if (column == StorageColumnRank(StorageColumn::kCardSlots)) {
    return by(left.card_slots, right.card_slots);
  }
  if (column == StorageColumnRank(StorageColumn::kAmount)) {
    return by(left.amount, right.amount);
  }
  if (column == StorageColumnRank(StorageColumn::kValue)) {
    return by(left.value, right.value);
  }
  // Par défaut, le NOM. ⚠ Comparé tel quel, donc dans l'ordre des octets : un
  // tri qui suivrait la langue demanderait une collation, et le client
  // d'origine n'en a pas non plus.
  return left.name.compare(right.name);
}

// Le tri, refait seulement quand ce qu'il lit a bougé.
void SortRows(StorageWindowState& state, std::span<const InventoryCellView> cells) {
  ImGuiTableSortSpecs* const specs = ImGui::TableGetSortSpecs();
  std::uint64_t wanted = StorageRowsFingerprint(cells);
  int column = StorageColumnRank(StorageColumn::kItem);
  bool ascending = true;
  if (specs != nullptr && specs->SpecsCount > 0) {
    column = specs->Specs[0].ColumnIndex;
    ascending = specs->Specs[0].SortDirection == ImGuiSortDirection_Ascending;
  }
  MixInto(wanted, static_cast<std::uint64_t>(column));
  MixInto(wanted, ascending ? 1U : 0U);

  const bool dirty = specs != nullptr && specs->SpecsDirty;
  if (!dirty && wanted == state.sorted_for && state.order.size() == cells.size()) {
    return;
  }
  state.order.resize(cells.size());
  std::iota(state.order.begin(), state.order.end(), 0);
  std::ranges::stable_sort(state.order, [&](int left, int right) {
    const int order = CompareRows(cells[static_cast<std::size_t>(left)],
                                  cells[static_cast<std::size_t>(right)], column);
    return ascending ? order < 0 : order > 0;
  });
  state.sorted_for = wanted;
  if (specs != nullptr) {
    specs->SpecsDirty = false;
  }
}

// Les colonnes, TOUTES déclarées, dans l'ordre figé de `StorageColumn`.
//
// 🔴 TROIS SE MONTRENT DEPUIS LE PANNEAU, et de là SEULEMENT. Le menu d'en-tête
// d'ImGui retient son choix dans `imgui.ini` : le laisser faire donnerait deux
// réglages pour la même colonne, dans deux fichiers, et celui qu'on vient de
// poser disparaîtrait au démarrage suivant. `Disabled` les retire de la table
// ET de son menu — il n'y a qu'un endroit où les demander.
//
// ⚠ L'ORDRE DES COLONNES, LUI, RESTE AU JOUEUR : les en-têtes se glissent, et
// `Reorderable` ne se dispute avec rien.
void SetupColumns(const StorageWindowState& state) {
  const auto fixed = ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_PreferSortDescending;
  const auto off = ImGuiTableColumnFlags_Disabled;
  ImGui::TableSetupScrollFreeze(0, 1);
  ImGui::TableSetupColumn(StorageColumnLabel(StorageColumn::kSlot),
                          fixed | (state.slot_column ? 0 : off), kSlotColumnPixels);
  ImGui::TableSetupColumn(StorageColumnLabel(StorageColumn::kItem),
                          ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_DefaultSort |
                              ImGuiTableColumnFlags_NoHide);
  ImGui::TableSetupColumn(StorageColumnLabel(StorageColumn::kItemId),
                          fixed | (state.item_id_column ? 0 : off), kItemIdColumnPixels);
  ImGui::TableSetupColumn(StorageColumnLabel(StorageColumn::kCardSlots),
                          fixed | (state.card_slots_column ? 0 : off),
                          ImGui::CalcTextSize(StorageColumnLabel(StorageColumn::kCardSlots)).x);
  ImGui::TableSetupColumn(
      StorageColumnLabel(StorageColumn::kAmount),
      ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_PreferSortDescending,
      kAmountColumnPixels);
  ImGui::TableSetupColumn(StorageColumnLabel(StorageColumn::kValue),
                          fixed | (state.value_column ? 0 : off), kValueColumnPixels);
  ImGui::TableHeadersRow();
}

// ── CE QUE LA FENÊTRE SAIT FAIRE, ÉCRIT NOIR SUR BLANC ──────────────────────
//
// 🔴 ELLE NE LISTE QUE CE QUI MARCHE. Un geste annoncé qui ne répond pas est
// pire qu'un geste absent : le joueur le réessaie, doute de sa souris, puis du
// reste. Le lien de chat au Maj+clic gauche et la description au Ctrl+clic
// droit n'existent pas encore ici ; ils rejoindront la liste avec ce qu'ils
// décrivent, pas avant.
//
// ⚠ UNE LIGNE PAR ENTRÉE DE CATALOGUE : un pavé écrit d'un bloc n'entrerait
// dans aucune traduction. `TrText` les marque ici, `SkinHelpMarker` les traduit
// et les recolle au survol.
constexpr const char* kStorageHelpTitle = core::TrText("Raccourcis storage");
constexpr std::array<const char*, 7> kStorageShortcuts = {
    core::TrText("- Clic gauche : retrait (Maj ou pile d'un seul : direct ; pile : le menu"
                 " demande combien)"),
    core::TrText("- Un retrait va vers la fenêtre OUVERTE, l'inventory en premier : rien ne part"
                 " si les deux sont fermées"),
    core::TrText("- Clic droit : menu contextuel, qui nomme sa destination"),
    core::TrText("- Alt ou Maj + clic droit : retrait rapide de la pile entière"),
    core::TrText("- Glisser : lâcher sur l'inventory ou le cart pour y transférer"),
    core::TrText("- Entrée : valide la quantité (défaut = pile entière)"),
    core::TrText("- Clic sur un en-tête de colonne : tri ; le glisser : la déplacer"),
};

// LE PIED : le compte, et le bouton qui referme la session.
//
// ⚠ NI POIDS NI BOURSE. Un coffre ne pèse rien pour celui qui le remplit, et
// c'est bien pour cela qu'on s'en sert.
void DrawFooter(const SkinArt& art, StorageTally tally, float footer_height,
                StorageWindowAction& action) {
  const ImVec2 window_at = ImGui::GetWindowPos();
  const ImVec2 window_size = ImGui::GetWindowSize();
  const float top = window_at.y + window_size.y - footer_height;
  const ImVec2 top_left(window_at.x, top);
  const ImVec2 bottom_right(window_at.x + window_size.x, window_at.y + window_size.y);

  ImDrawList* const into = ImGui::GetWindowDrawList();
  DrawFooterBar(into, art, top_left, bottom_right);

  const float middle = top + footer_height * 0.5F;

  std::array<char, 64> line = {};
  std::snprintf(line.data(), line.size(), core::Tr("%d/%d (%d affichés)"), tally.item_count,
                tally.capacity, tally.shown_count);
  float x = top_left.x + kFooterInsetPixels;
  x += DrawFooterIcon(into, art.Get(SkinPiece::kCountIcon), x, middle) + kFooterIconGapPixels;
  const bool full = tally.capacity > 0 && tally.item_count >= tally.capacity;
  into->AddText(ImVec2(x, middle - ImGui::GetTextLineHeight() * 0.5F),
                full ? kItemOverweightText : art.look().text, line.data());

  // 🔴 « QUITTER » N'EST PAS « FERMER LA FENÊTRE » — c'est la même chose, et
  // c'est bien le problème : les deux doivent le dire au serveur. Le bouton du
  // client d'origine est ici, à droite du pied, et la croix de la barre de titre
  // mène au même verbe.
  //
  // 🔴 LE PIED SE REMPLIT DEPUIS LA DROITE, ET UN SEUL CURSEUR L'AVANCE. Le
  // bouton et le marqueur d'aide se plaçaient chacun depuis le bord droit, à
  // deux marges différentes : ils se recouvraient, et le « (?) » dépassait de
  // deux pixels à droite du skin du bouton. Deux ancres sur le même bord, c'est
  // un chevauchement qui n'attend que le premier libellé traduit.
  const char* const leave = core::Tr("Fermer");
  const float width = SkinSmallButtonWidth(leave, art);
  const float button_x = bottom_right.x - kResizeGripMarginPixels - width;
  if (FooterButton(leave, art, ImVec2(button_x, middle), width)) {
    action.verb = StorageVerb::kClose;
  }
  // LE MARQUEUR D'AIDE, JUSTE À GAUCHE DU BOUTON — la même place que dans le
  // sac, où il précède le groupe d'interrupteurs. L'œil la retrouve d'une
  // fenêtre à l'autre.
  FooterHelp(kStorageHelpTitle, kStorageShortcuts,
             ImVec2(button_x - kFooterHelpGapPixels - FooterHelpWidth(), middle));
}

// L'invite du coffre : la question partagée, avec la destination qu'elle vise.
void DrawTakePrompt(std::span<const InventoryCellView> cells, StorageWindowState& state,
                    StorageWindowAction& action, const SkinArt& art) {
  const char* const confirm =
      state.ask_to_cart ? core::Tr("Vers le cart") : core::Tr("Vers l'inventory");
  const QuantityAnswer answer = DrawQuantityPrompt(cells, state.ask, confirm, art);
  if (answer.slot != 0) {
    action.verb = state.ask_to_cart ? StorageVerb::kToCart : StorageVerb::kToInventory;
    action.slot = answer.slot;
    action.amount = answer.amount;
  }
}

}  // namespace

std::span<const ItemTab> StorageTabs() { return kStorageTabs; }

const char* StorageColumnLabel(StorageColumn column) {
  switch (column) {
    case StorageColumn::kSlot:
      return core::Tr("Index");
    case StorageColumn::kItem:
      return core::Tr("Item");
    case StorageColumn::kItemId:
      return core::Tr("ID");
    case StorageColumn::kCardSlots:
      return core::Tr("Slots");
    case StorageColumn::kAmount:
      return core::Tr("Qté");
    case StorageColumn::kValue:
      // ⚠ « PRIX REVENTE » ET NON « PRIX » TOUT COURT : c'est ce que la pile
      // RAPPORTE si on la vend, pas ce qu'elle a coûté — un objet se rachète au
      // tiers. C'est le mot du client.
      return core::Tr("Prix revente");
  }
  return "";
}

std::uint64_t StorageRowsFingerprint(std::span<const InventoryCellView> cells) {
  std::uint64_t mixed = kFingerprintSeed;
  MixInto(mixed, cells.size());
  for (const InventoryCellView& cell : cells) {
    MixInto(mixed, static_cast<std::uint64_t>(cell.value));
    MixInto(mixed, cell.client_slot);
    MixInto(mixed, cell.item_id);
    // ⚠ LA QUANTITÉ EN FAIT PARTIE : un retrait partiel la change sans retirer
    // la ligne, et un tri par quantité resterait sinon celui d'avant.
    MixInto(mixed, static_cast<std::uint64_t>(cell.amount));
  }
  return mixed;
}

StorageWindowAction DrawStorageWindow(StorageWindowState& state, const StorageWindowView& view,
                                      std::span<const InventoryCellView> cells, StorageTally tally,
                                      const SkinArt& art) {
  StorageWindowAction action;
  if (!state.open) {
    return action;
  }

  // ⚠ RECOMPOSÉ SEULEMENT QUAND IL LE FAUT. Le refaire à chaque image coûterait
  // une allocation par image pour un texte qui ne bouge qu'au changement de
  // coffre ou de langue — c'est la règle que `WindowName` tient pour les
  // fenêtres au titre fixe, et le coffre ne peut pas s'en servir : son titre
  // n'est pas un littéral du catalogue mais une donnée du serveur.
  const std::size_t language = core::LanguageSerial();
  if (state.title.empty() || view.name != state.title_for || language != state.title_language) {
    state.title_for = view.name;
    state.title_language = language;
    // ⚠ LE REPLI EST ÉCRIT EN TOUTES LETTRES ICI, et non pris à la constante :
    // le relèvé des traductions ne sait lire qu'un appel porté sur un LITTÉRAL, et
    // passer par un intermédiaire rendrait ce texte invisible aux catalogues.
    state.title = view.name.empty() ? core::Tr("Storage") : view.name;
    state.title += "###";
    state.title += kStorageWindowId;
  }

  ImGui::SetNextWindowSize(ImVec2(kStorageFirstWidthPixels, kStorageFirstHeightPixels),
                           ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSizeConstraints(ImVec2(kStorageFirstWidthPixels, kStorageFirstHeightPixels),
                                      ImVec2(FLT_MAX, FLT_MAX));

  // 🔴 LA CROIX NE FERME PAS LA FENÊTRE, ELLE DEMANDE LA FERMETURE. Le coffre
  // est une session du serveur : c'est lui qui la clôt, et la fenêtre disparaît
  // à sa réponse. La laisser se fermer seule laisserait le personnage sur un
  // coffre que le serveur croit ouvert, donc incapable d'échanger avec sa
  // charrette sans comprendre pourquoi.
  // 🔴 L'ORDRE DE POSE EST L'INVERSE DE L'ORDRE AFFICHÉ, et l'ordre affiché est
  // celui du client : [Inventory][Cart]. Le premier posé va le plus à droite.
  //
  // ⚠ LE RACCOURCI VERS LA CHARRETTE SE GRISE quand le personnage n'en pousse
  // pas, au lieu de disparaître : un bouton absent passe pour un défaut.
  if (state.peer_buttons) {
    SetNextWindowTitleAction(core::Tr("Cart"),
                             !view.cart_available
                                 ? core::Tr("Vous ne possédez pas de Cart")
                                 : (view.cart_open ? core::Tr("Refermer cette fenêtre")
                                                   : core::Tr("Ouvrir cette fenêtre")),
                             view.cart_available);
    SetNextWindowTitleAction(
        core::Tr("Inventory"),
        view.inventory_open ? core::Tr("Refermer cette fenêtre") : core::Tr("Ouvrir cette fenêtre"),
        true);
  }
  SetNextWindowBulletClickable(core::Tr("Réglages de cette fenêtre"));
  // ⛔ PAS D'ÉPINGLE ICI, et c'est la conséquence de la ligne du dessus : une
  // épingle dit « Échap ne me ferme plus », et le coffre n'est déjà PAS dans la
  // pile d'Échap — c'est une session du serveur, que la pile ne saurait pas
  // clore. L'épingle n'aurait donc rien à empêcher.
  bool shown = true;
  const bool body_shown = BeginSkinnedWindow(state.title.c_str(), &shown, art, SkinnedTitle::kBar);
  if (!shown) {
    action.verb = StorageVerb::kClose;
  }
  // 🔴 LA PUCE SE LIT AVANT LE DEMI-TOUR, et c'est la seule façon qu'elle
  // fonctionne REPLIÉE. `BeginSkinnedWindow` rend FAUX sur une fenêtre repliée —
  // son corps ne se dessine pas —, et l'on repartait alors sans jamais demander
  // ce que la barre de titre avait répondu. Or elle, elle répond : ses boutons
  // lisent la souris à la main, précisément pour survivre au repli.
  if (TitleBulletClicked()) {
    action.settings_asked = true;
  }
  if (TitleActionClicked(0)) {
    action.toggle_cart = true;
  }
  if (TitleActionClicked(1)) {
    action.toggle_inventory = true;
  }
  if (!body_shown) {
    EndSkinnedWindow();
    return action;
  }

  const float footer_height = SkinFooterHeight(art);

  if (state.filter_shown) {
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputTextWithHint("##filtre", core::Tr("Filtrer..."), state.filter,
                             sizeof(state.filter));
  }

  // LE SOUS-TYPE, SOUS LE FILTRE ET AU-DESSUS DES ONGLETS.
  //
  // ⚠ IL NE PARAÎT QUE S'IL Y A DE QUOI CHOISIR : un seul sous-type présent
  // n'offre rien, et prendrait une ligne à une table qui en montre six cents.
  //
  // 🔴 ET LE CHOIX SE REPLIE QUAND IL DISPARAÎT. Les sous-types viennent du
  // contenu : changer d'onglet ou de coffre en change la liste, et une clé qui
  // n'y est plus cacherait TOUTES les lignes sans que rien ne l'explique.
  if (!view.sub_categories.empty()) {
    const char* chosen_label = core::Tr("Tout");
    bool still_there = state.sub_category == kAnySubCategory;
    for (const StorageSubCategoryView& sub : view.sub_categories) {
      if (sub.key == state.sub_category) {
        chosen_label = sub.label.data();
        still_there = true;
      }
    }
    if (!still_there) {
      state.sub_category = kAnySubCategory;
    }
    // ⚠ SON INTITULÉ EST À GAUCHE, PAS DANS LA LISTE : un combo qui ne porte
    // que la valeur choisie ne dit pas de QUOI elle est la valeur. C'est la
    // disposition du reste du panneau.
    ImGui::TextUnformatted(core::Tr("Sous-type"));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-FLT_MIN);
    int step = 0;
    const bool open = BeginSkinCombo("##sous_type", chosen_label, art, &step);
    // Le rang zéro est « Tout », les sous-types suivent.
    const auto& subs = view.sub_categories;
    const auto at = std::ranges::find(subs, state.sub_category, &StorageSubCategoryView::key);
    const int rank = at == subs.end() ? 0 : static_cast<int>(at - subs.begin()) + 1;
    if (const int to = SkinComboStepped(rank, step, static_cast<int>(subs.size()) + 1);
        to != rank) {
      state.sub_category = to == 0 ? kAnySubCategory : subs[static_cast<std::size_t>(to - 1)].key;
    }
    if (open) {
      if (ImGui::Selectable(core::Tr("Tout"), state.sub_category == kAnySubCategory)) {
        state.sub_category = kAnySubCategory;
      }
      for (const StorageSubCategoryView& sub : view.sub_categories) {
        ImGui::PushID(sub.key);
        if (ImGui::Selectable(sub.label.data(), sub.key == state.sub_category)) {
          state.sub_category = sub.key;
        }
        ImGui::PopID();
      }
      EndSkinCombo();
    }
  }

  // UNE ÉCHOPPE EN PRÉPARATION GÈLE LE COFFRE : un retrait refusé sans le dire
  // passerait pour une fenêtre cassée.
  DrawTransfersFrozenBanner(view.transfers_frozen);

  // ── CE QUE LE COFFRE VAUT, REVENDU ────────────────────────────────────────
  //
  // 🔴 UNE LIGNE À ELLE SEULE, ET SURTOUT PAS DE `SameLine` : ce qui précède est
  // le combo des sous-types ou le champ de filtre, tous deux en largeur pleine,
  // et le texte se retrouverait dessous. Sa place est au-dessus de la liste,
  // pas dans le pied, où elle se lirait comme une part du compte.
  //
  // ⚠ ET SEULEMENT SI LE SERVEUR A PARLÉ : sans ses prix, un « 0z » se lirait
  // « ce coffre ne vaut rien » au lieu de « je ne sais pas ».
  if (state.total_value && tally.value_known) {
    const std::string total = GroupThousands(tally.shown_value);
    ImGui::Text(core::Tr("Valeur estimée : %s%s"), total.c_str(), std::string(kZenySuffix).c_str());
  }

  const float table_height = -(footer_height + ImGui::GetStyle().ItemSpacing.y);

  // 🔴 LES COFFRES SONT PERPENDICULAIRES AUX CATÉGORIES : deux rangées
  // d'onglets l'une sous l'autre se confondent, alors qu'elles ne coûtent pas
  // la même chose — celle des coffres PARLE AU SERVEUR, qui referme l'un et
  // ouvre l'autre ; celle des catégories ne fait que trier ce qui est déjà là.
  //
  // 🔴 ET C'EST `tabs_vertical` SEUL QUI TRANCHE, jamais la présence des
  // catégories : décocher la bande des catégories ne doit pas faire pivoter les
  // coffres. Le réglage continue de décider de LEUR orientation quand elle n'est
  // plus là — c'est ce que dit son infobulle, et c'est ce que fait le client.
  const TabStripLook tabs_look{
      .lying = !state.tabs_vertical, .images = state.tab_images, .favorite_tinted = false};
  const float tabs_thickness =
      state.type_tabs ? ItemTabsThickness(art, kStorageTabs, tabs_look) : 0.0F;
  // La bande des catégories est-elle RÉELLEMENT debout ? (Elle est désactivable ;
  // sans ce test, la rangée s'indenterait derrière une bande jamais dessinée.)
  const bool standing_cats = state.type_tabs && state.tabs_vertical;
  // ⚠ SOUS DEUX COFFRES IL N'Y A PAS DE BANDE — un onglet seul n'offre aucun
  // choix. Le même test qu'au dessin, sinon on indente derrière du vide.
  const bool shelves_shown = state.shelf_row && view.shelves.size() >= 2;
  const bool shelf_column_left = shelves_shown && !state.tabs_vertical;
  const float shelf_thickness =
      shelf_column_left ? StorageShelfThickness(art, /*lying=*/false) : 0.0F;

  // ⚠ CATÉGORIES DEBOUT (à gauche) → COFFRES EN RANGÉE, au-dessus, et INDENTÉS
  // de la largeur de la bande : sans cela la rangée pend à gauche, en dehors de
  // ce qu'elle commande.
  if (shelves_shown && state.tabs_vertical) {
    if (standing_cats) {
      ImGui::Indent(tabs_thickness);
    }
    DrawStorageShelfRow(state, view, art, /*lying=*/true, 0.0F, action);
    if (standing_cats) {
      ImGui::Unindent(tabs_thickness);
    }
  }

  // ⚠ LES ONGLETS SONT COLLÉS À LA TABLE, émis juste au-dessus d'elle et non en
  // tête de fenêtre : posés avant le filtre, ils flottaient à une ligne de la
  // liste qu'ils commandent.
  //
  // ⚠ ET LA RANGÉE COUCHÉE S'INDENTE DERRIÈRE LA COLONNE DES COFFRES — symétrique
  // exact de l'indent ci-dessus. La colonne décale la table d'autant ; sans cet
  // écart la rangée démarrerait au bord de la fenêtre, au-dessus de rien.
  if (state.type_tabs) {
    if (shelf_column_left) {
      ImGui::Indent(shelf_thickness);
    }
    state.tab =
        DrawItemTabs(kStorageTabs, state.tab, art, tabs_look, tabs_thickness, table_height).chosen;
    if (shelf_column_left) {
      ImGui::Unindent(shelf_thickness);
    }
    if (standing_cats) {
      ImGui::SameLine(0.0F, 0.0F);
    }
  } else {
    // 🔴 SANS BANDE, LA VUE REPASSE À « TOUT ». Un onglet qui filtre sans se
    // montrer cacherait des lignes sans que rien ne l'explique, et le joueur
    // conclurait que son coffre a perdu des objets.
    state.tab = ItemTab::kAll;
  }

  // ⚠ CATÉGORIES COUCHÉES (ou absentes) → COFFRES EN COLONNE, à gauche de la
  // table : la colonne ouvre la ligne, et la table se pose juste à sa droite.
  if (shelf_column_left) {
    DrawStorageShelfRow(state, view, art, /*lying=*/false, table_height, action);
    ImGui::SameLine(0.0F, 0.0F);
  }

  // ⚠ PAS DE `Hideable` : le menu d'en-tête montrait et cachait les mêmes
  // colonnes que le panneau, et les deux se contredisaient d'un démarrage à
  // l'autre — ImGui garde son choix dans `imgui.ini`, nous le nôtre dans le
  // fichier de réglages. Le panneau tranche, seul.
  const auto flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Sortable |
                     ImGuiTableFlags_Reorderable | ImGuiTableFlags_ScrollY |
                     ImGuiTableFlags_SizingStretchProp;
  if (ImGui::BeginTable(kTableId, kStorageColumnCount, flags, ImVec2(0.0F, table_height))) {
    // ⚠ AVANT LA DÉCLARATION DES COLONNES, et consommée tout de suite : la
    // remise à plat efface l'ordre ET les largeurs qu'ImGui garde pour cette
    // table, et une demande qui resterait posée le referait à chaque image.
    if (state.reset_columns) {
      ImGui::TableResetSettings(ImGui::GetCurrentTable());
      state.reset_columns = false;
    }
    SetupColumns(state);
    SortRows(state, cells);
    for (const int rank : state.order) {
      DrawStorageRow(cells[static_cast<std::size_t>(rank)], state, view, action);
    }
    PaintWindowScrollbar(art);
    ImGui::EndTable();
  }
  // CE QU'ON DÉPOSE DANS LE COFFRE : ce qui vient du sac ou de la charrette.
  //
  // ⚠ APRÈS `EndTable`, et pour la même raison que l'enfant des grilles : la
  // cible se pose sur le DERNIER élément soumis, et la table n'en est un
  // qu'une fois refermée.
  action.received = AcceptItemDrop(ItemHolder::kStorage);

  DrawFooter(art, tally, footer_height, action);
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
