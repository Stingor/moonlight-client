// LA DESCRIPTION D'UN OBJET : l'onglet « Probabilités » et le fil « < Retour ».
//
// L'onglet dit ce que rend un CONTENEUR — boîte, album d'objets, branche — et à
// quelle fréquence. Il vit dans la description même, et non dans une fenêtre
// à part. Chaque ligne est un lien : un objet se décrit À LA PLACE de celui
// qu'on lit, un monstre ouvre sa fiche, et l'on revient d'un pas par
// « < Retour ».

#include "description_odds.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <string>
#include <string_view>

#include <imgui.h>

#include "ro/core/ascii.h"
#include "ro/ui/skin_tooltip.h"
#include "ro/ui/skin_widgets.h"

#include "ui_scale.h"

namespace ro::ui {
namespace {

// La mise en page de l'onglet.
constexpr float kFilterWidthPixels = 180.0F;
constexpr float kChanceColumnPixels = 80.0F;
constexpr float kDrawColumnPixels = 60.0F;
constexpr ImU32 kHeaderColor = IM_COL32(206, 198, 172, 255);
// L'icône d'un objet, à la hauteur d'une ligne de texte : une table de mille
// lignes se lit, elle ne se regarde pas.
constexpr float kIconLines = 1.0F;

constexpr int kContentColumn = 0;
constexpr int kChanceColumn = 1;
constexpr int kDrawColumn = 2;

constexpr const char* kLinkMenuId = "##menu_lien_description";

[[nodiscard]] int Compare(double left, double right) {
  return left < right ? -1 : (left > right ? 1 : 0);
}

}  // namespace

void DrawDescriptionLink(DescriptionWindowState& state, const DescriptionLinkCell& cell,
                         DescriptionWindowAction& action) {
  DrawItemLinkCell(ItemLinkCell{.item_id = cell.id,
                                .kind = cell.monster ? ChatLinkKind::kMonster : ChatLinkKind::kItem,
                                .text = cell.shown,
                                .menu_label = cell.name,
                                .icon = cell.with_icon ? cell.icon : SkinImage{},
                                .icon_side = ImGui::GetTextLineHeight() * kIconLines,
                                .keep_icon_place = cell.with_icon,
                                .color = cell.color},
                   state.link_menu, action.link);
}

void FinishItemSheetLinks(DescriptionWindowState& state, const DescriptionView& view,
                          ItemSheetHost host, DescriptionWindowAction& action) {
  // Le menu, hors des tables : une cellule change la pile d'identifiants, et le
  // surgissant ne s'y retrouverait pas.
  DrawItemLinkMenu(kLinkMenuId, state.link_menu, action.link, view.link_facts);
  // 🔴 DÉCRIRE UN OBJET NE SORT PAS DE LA FICHE : c'est une surface de
  // description, et le clic y REMPLACE l'objet affiché — la fenêtre par son fil
  // « < Retour », la base par le sien. Un monstre, lui, a sa propre fiche.
  if (host == ItemSheetHost::kDescriptionWindow &&
      action.link.gesture == ChatLinkGesture::kDescribe &&
      action.link.kind == ChatLinkKind::kItem) {
    NavigateDescription(state, action.link.item_id);
    action.link.gesture = ChatLinkGesture::kNothing;
  }
}

void NoteDescriptionNavigation(DescriptionWindowState& state) {
  if (!state.open) {
    state.back.clear();
    state.navigated_to = 0;
  } else if (state.item_id == state.navigated_to) {
    state.navigated_to = 0;  // notre propre saut : la pile reste
  } else if (state.item_id != state.last_item) {
    state.back.clear();
  }
  state.last_item = state.open ? state.item_id : 0;
}

void NavigateDescription(DescriptionWindowState& state, std::uint32_t item_id) {
  if (item_id == 0 || item_id == state.item_id) {
    return;
  }
  state.back.push_back(state.item_id);
  state.item_id = item_id;
  state.navigated_to = item_id;
  // Un objet de table n'est pas un objet qu'on tient : le nom se compose de
  // son seul identifiant.
  state.from_slot = 0;
  state.from_holder = ItemHolder::kInventory;
}

void DrawDescriptionBack(DescriptionWindowState& state, const DescriptionView& view,
                         const SkinArt& art) {
  if (state.back.empty()) {
    return;
  }
  // Au-dessus des onglets, pas dedans : on revient en arrière depuis n'importe
  // lequel d'entre eux, y compris celui des probabilités d'où l'on vient.
  std::array<char, 160> label = {};
  const std::string name = view.back_name.empty() ? std::string("?") : std::string(view.back_name);
  std::snprintf(label.data(), label.size(), core::Tr("< Retour : %s"), name.c_str());
  const std::string button = std::string(label.data()) + "###retour_description";
  if (SkinSmallButton(button.c_str(), art)) {
    // Revenir d'un cran ne réempile rien : sinon « précédent » ferait osciller
    // entre deux objets.
    state.item_id = state.back.back();
    state.back.pop_back();
    state.navigated_to = state.item_id;
    state.from_slot = 0;
    state.from_holder = ItemHolder::kInventory;
  }
  if (ImGui::IsItemHovered()) {
    SkinTooltipFormat(core::Tr("Revenir à l'item d'où vient ce lien (%zu pas)."),
                      state.back.size());
  }
}

void DrawOddsTab(DescriptionWindowState& state, const DescriptionView& view,
                 DescriptionWindowAction& action) {
  ImGuiTextFilter& filter = state.odds_filters[view.item_id];
  ImGui::SetNextItemWidth(Px(kFilterWidthPixels));
  filter.Draw(core::Tr("Filtrer (contenu)##probabilites"));

  state.odds_order.clear();
  for (std::size_t rank = 0; rank < view.odds.size(); ++rank) {
    // ⚠ PAR SES BORNES, sans copie : une boîte peut compter mille lignes, et
    // la liste se refiltre à chaque image.
    const std::string_view name = view.odds[rank].name;
    if (filter.PassFilter(name.data(), name.data() + name.size())) {
      state.odds_order.push_back(static_cast<int>(rank));
    }
  }
  ImGui::SameLine();
  ImGui::TextDisabled(core::Tr("%zu / %zu entrée(s)"), state.odds_order.size(), view.odds.size());

  const int columns = view.odds_many_groups ? 3 : 2;
  // 🔴 PAS DE DÉFILEMENT PROPRE À LA TABLE : c'est la FENÊTRE qui défile. Une
  // région de défilement interne demande une hauteur, et toute hauteur écrite
  // ici finit par dépasser une fenêtre qu'on retaille. Le découpage des lignes
  // visibles suffit à ce que mille lignes ne coûtent rien.
  constexpr ImGuiTableFlags kFlags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                     ImGuiTableFlags_Sortable | ImGuiTableFlags_SizingStretchProp;
  ImGui::PushStyleColor(ImGuiCol_TableHeaderBg, kHeaderColor);
  if (ImGui::BeginTable("##probabilites", columns, kFlags)) {
    ImGui::TableSetupColumn(core::Tr("Contenu"), ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn(core::Tr("Chance"),
                            ImGuiTableColumnFlags_WidthFixed |
                                ImGuiTableColumnFlags_PreferSortDescending |
                                ImGuiTableColumnFlags_DefaultSort,
                            Px(kChanceColumnPixels));
    if (view.odds_many_groups) {
      ImGui::TableSetupColumn(core::Tr("Tirage"), ImGuiTableColumnFlags_WidthFixed,
                              Px(kDrawColumnPixels));
    }
    ImGui::TableHeadersRow();

    if (const ImGuiTableSortSpecs* sort = ImGui::TableGetSortSpecs();
        sort != nullptr && sort->SpecsCount > 0) {
      const ImGuiTableColumnSortSpecs& spec = sort->Specs[0];
      const bool ascending = spec.SortDirection == ImGuiSortDirection_Ascending;
      std::ranges::stable_sort(state.odds_order, [&](int left, int right) {
        const DescriptionOddsRow& a = view.odds[static_cast<std::size_t>(left)];
        const DescriptionOddsRow& b = view.odds[static_cast<std::size_t>(right)];
        int compared = 0;
        if (spec.ColumnIndex == kChanceColumn) {
          compared = Compare(a.percent, b.percent);
        } else if (spec.ColumnIndex == kDrawColumn) {
          compared = Compare(a.group, b.group);
        } else {
          compared = core::AsciiCompareFolded(a.name, b.name);
        }
        return ascending ? compared < 0 : compared > 0;
      });
    }

    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(state.odds_order.size()));
    while (clipper.Step()) {
      for (int shown_rank = clipper.DisplayStart; shown_rank < clipper.DisplayEnd; ++shown_rank) {
        const DescriptionOddsRow& row = view.odds[static_cast<std::size_t>(
            state.odds_order[static_cast<std::size_t>(shown_rank)])];
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        // 🔴 UN NOM D'ABORD, PUIS LE RANG : l'en-tête pose déjà une identité par
        // numéro de colonne au même étage, et un rang seul pourrait la rejoindre.
        ImGui::PushID("ligne");
        ImGui::PushID(shown_rank);
        std::string shown(row.name);
        if (row.id != 0) {
          shown += " (" + std::to_string(row.id) + ")";
        }
        if (row.id == 0) {
          // Un libellé sans identifiant : rien vers quoi pointer.
          ImGui::TextUnformatted(shown.c_str());
        } else {
          DrawDescriptionLink(state,
                              DescriptionLinkCell{.id = row.id,
                                                  .monster = row.monster,
                                                  .name = row.name,
                                                  .shown = shown,
                                                  .with_icon = !row.monster,
                                                  .icon = row.icon},
                              action);
        }
        ImGui::PopID();
        ImGui::PopID();
        ImGui::TableNextColumn();
        ImGui::Text("%.4f%%", row.percent);
        if (ImGui::IsItemHovered() && row.total > 0) {
          SkinTooltipFormat(core::Tr("%d chances sur %d"), row.weight, row.total);
        }
        if (view.odds_many_groups) {
          ImGui::TableNextColumn();
          if (row.guaranteed) {
            ImGui::TextDisabled("%s", core::Tr("garanti"));
          } else {
            ImGui::Text("%d", row.group);
          }
        }
      }
    }
    ImGui::EndTable();
  }
  ImGui::PopStyleColor();
}

}  // namespace ro::ui
