// La fiche d'un objet dans la base de données : ce que le serveur en sait, la
// description que le jeu montre, ses scripts, qui le lâche et qui le vend.

#include <array>
#include <cstddef>
#include <cstdio>
#include <string>

#include <imgui.h>

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
  if (view.script.empty() && view.equip_script.empty() && view.unequip_script.empty()) {
    return;
  }
  // REPLIÉ PAR DÉFAUT : un script se lit quand on le cherche.
  if (!ImGui::CollapsingHeader(core::Tr("Script du serveur"))) {
    return;
  }
  DrawScript(core::Tr("À l'usage ou porté"), view.script);
  DrawScript(core::Tr("À l'équipement"), view.equip_script);
  DrawScript(core::Tr("Au retrait"), view.unequip_script);
}

// « LÂCHÉ PAR » : chaque monstre est un lien vers sa fiche.
void DrawDroppers(DatabaseWindowState& state, const DatabaseItemView& view) {
  std::array<char, kLineBytes> heading{};
  std::snprintf(heading.data(), heading.size(), core::Tr("Lâché par (%d)###db_droppers"),
                static_cast<int>(view.droppers.size()));
  if (!ImGui::CollapsingHeader(heading.data(), ImGuiTreeNodeFlags_DefaultOpen)) {
    return;
  }
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
  if (!ImGui::CollapsingHeader(core::Tr("Vendu chez###db_sellers"),
                               ImGuiTreeNodeFlags_DefaultOpen)) {
    return;
  }
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

}  // namespace

void DrawItemSheet(DatabaseWindowState& state, const DatabaseItemView& view, const SkinArt& art,
                   ItemLinkMenu& menu, DatabaseAnswer& answer) {
  DrawHeader(view, art, menu, answer);
  ImGui::Separator();
  DrawFacts(view);
  if (view.description != nullptr &&
      ImGui::CollapsingHeader(core::Tr("Description en jeu###db_description"),
                              ImGuiTreeNodeFlags_DefaultOpen)) {
    DrawDescriptionPreviewPanel("##db_description_panel", *view.description, art);
  }
  DrawScripts(view);
  DrawDroppers(state, view);
  DrawSellers(view, art, answer);
}

}  // namespace ro::ui::database
