// La fiche d'un objet dans la base de données : sous son en-tête, un onglet par
// sujet — ce que le serveur en sait, la description que le jeu montre, ses
// scripts, qui le lâche et qui le vend.

#include <array>
#include <cstddef>
#include <cstdio>
#include <string>

#include <imgui.h>
#include <imgui_internal.h>

#include "ro/core/i18n.h"
#include "ro/ui/skin_tooltip.h"
#include "ro/ui/skin_widgets.h"

#include "database_parts.h"

namespace ro::ui::database {
namespace {

// Le nom de l'objet, dans le brun des titres de description.
constexpr ImU32 kTitleColor = IM_COL32(115, 61, 5, 255);
constexpr ImU32 kAmberColor = IM_COL32(153, 102, 13, 255);
constexpr ImU32 kRedColor = IM_COL32(153, 31, 31, 255);
constexpr float kIconPixels = 32.0F;
constexpr int kFactColumns = 2;
constexpr int kDropperColumns = 2;
constexpr int kSellerColumns = 4;
constexpr float kRateColumnPixels = 80.0F;
constexpr std::size_t kLineBytes = 96;
// Les rangs de boss de la base.
constexpr std::uint8_t kBossRank = 1;
constexpr std::uint8_t kMvpRank = 2;

[[nodiscard]] float Scale() { return ImGui::GetStyle().FontScaleMain; }

void TextOf(std::string_view text) {
  ImGui::TextUnformatted(text.data(), text.data() + text.size());
}

void DrawHeader(const DatabaseItemView& view, const SkinArt& art, ItemLinkMenu& menu,
                DatabaseAnswer& answer) {
  const float side = kIconPixels * Scale();
  ImGui::BeginGroup();
  // L'icône et le nom portent les gestes de tout objet : le clic décrit, le
  // clic droit ouvre le menu des liens.
  DrawItemLinkCell(ItemLinkCell{.item_id = view.item_id,
                                .text = view.name,
                                .icon = view.icon,
                                .icon_side = side,
                                .keep_icon_place = true,
                                .color = kTitleColor},
                   menu, answer.item);
  ImGui::SameLine();
  ImGui::TextDisabled("#%u", static_cast<unsigned>(view.item_id));
  ImGui::SameLine();
  if (SkinSmallButton(core::Tr("Marché##db_market"), art)) {
    answer.market_item = view.item_id;
  }
  SkinTooltip(core::Tr("Ouvre le Marché sur cet item : qui le vend, qui l'achète."));
  ImGui::EndGroup();
}

void DrawFacts(const DatabaseItemView& view) {
  if (view.facts.empty() ||
      !ImGui::BeginTable("##db_facts", kFactColumns,
                         ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg |
                             ImGuiTableFlags_BordersInner)) {
    return;
  }
  for (const DatabaseFact& fact : view.facts) {
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextDisabled("%.*s", static_cast<int>(fact.label.size()), fact.label.data());
    ImGui::TableNextColumn();
    ImGui::PushTextWrapPos(0.0F);
    TextOf(fact.value);
    ImGui::PopTextWrapPos();
  }
  ImGui::EndTable();
}

// UN SCRIPT BRUT, tel que le serveur l'exécute, sous son intitulé.
void DrawScript(const char* heading, std::string_view script) {
  if (script.empty()) {
    return;
  }
  ImGui::TextDisabled("%s", heading);
  ImGui::Indent();
  ImGui::PushTextWrapPos(0.0F);
  TextOf(script);
  ImGui::PopTextWrapPos();
  ImGui::Unindent();
}

void DrawScripts(const DatabaseItemView& view) {
  DrawScript(core::Tr("À l'usage ou porté"), view.script);
  DrawScript(core::Tr("À l'équipement"), view.equip_script);
  DrawScript(core::Tr("Au retrait"), view.unequip_script);
}

// « LÂCHÉ PAR » : chaque monstre est un lien vers sa fiche.
void DrawDroppers(DatabaseWindowState& state, const DatabaseItemView& view) {
  if (view.droppers.empty()) {
    ImGui::TextDisabled("%s", core::Tr("Aucun monstre ne le lâche."));
    return;
  }
  if (!ImGui::BeginTable("##db_droppers_table", kDropperColumns,
                         ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg)) {
    return;
  }
  ImGui::TableSetupColumn("##db_dropper", ImGuiTableColumnFlags_WidthStretch);
  ImGui::TableSetupColumn("##db_rate", ImGuiTableColumnFlags_WidthFixed,
                          kRateColumnPixels * Scale());
  for (std::size_t rank = 0; rank < view.droppers.size(); ++rank) {
    const DatabaseDropperRow& row = view.droppers[rank];
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    std::array<char, kLineBytes> label{};
    std::snprintf(label.data(), label.size(), "%.*s##db_dropper%zu",
                  static_cast<int>(row.name.size()), row.name.data(), rank);
    if (ImGui::Selectable(label.data())) {
      state.history.Visit(DatabaseTarget{.kind = DatabaseKind::kMonster, .id = row.monster_id});
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }
    if (row.rank == kMvpRank || row.rank == kBossRank) {
      ImGui::SameLine();
      ImGui::PushStyleColor(ImGuiCol_Text, row.rank == kMvpRank ? kRedColor : kAmberColor);
      ImGui::TextUnformatted(row.rank == kMvpRank ? core::Tr("[MVP]") : core::Tr("[Mini-boss]"));
      ImGui::PopStyleColor();
    }
    if (row.mvp_reward) {
      ImGui::SameLine();
      ImGui::TextDisabled("%s", core::Tr("(récompense MVP)"));
    }
    ImGui::TableNextColumn();
    TextOf(row.rate);
  }
  ImGui::EndTable();
}

// « VENDU CHEZ » : les boutiques de PNJ, que le Marché sait.
void DrawSellers(const DatabaseItemView& view, const SkinArt& art, DatabaseAnswer& answer) {
  switch (view.sellers_phase) {
    case DatabaseSellersPhase::kOffline:
      ImGui::TextDisabled("%s", core::Tr("Le serveur seul le sait : connectez-vous."));
      return;
    case DatabaseSellersPhase::kAsking:
      ImGui::TextDisabled("%s", core::Tr("Demande au serveur…"));
      return;
    case DatabaseSellersPhase::kRefused:
      TextOf(view.sellers_status);
      if (SkinSmallButton(core::Tr("Réessayer##db_sellers_retry"), art)) {
        answer.ask_sellers = true;
      }
      return;
    case DatabaseSellersPhase::kReady:
      break;
  }
  if (view.sellers.empty()) {
    ImGui::TextDisabled("%s", core::Tr("Aucun PNJ ne le vend."));
    return;
  }
  if (!ImGui::BeginTable("##db_sellers_table", kSellerColumns,
                         ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg)) {
    return;
  }
  for (std::size_t rank = 0; rank < view.sellers.size(); ++rank) {
    const DatabaseSellerRow& row = view.sellers[rank];
    ImGui::PushID(static_cast<int>(rank));
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::AlignTextToFramePadding();
    TextOf(row.npc);
    ImGui::TableNextColumn();
    ImGui::AlignTextToFramePadding();
    TextOf(row.place);
    ImGui::TableNextColumn();
    ImGui::AlignTextToFramePadding();
    TextOf(row.price);
    ImGui::TableNextColumn();
    if (SkinSmallButton(core::Tr("Y aller"), art)) {
      answer.go_to_seller = static_cast<int>(rank);
    }
    if (ImGui::IsItemHovered()) {
      SkinTooltip(core::Tr("Guide jusqu'au PNJ"));
    }
    ImGui::PopID();
  }
  ImGui::EndTable();
}

constexpr std::array<DatabaseItemTab, 5> kItemTabs = {
    DatabaseItemTab::kGeneral,  DatabaseItemTab::kDescription, DatabaseItemTab::kScript,
    DatabaseItemTab::kDroppers, DatabaseItemTab::kSellers,
};

// ⚠ LE COMPTE FAIT PARTIE DU LIBELLÉ, l'identité non : `###` la fige, sans quoi
// l'onglet choisi sauterait d'un objet à l'autre.
[[nodiscard]] std::string TabLabel(DatabaseItemTab tab, const DatabaseItemView& view) {
  std::array<char, kLineBytes> counted{};
  switch (tab) {
    case DatabaseItemTab::kGeneral:
      return core::Tr("Général###db_general");
    case DatabaseItemTab::kDescription:
      return core::Tr("Description###db_description");
    case DatabaseItemTab::kScript:
      return core::Tr("Script###db_script");
    case DatabaseItemTab::kDroppers:
      std::snprintf(counted.data(), counted.size(), core::Tr("Lâché par (%d)###db_droppers"),
                    static_cast<int>(view.droppers.size()));
      return counted.data();
    case DatabaseItemTab::kSellers:
      // Le nombre n'est su qu'à la réponse du Marché.
      if (view.sellers_phase != DatabaseSellersPhase::kReady) {
        return core::Tr("Vendu chez###db_sellers");
      }
      std::snprintf(counted.data(), counted.size(), core::Tr("Vendu chez (%d)###db_sellers"),
                    static_cast<int>(view.sellers.size()));
      return counted.data();
  }
  return {};
}

void DrawTabBody(DatabaseItemTab tab, DatabaseWindowState& state, const DatabaseItemView& view,
                 const SkinArt& art, DatabaseAnswer& answer) {
  switch (tab) {
    case DatabaseItemTab::kGeneral:
      DrawFacts(view);
      return;
    case DatabaseItemTab::kDescription:
      DrawDescriptionPreviewPanel("##db_description_panel", *view.description, art);
      return;
    case DatabaseItemTab::kScript:
      DrawScripts(view);
      return;
    case DatabaseItemTab::kDroppers:
      DrawDroppers(state, view);
      return;
    case DatabaseItemTab::kSellers:
      answer.sellers_shown = true;
      DrawSellers(view, art, answer);
      return;
  }
}

void DrawTabs(DatabaseWindowState& state, const DatabaseItemView& view, const SkinArt& art,
              DatabaseAnswer& answer) {
  // L'ONGLET MONTRÉ : le choix du joueur, ou « Général » quand l'objet ne l'a
  // pas — sans toucher au choix, que l'objet suivant retrouvera.
  const DatabaseItemTab shown =
      DatabaseItemTabShown(state.item_tab, view) ? state.item_tab : DatabaseItemTab::kGeneral;
  if (!BeginSkinTabBar("##db_item_tabs", art)) {
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
  for (const DatabaseItemTab tab : kItemTabs) {
    if (!DatabaseItemTabShown(tab, view)) {
      continue;
    }
    const std::string label = TabLabel(tab, view);
    if (!ImGui::BeginTabItem(label.c_str())) {
      continue;
    }
    // ⚠ UN ONGLET NE SE MONTRE PAS À L'IMAGE OÙ IL PARAÎT : ImGui en montre
    // alors un autre, que seul un clic fait choisir.
    if (clicked && tab != shown) {
      state.item_tab = tab;
    }
    // LE CONTENU DÉFILE SEUL : l'en-tête et les onglets restent en place.
    if (ImGui::BeginChild("##db_item_body")) {
      DrawTabBody(tab, state, view, art, answer);
    }
    ImGui::EndChild();
    ImGui::EndTabItem();
  }
  EndSkinTabBar();
}

}  // namespace

void DrawItemSheet(DatabaseWindowState& state, const DatabaseItemView& view, const SkinArt& art,
                   ItemLinkMenu& menu, DatabaseAnswer& answer) {
  DrawHeader(view, art, menu, answer);
  ImGui::Separator();
  DrawTabs(state, view, art, answer);
}

}  // namespace ro::ui::database

namespace ro::ui {

bool DatabaseItemTabShown(DatabaseItemTab tab, const DatabaseItemView& view) {
  switch (tab) {
    case DatabaseItemTab::kDescription:
      return view.description != nullptr;
    case DatabaseItemTab::kScript:
      return !view.script.empty() || !view.equip_script.empty() || !view.unequip_script.empty();
    case DatabaseItemTab::kGeneral:
    case DatabaseItemTab::kDroppers:
    case DatabaseItemTab::kSellers:
      return true;
  }
  return false;
}

}  // namespace ro::ui
