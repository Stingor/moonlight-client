// LA FICHE D'UN OBJET, un seul dessin pour ses deux hôtes : la fenêtre de
// description et le panneau de droite de la base de données (ADR 0116). Sous
// l'en-tête de l'hôte, un onglet par sujet — ce que le jeu en dit, ce que la
// base en sait, ce qu'il rend, qui le lâche, ses scripts, ses combos, qui le
// vend.

#include <array>
#include <cstddef>
#include <cstdio>
#include <string>
#include <string_view>

#include <imgui.h>
#include <imgui_internal.h>

#include "ro/core/i18n.h"
#include "ro/ui/item_link_cell.h"
#include "ro/ui/skin_tooltip.h"
#include "ro/ui/skin_widgets.h"

#include "description_odds.h"
#include "table_cells.h"
#include "ui_scale.h"
#include "ui_text.h"

namespace ro::ui {
namespace {

// Le nom de l'objet, dans le brun des titres de description.
constexpr ImU32 kTitleColor = IM_COL32(115, 61, 5, 255);
constexpr float kIconPixels = 32.0F;
constexpr int kFactColumns = 2;
constexpr int kSellerColumns = 4;
constexpr std::size_t kLabelBytes = 96;

constexpr std::array<ItemSheetTab, 8> kTabs = {
    ItemSheetTab::kDescription, ItemSheetTab::kGeneral, ItemSheetTab::kOdds,
    ItemSheetTab::kMonsters,    ItemSheetTab::kBoss,    ItemSheetTab::kScript,
    ItemSheetTab::kCombos,      ItemSheetTab::kSellers,
};

// L'ONGLET EXISTE-T-IL pour cet objet ? Un onglet du serveur existe dès qu'on
// peut lui répondre ; il se grise quand la réponse arrivée est vide.
[[nodiscard]] bool Present(ItemSheetTab tab, const DescriptionView& view) {
  switch (tab) {
    case ItemSheetTab::kDescription:
      return true;
    case ItemSheetTab::kGeneral:
      return !view.facts.empty();
    case ItemSheetTab::kOdds:
      return !view.odds.empty();
    case ItemSheetTab::kMonsters:
    case ItemSheetTab::kBoss:
    case ItemSheetTab::kScript:
    case ItemSheetTab::kCombos:
      return view.server_tabs;
    case ItemSheetTab::kSellers:
      return view.sellers_phase != ItemSheetSellersPhase::kAbsent;
  }
  return false;
}

[[nodiscard]] bool Greyed(ItemSheetTab tab, const DescriptionView& view) {
  switch (tab) {
    case ItemSheetTab::kMonsters:
      return SourcesKnownEmpty(view.drops_ordinary);
    case ItemSheetTab::kBoss:
      return SourcesKnownEmpty(view.drops_boss);
    case ItemSheetTab::kScript:
      return ScriptsKnownEmpty(view.scripts);
    case ItemSheetTab::kCombos:
      return CombosKnownEmpty(view.scripts);
    case ItemSheetTab::kDescription:
    case ItemSheetTab::kGeneral:
    case ItemSheetTab::kOdds:
    case ItemSheetTab::kSellers:
      return false;
  }
  return false;
}

// ⚠ LE COMPTE FAIT PARTIE DU LIBELLÉ, l'identité non : `###` la fige, sans quoi
// l'onglet choisi sauterait d'un objet à l'autre.
[[nodiscard]] std::string TabLabel(ItemSheetTab tab, const DescriptionView& view) {
  switch (tab) {
    case ItemSheetTab::kDescription:
      return std::string(core::Tr("Description")) + "###sheet_description";
    case ItemSheetTab::kGeneral:
      return std::string(core::Tr("Général")) + "###sheet_general";
    case ItemSheetTab::kOdds:
      return std::string(core::Tr("Probabilités")) + "###sheet_odds";
    case ItemSheetTab::kMonsters:
      return std::string(core::Tr("Monstres")) + "###sheet_monsters";
    case ItemSheetTab::kBoss:
      return std::string(core::Tr("Boss / MVP")) + "###sheet_boss";
    case ItemSheetTab::kScript:
      return std::string(core::Tr("Script")) + "###sheet_script";
    case ItemSheetTab::kCombos:
      return std::string(core::Tr("Combos")) + "###sheet_combos";
    case ItemSheetTab::kSellers: {
      // Le nombre n'est su qu'à la réponse du Marché.
      if (view.sellers_phase != ItemSheetSellersPhase::kReady) {
        return std::string(core::Tr("Vendu chez")) + "###sheet_sellers";
      }
      std::array<char, kLabelBytes> counted{};
      std::snprintf(counted.data(), counted.size(), core::Tr("Vendu chez (%d)"),
                    static_cast<int>(view.sellers.size()));
      return std::string(counted.data()) + "###sheet_sellers";
    }
  }
  return {};
}

// « GÉNÉRAL » : ce que la base sait de l'objet, ligne à ligne.
void DrawFacts(const DescriptionView& view) {
  if (!ImGui::BeginTable("##sheet_facts", kFactColumns,
                         ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg |
                             ImGuiTableFlags_BordersInner)) {
    return;
  }
  for (const ItemSheetFact& fact : view.facts) {
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextDisabled("%.*s", static_cast<int>(fact.label.size()), fact.label.data());
    ImGui::TableNextColumn();
    ImGui::PushTextWrapPos(0.0F);
    Unformatted(fact.value);
    ImGui::PopTextWrapPos();
  }
  ImGui::EndTable();
}

// « VENDU CHEZ » : les boutiques de PNJ, que le Marché sait.
void DrawSellers(const DescriptionView& view, const SkinArt& art, DescriptionWindowAction& action) {
  switch (view.sellers_phase) {
    case ItemSheetSellersPhase::kAbsent:
      return;
    case ItemSheetSellersPhase::kAsking:
      ImGui::TextDisabled("%s", core::Tr("Demande au serveur…"));
      return;
    case ItemSheetSellersPhase::kRefused:
      Unformatted(view.sellers_status);
      if (SkinSmallButton(core::Tr("Réessayer"), art)) {
        action.ask_sellers = true;
      }
      return;
    case ItemSheetSellersPhase::kReady:
      break;
  }
  if (view.sellers.empty()) {
    ImGui::TextDisabled("%s", core::Tr("Aucun PNJ ne le vend."));
    return;
  }
  if (!ImGui::BeginTable("##sheet_sellers", kSellerColumns,
                         ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_Resizable |
                             ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) {
    return;
  }
  // Le PNJ prend la place qui reste ; les autres tiennent leur texte.
  ImGui::TableSetupColumn(core::Tr("PNJ"), ImGuiTableColumnFlags_WidthStretch);
  ImGui::TableSetupColumn(core::Tr("Carte"));
  ImGui::TableSetupColumn(core::Tr("Prix"));
  ImGui::TableSetupColumn("##sheet_go", ImGuiTableColumnFlags_NoResize);
  ImGui::TableHeadersRow();
  for (std::size_t rank = 0; rank < view.sellers.size(); ++rank) {
    const ItemSheetSellerRow& row = view.sellers[rank];
    ImGui::PushID(static_cast<int>(rank));
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::AlignTextToFramePadding();
    CellText(row.npc);
    ImGui::TableNextColumn();
    ImGui::AlignTextToFramePadding();
    CellText(row.place);
    ImGui::TableNextColumn();
    ImGui::AlignTextToFramePadding();
    CellText(row.price);
    ImGui::TableNextColumn();
    if (SkinSmallButton(core::Tr("Y aller"), art)) {
      action.go_to_seller = static_cast<int>(rank);
    }
    if (ImGui::IsItemHovered()) {
      SkinTooltip(core::Tr("Guide jusqu'au PNJ"));
    }
    ImGui::PopID();
  }
  ImGui::EndTable();
}

void DrawTabBody(ItemSheetTab tab, DescriptionWindowState& state, const DescriptionView& view,
                 const SkinArt& art, DescriptionWindowAction& action) {
  switch (tab) {
    case ItemSheetTab::kDescription:
      DrawDescriptionTab(state, view, art, action);
      return;
    case ItemSheetTab::kGeneral:
      DrawFacts(view);
      return;
    case ItemSheetTab::kOdds:
      DrawOddsTab(state, view, action);
      return;
    case ItemSheetTab::kMonsters:
      DrawSourcesTab(state, view, false, art, action);
      return;
    case ItemSheetTab::kBoss:
      DrawSourcesTab(state, view, true, art, action);
      return;
    case ItemSheetTab::kScript:
      DrawScriptsTab(view.scripts, art);
      return;
    case ItemSheetTab::kCombos:
      DrawCombosTab(state, view, art, action);
      return;
    case ItemSheetTab::kSellers:
      action.sellers_shown = true;
      DrawSellers(view, art, action);
      return;
  }
}

// L'EN-TÊTE DANS LA BASE : l'icône et le nom, qui portent les gestes de tout
// objet, l'identifiant, et « Marché ».
void DrawDatabaseHeader(DescriptionWindowState& state, const DescriptionView& view,
                        const SkinArt& art, DescriptionWindowAction& action) {
  ImGui::BeginGroup();
  DrawItemLinkCell(ItemLinkCell{.item_id = view.item_id,
                                .text = view.name,
                                .icon = view.icon,
                                .icon_side = kIconPixels * Scale(),
                                .keep_icon_place = true,
                                .color = kTitleColor},
                   state.link_menu, action.link);
  ImGui::SameLine();
  ImGui::TextDisabled("#%u", static_cast<unsigned>(view.item_id));
  ImGui::SameLine();
  if (SkinSmallButton(core::Tr("Marché"), art)) {
    action.search_market = true;
  }
  SkinTooltip(core::Tr("Ouvre le Marché sur cet item : qui le vend, qui l'achète."));
  // « SIGNALER UN BUG », celui de la barre de titre de la description : le
  // panneau de la base n'a pas de barre, l'en-tête le porte. Même réglage, même
  // demande.
  if (state.bug_report) {
    ImGui::SameLine();
    if (SkinSmallButton(core::Tr("Signaler un bug"), art)) {
      action.report_bug = true;
    }
  }
  ImGui::EndGroup();
  ImGui::Separator();
}

void DrawTabs(DescriptionWindowState& state, const DescriptionView& view, const SkinArt& art,
              DescriptionWindowAction& action) {
  // L'ONGLET MONTRÉ : le choix du joueur, ou « Description » quand l'objet ne
  // l'offre pas — sans toucher au choix, que l'objet suivant retrouvera.
  const ItemSheetTab shown =
      ItemSheetTabOffered(state.sheet_tab, view) ? state.sheet_tab : ItemSheetTab::kDescription;
  if (!BeginSkinTabBar("##item_sheet_tabs", art)) {
    return;
  }
  // ⚠ IMPOSÉ AVANT LE PREMIER ONGLET : la barre ne se dispose qu'à lui, et y
  // prend la sélection en attente. `SetSelected` n'obéirait qu'à l'image
  // d'après, et l'image entre les deux n'aurait montré aucun onglet. Un clic
  // en attente passe devant : c'est un nouveau choix.
  ImGuiTabBar* const bar = GImGui->CurrentTabBar;
  const std::string shown_label = TabLabel(shown, view);
  const bool clicked = bar->NextSelectedTabId != 0;
  if (!clicked && bar->SelectedTabId != ImGui::GetCurrentWindow()->GetID(shown_label.c_str())) {
    ImGui::TabBarQueueFocus(bar, shown_label.c_str());
  }
  for (const ItemSheetTab tab : kTabs) {
    if (!Present(tab, view)) {
      continue;
    }
    const std::string label = TabLabel(tab, view);
    // 🔴 LE GRISÉ NE COUVRE QUE L'ONGLET, pas son corps : l'onglet resté ouvert
    // quand la réponse vide est arrivée se dessinerait gris lui aussi.
    ImGui::BeginDisabled(Greyed(tab, view));
    const bool open = ImGui::BeginTabItem(label.c_str());
    ImGui::EndDisabled();
    if (!open) {
      continue;
    }
    // ⚠ UN ONGLET NE SE MONTRE PAS À L'IMAGE OÙ IL PARAÎT : ImGui en montre
    // alors un autre, que seul un clic fait choisir.
    if (clicked && tab != shown) {
      state.sheet_tab = tab;
    }
    // LE CONTENU DÉFILE SEUL : l'en-tête et les onglets restent en place.
    if (ImGui::BeginChild("##item_sheet_body")) {
      DrawTabBody(tab, state, view, art, action);
      // L'ascenseur d'ImGui est transparent : c'est celui du jeu qui se voit.
      PaintWindowScrollbar(art);
    }
    ImGui::EndChild();
    ImGui::EndTabItem();
  }
  EndSkinTabBar();
}

}  // namespace

bool ItemSheetTabOffered(ItemSheetTab tab, const DescriptionView& view) {
  return Present(tab, view) && !Greyed(tab, view);
}

void DrawItemSheetBody(DescriptionWindowState& state, const DescriptionView& view,
                       const SkinArt& art, ItemSheetHost host, DescriptionWindowAction& action) {
  if (host == ItemSheetHost::kDatabase) {
    DrawDatabaseHeader(state, view, art, action);
  } else {
    DrawDescriptionBack(state, view, art);
  }
  // 🔴 PAS D'ONGLETS SANS RIEN À Y METTRE : un onglet « Description » seul
  // n'offrirait aucun choix et prendrait une ligne.
  bool alone = true;
  for (const ItemSheetTab tab : kTabs) {
    alone = alone && (tab == ItemSheetTab::kDescription || !Present(tab, view));
  }
  if (alone) {
    DrawDescriptionTab(state, view, art, action);
    return;
  }
  DrawTabs(state, view, art, action);
}

}  // namespace ro::ui
