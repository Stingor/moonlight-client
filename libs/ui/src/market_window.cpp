#include "ro/ui/market_window.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <limits>
#include <string>
#include <utility>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/ui/skin_tooltip.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"
#include "ro/ui/trade_widgets.h"

#include "table_cells.h"
#include "ui_scale.h"
#include "ui_text.h"

namespace ro::ui {
namespace {

// ── LES MESURES ─────────────────────────────────────────────────────────────
constexpr float kItemColumnPixels = 180.0F;
constexpr float kPriceColumnPixels = 90.0F;
constexpr float kAmountColumnPixels = 40.0F;
// La colonne de la carte, à sa première ouverture, tient ce texte.
constexpr const char* kMapColumnSample = "moc_ruins 999,999";
constexpr int kResultColumnCount = kMarketActionsColumn + 1;
// L'épaisseur d'une bordure de colonne.
constexpr float kColumnBorderPixels = 1.0F;
constexpr float kQueryFieldPixels = 220.0F;
constexpr float kPriceFieldPixels = 80.0F;
constexpr float kCategoryFieldPixels = 130.0F;
constexpr float kRefineFieldPixels = 90.0F;
// La place de la table : la fenêtre, moins le pied.
constexpr float kFooterLines = 2.5F;
// Le marchand hors ligne : son échoppe s'écrit grisée.
constexpr ImVec4 kAutotradeText = ImVec4(0.45F, 0.45F, 0.45F, 1.0F);
// La ligne de l'échoppe comparée, peinte d'un jaune pâle : la référence.
constexpr ImU32 kComparedRowColor = IM_COL32(255, 244, 196, 255);

constexpr const char* kItemMenuId = "##market_item_menu";
constexpr const char* kStoreItemMenuId = "##market_store_item_menu";

// LES FAMILLES D'ITEM, dans l'ordre du fil.
constexpr std::array<const char*, 8> kCategoryLabels = {
    core::TrText("Tous les types"), core::TrText("Consommables"), core::TrText("Armes"),
    core::TrText("Armures"),        core::TrText("Cartes"),       core::TrText("Divers"),
    core::TrText("Munitions"),      core::TrText("Familiers"),
};

void TextRightAligned(std::string_view text) {
  const bool overflows = OverflowsCell(text);
  ImGui::SetCursorPosX(ImGui::GetCursorPosX() +
                       std::max(0.0F, ImGui::GetContentRegionAvail().x - TextWidth(text)));
  Unformatted(text);
  if (overflows && ImGui::IsItemHovered()) {
    TooltipWhole(text);
  }
}

// LES LARGEURS INITIALES DES COLONNES FIXES de la table des offres, en pixels
// d'écran. Celle des actions tient pile ses deux boutons.
struct ResultColumns {
  float item = 0.0F;
  float price = 0.0F;
  float amount = 0.0F;
  float map = 0.0F;
  float actions = 0.0F;
  bool amount_shown = true;
};

[[nodiscard]] ResultColumns ResultColumnsOf(const SkinArt& art, bool amount_shown) {
  const float scale = Scale();
  return ResultColumns{
      .item = kItemColumnPixels * scale,
      .price = kPriceColumnPixels * scale,
      .amount = kAmountColumnPixels * scale,
      .map = ImGui::CalcTextSize(kMapColumnSample).x,
      .actions = SkinSmallButtonWidth(core::Tr("Y aller"), art) + ImGui::GetStyle().ItemSpacing.x +
                 SkinSmallButtonWidth(core::Tr("Voir"), art),
      .amount_shown = amount_shown,
  };
}

// LA FENÊTRE LA PLUS ÉTROITE qui laisse au nom sa largeur minimale, les autres
// colonnes à leur largeur initiale : leurs marges, leurs bordures, la barre de
// défilement et la marge de la fenêtre comprises.
[[nodiscard]] float LeastWindowWidth(const ResultColumns& columns) {
  const ImGuiStyle& style = ImGui::GetStyle();
  const int shown_columns = kResultColumnCount - (columns.amount_shown ? 0 : 1);
  const float contents = columns.item + columns.price +
                         (columns.amount_shown ? columns.amount : 0.0F) + columns.map +
                         columns.actions + kMarketShopColumnLeastPixels * Scale();
  const float chrome =
      static_cast<float>(shown_columns) * (style.CellPadding.x * 2.0F + kColumnBorderPixels) +
      style.ScrollbarSize + style.WindowPadding.x * 2.0F;
  return contents + chrome;
}

// L'icône et le nom, maniés d'un bloc comme un item de commerce : la bulle
// d'item au survol, Maj + clic pour le lien, clic droit pour le menu.
void DrawItemCell(const MarketRowView& row, int row_index, bool in_store, ItemLinkMenu& menu,
                  MarketAction& action) {
  ImGui::BeginGroup();
  DrawGradedTradeIcon(row.item, row.grade_badge);
  ImGui::SameLine();
  ImGui::AlignTextToFramePadding();
  Unformatted(row.item.name);
  ImGui::EndGroup();
  TradeItemGesture gesture;
  AddLastItemGesture(gesture);
  if (!gesture.hovered) {
    return;
  }
  action.hovered_row = row_index;
  action.hovered_in_store = in_store;
  if (NoteTradeItemLink(gesture, row.item, static_cast<std::uint32_t>(row_index), menu)) {
    action.describe_row = row_index;
    action.describe_in_store = in_store;
    action.item_gesture = ChatLinkGesture::kRepost;
  }
}

// Le tri que la table demande, s'il a changé : la première page revient.
void ReadSort(MarketWindowState& state) {
  ImGuiTableSortSpecs* specs = ImGui::TableGetSortSpecs();
  if (specs == nullptr || !specs->SpecsDirty) {
    return;
  }
  if (specs->SpecsCount > 0) {
    state.sort_column = specs->Specs[0].ColumnIndex;
    state.sort_descending = specs->Specs[0].SortDirection == ImGuiSortDirection_Descending;
  }
  specs->SpecsDirty = false;
  state.page = 1;
  state.expanded_row = kMarketNoRow;
}

void DrawSearchBar(MarketWindowState& state, const MarketView& view, const SkinArt& art,
                   MarketAction& action) {
  const float scale = Scale();
  if (BeginSkinTabBar("##market_tabs", art)) {
    const std::array<std::pair<int, const char*>, 3> tabs = {{
        {kMarketSellTab, core::Tr("Ventes")},
        {kMarketBuyTab, core::Tr("Achats")},
        {kMarketNpcTab, core::Tr("PNJ")},
    }};
    // À L'OUVERTURE, L'ONGLET EST CELUI DE L'ÉTAT : la barre d'ImGui choisirait
    // d'elle-même le premier la première fois qu'elle paraît.
    for (const auto& [tab, label] : tabs) {
      const ImGuiTabItemFlags flags =
          state.select_tab && tab == state.tab ? ImGuiTabItemFlags_SetSelected : 0;
      if (ImGui::BeginTabItem(label, nullptr, flags)) {
        // ⚠ LE CHOIX D'IMGUI NE COMPTE PAS tant que l'onglet voulu n'est pas
        // montré : il n'obéit qu'à l'image d'après.
        if (state.select_tab) {
          state.select_tab = tab != state.tab;
        } else if (state.tab != tab) {
          state.tab = tab;
          action.search = true;
        }
        ImGui::EndTabItem();
      }
    }
    EndSkinTabBar();
  }
  ImGui::SetNextItemWidth(kQueryFieldPixels * scale);
  if (ImGui::InputTextWithHint("##market_query", core::Tr("Nom ou ID d'item"), state.query.data(),
                               state.query.size(), ImGuiInputTextFlags_EnterReturnsTrue) &&
      !view.busy) {
    action.search = true;
  }
  ImGui::SameLine();
  ImGui::BeginDisabled(view.busy);
  if (SkinButton(core::Tr("Rechercher"), art)) {
    action.search = true;
  }
  ImGui::SameLine();
  if (SkinButton(core::Tr("Actualiser"), art)) {
    action.refresh = true;
  }
  ImGui::EndDisabled();
}

void DrawFilters(MarketWindowState& state, const SkinArt& art) {
  const float scale = Scale();
  ImGui::SetNextItemWidth(kCategoryFieldPixels * scale);
  static_cast<void>(SkinCombo("##market_category", &state.category, kCategoryLabels, art));
  ImGui::SameLine();
  ImGui::AlignTextToFramePadding();
  Unformatted(core::Tr("Zeny"));
  ImGui::SameLine();
  ImGui::SetNextItemWidth(kPriceFieldPixels * scale);
  ImGui::InputTextWithHint("##market_min_price", core::Tr("min"), state.min_price.data(),
                           state.min_price.size(), ImGuiInputTextFlags_CharsDecimal);
  ImGui::SameLine();
  ImGui::TextUnformatted("~");
  ImGui::SameLine();
  ImGui::SetNextItemWidth(kPriceFieldPixels * scale);
  ImGui::InputTextWithHint("##market_max_price", core::Tr("max"), state.max_price.data(),
                           state.max_price.size(), ImGuiInputTextFlags_CharsDecimal);
  // UNE BOUTIQUE D'ACHAT OU DE PNJ NE PORTE QUE DU NEUF : ni raffinement, ni
  // carte, ni option. Ces filtres n'y ont pas de sens.
  ImGui::BeginDisabled(state.tab != kMarketSellTab);
  ImGui::AlignTextToFramePadding();
  Unformatted(core::Tr("Raffinement"));
  ImGui::SameLine();
  ImGui::SetNextItemWidth(kRefineFieldPixels * scale);
  static_cast<void>(SkinInputInt("##market_refine_min", &state.refine_min, 0,
                                 kMarketWindowHighestRefine, 1, 0, art));
  ImGui::SameLine();
  ImGui::TextUnformatted("~");
  ImGui::SameLine();
  ImGui::SetNextItemWidth(kRefineFieldPixels * scale);
  static_cast<void>(SkinInputInt("##market_refine_max", &state.refine_max, 0,
                                 kMarketWindowHighestRefine, 1, 0, art));
  ImGui::SameLine();
  static_cast<void>(SkinCheckBox(core::Tr("Avec cartes"), &state.with_cards, art));
  ImGui::SameLine();
  static_cast<void>(SkinCheckBox(core::Tr("Avec options"), &state.with_options, art));
  ImGui::EndDisabled();
}

// LE NOM DE L'ÉCHOPPE OU DU PNJ. Au survol, la bulle dit le nom entier s'il est
// coupé, et si le marchand est en ligne — un PNJ l'est toujours.
void DrawShopCell(const MarketRowView& row, bool npc) {
  ImGui::AlignTextToFramePadding();
  const bool overflows = OverflowsCell(row.shop_name);
  if (row.autotrade) {
    ColoredText(kAutotradeText, row.shop_name);
  } else {
    Unformatted(row.shop_name);
  }
  if (!ImGui::IsItemHovered()) {
    return;
  }
  const char* presence = nullptr;
  if (!npc) {
    presence = row.autotrade ? core::Tr("Marchand hors ligne") : core::Tr("Marchand en ligne");
  }
  if (overflows && presence != nullptr) {
    SkinTooltipFormat("%.*s\n%s", static_cast<int>(row.shop_name.size()), row.shop_name.data(),
                      presence);
  } else if (overflows) {
    TooltipWhole(row.shop_name);
  } else if (presence != nullptr) {
    SkinTooltip(presence);
  }
}

// Les détails d'une ligne dépliée, sous elle.
void DrawDetails(const MarketRowView& row) {
  ImGui::TableNextRow();
  ImGui::TableNextColumn();
  ImGui::Indent();
  for (const std::string& line : row.details) {
    ImGui::TextDisabled("%s", line.c_str());
  }
  ImGui::Unindent();
}

void DrawResults(MarketWindowState& state, const MarketView& view, const SkinArt& art,
                 MarketAction& action) {
  const float height =
      std::max(ImGui::GetFrameHeight(), ImGui::GetContentRegionAvail().y -
                                            kFooterLines * ImGui::GetFrameHeightWithSpacing());
  // 🔴 LES COLONNES SE RÈGLENT À LA SOURIS, et ImGui retient leur largeur dans
  // son fichier de réglages : la table n'est pas `NoSavedSettings`. Une seule
  // colonne s'étire, celle du nom : avec deux, leurs poids se partageraient la
  // place, et un poids initial pris pour une largeur écraserait l'autre.
  if (!ImGui::BeginTable("##market_results", kResultColumnCount,
                         ImGuiTableFlags_Sortable | ImGuiTableFlags_Resizable |
                             ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_BordersInnerV |
                             ImGuiTableFlags_BordersOuter | ImGuiTableFlags_RowBg |
                             ImGuiTableFlags_ScrollY,
                         ImVec2(0.0F, height))) {
    return;
  }
  const ResultColumns columns = ResultColumnsOf(art, view.amount_shown);
  ImGui::TableSetupScrollFreeze(0, 1);
  ImGui::TableSetupColumn(core::Tr("Item"), ImGuiTableColumnFlags_WidthFixed, columns.item);
  ImGui::TableSetupColumn(core::Tr("Prix"),
                          ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_DefaultSort |
                              ImGuiTableColumnFlags_PreferSortAscending,
                          columns.price);
  ImGui::TableSetupColumn(
      core::Tr("Qté"),
      ImGuiTableColumnFlags_WidthFixed | (view.amount_shown ? 0 : ImGuiTableColumnFlags_Disabled),
      columns.amount);
  const bool npc = state.tab == kMarketNpcTab;
  ImGui::TableSetupColumn(npc ? core::Tr("PNJ") : core::Tr("Échoppe"),
                          ImGuiTableColumnFlags_WidthStretch);
  ImGui::TableSetupColumn(core::Tr("Carte"), ImGuiTableColumnFlags_WidthFixed, columns.map);
  ImGui::TableSetupColumn("##market_actions",
                          ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoSort |
                              ImGuiTableColumnFlags_NoResize,
                          columns.actions);
  ImGui::TableHeadersRow();
  ReadSort(state);
  for (std::size_t rank = 0; rank < view.rows.size(); ++rank) {
    const MarketRowView& row = view.rows[rank];
    const int row_index = static_cast<int>(rank);
    ImGui::PushID(row_index);
    ImGui::TableNextRow();
    if (row.compared) {
      ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg1,
                             OnBackground(kComparedRowColor, PaintingLook().body));
    }
    ImGui::TableNextColumn();
    const bool expanded = state.expanded_row == row_index;
    if (!row.details.empty()) {
      if (ImGui::ArrowButton("##market_expand", expanded ? ImGuiDir_Down : ImGuiDir_Right)) {
        state.expanded_row = expanded ? kMarketNoRow : row_index;
      }
      ImGui::SameLine();
    }
    DrawItemCell(row, row_index, false, state.item_menu, action);
    ImGui::TableNextColumn();
    ImGui::AlignTextToFramePadding();
    TextRightAligned(row.price);
    ImGui::TableNextColumn();
    if (view.amount_shown) {
      ImGui::AlignTextToFramePadding();
      TextRightAligned(row.amount);
    }
    ImGui::TableNextColumn();
    DrawShopCell(row, npc);
    ImGui::TableNextColumn();
    ImGui::AlignTextToFramePadding();
    CellText(row.place);
    ImGui::TableNextColumn();
    if (SkinSmallButton(core::Tr("Y aller"), art)) {
      action.go_to_row = row_index;
    }
    if (ImGui::IsItemHovered()) {
      SkinTooltip(npc ? core::Tr("Guide jusqu'au PNJ") : core::Tr("Guide jusqu'à l'échoppe"));
    }
    ImGui::SameLine();
    if (SkinSmallButton(core::Tr("Voir"), art)) {
      action.view_store_row = row_index;
    }
    if (ImGui::IsItemHovered()) {
      SkinTooltip(npc ? core::Tr("Montre tout ce que vend ce PNJ")
                      : core::Tr("Montre tout le contenu de l'échoppe"));
    }
    if (expanded) {
      DrawDetails(row);
    }
    ImGui::PopID();
  }
  ImGui::EndTable();
}

void DrawFooter(MarketWindowState& state, const MarketView& view, const SkinArt& art) {
  if (!view.status.empty()) {
    ColoredText(kTradeRefusedText, view.status);
  } else {
    Unformatted(view.summary);
  }
  state.page = std::clamp(state.page, 1, std::max(view.page_count, 1));
  std::array<char, kMarketQueryBytes> pages{};
  std::snprintf(pages.data(), pages.size(), core::Tr("Page %d / %d"), state.page,
                std::max(view.page_count, 1));
  ImGui::BeginDisabled(state.page <= 1);
  if (SkinSmallButton("<##market_previous", art)) {
    --state.page;
    state.expanded_row = kMarketNoRow;
  }
  ImGui::EndDisabled();
  ImGui::SameLine();
  ImGui::AlignTextToFramePadding();
  ImGui::TextUnformatted(pages.data());
  ImGui::SameLine();
  ImGui::BeginDisabled(state.page >= view.page_count);
  if (SkinSmallButton(">##market_next", art)) {
    ++state.page;
    state.expanded_row = kMarketNoRow;
  }
  ImGui::EndDisabled();
}

// LE CONTENU D'UNE ÉCHOPPE, dans sa fenêtre : rien ne s'y achète.
void DrawStoreWindow(MarketWindowState& state, const MarketView& view, const SkinArt& art,
                     ItemLinkFacts link_facts, MarketAction& action) {
  if (!state.store_open) {
    return;
  }
  const float scale = Scale();
  ImGui::SetNextWindowSize(
      ImVec2(kMarketStoreWindowWidthPixels * scale, kMarketStoreWindowHeightPixels * scale),
      ImGuiCond_FirstUseEver);
  const std::string title = std::string(view.store_title) + kMarketStoreWindowId;
  const bool body_shown = BeginSkinnedWindow(title.c_str(), &state.store_open, art,
                                             SkinnedTitle::kBar, ImGuiWindowFlags_NoCollapse);
  if (body_shown && state.store_open) {
    if (!view.store_status.empty()) {
      Unformatted(view.store_status);
    }
    constexpr int kColumns = 3;
    if (ImGui::BeginTable("##market_store", kColumns,
                          ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingFixedFit |
                              ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_BordersOuter |
                              ImGuiTableFlags_RowBg)) {
      ImGui::TableSetupColumn(core::Tr("Item"), ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableSetupColumn(core::Tr("Prix"), ImGuiTableColumnFlags_WidthFixed,
                              kPriceColumnPixels * scale);
      ImGui::TableSetupColumn(core::Tr("Qté"),
                              ImGuiTableColumnFlags_WidthFixed |
                                  (view.store_amount_shown ? 0 : ImGuiTableColumnFlags_Disabled),
                              kAmountColumnPixels * scale);
      ImGui::TableHeadersRow();
      for (std::size_t rank = 0; rank < view.store_rows.size(); ++rank) {
        const MarketRowView& row = view.store_rows[rank];
        const int row_index = static_cast<int>(rank);
        ImGui::PushID(row_index);
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        DrawItemCell(row, row_index, true, state.store_item_menu, action);
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        TextRightAligned(row.price);
        ImGui::TableNextColumn();
        if (view.store_amount_shown) {
          ImGui::AlignTextToFramePadding();
          TextRightAligned(row.amount);
        }
        ImGui::PopID();
      }
      ImGui::EndTable();
    }
    if (const ChatLinkAsk chosen =
            DrawTradeItemMenu(kStoreItemMenuId, state.store_item_menu, link_facts);
        chosen.gesture != ChatLinkGesture::kNothing) {
      action.describe_row = static_cast<int>(chosen.token);
      action.describe_in_store = true;
      action.item_gesture = chosen.gesture;
    }
  }
  EndSkinnedWindow();
}

}  // namespace

void OpenMarketOn(MarketWindowState& state, std::string_view query) {
  const int tab = state.tab;
  const int sort_column = state.sort_column;
  const bool sort_descending = state.sort_descending;
  state = MarketWindowState();
  state.open = true;
  state.raise = true;
  state.tab = tab;
  state.sort_column = sort_column;
  state.sort_descending = sort_descending;
  const std::size_t length = std::min(query.size(), state.query.size() - 1);
  std::copy_n(query.data(), length, state.query.data());
}

MarketAction DrawMarketWindow(MarketWindowState& state, const MarketView& view, const SkinArt& art,
                              ItemLinkFacts link_facts) {
  MarketAction action;
  if (!state.open) {
    return action;
  }
  const float scale = Scale();
  if (state.raise) {
    ImGui::SetNextWindowFocus();
    state.raise = false;
  }
  ImGui::SetNextWindowSize(
      ImVec2(kMarketWindowWidthPixels * scale, kMarketWindowHeightPixels * scale),
      ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSizeConstraints(
      ImVec2(LeastWindowWidth(ResultColumnsOf(art, view.amount_shown)), 0.0F),
      ImVec2(std::numeric_limits<float>::max(), std::numeric_limits<float>::max()));
  const std::string title = std::string(core::Tr("Marché")) + kMarketWindowId;
  const bool body_shown = BeginSkinnedWindow(title.c_str(), &state.open, art, SkinnedTitle::kBar,
                                             ImGuiWindowFlags_NoCollapse);
  if (body_shown && state.open) {
    DrawSearchBar(state, view, art, action);
    DrawFilters(state, art);
    ImGui::Separator();
    DrawResults(state, view, art, action);
    DrawFooter(state, view, art);
    if (const ChatLinkAsk chosen = DrawTradeItemMenu(kItemMenuId, state.item_menu, link_facts);
        chosen.gesture != ChatLinkGesture::kNothing) {
      action.describe_row = static_cast<int>(chosen.token);
      action.describe_in_store = false;
      action.item_gesture = chosen.gesture;
    }
  }
  EndSkinnedWindow();
  if (!state.open) {
    state.store_open = false;
    action = MarketAction();
    action.close = true;
    return action;
  }
  DrawStoreWindow(state, view, art, link_facts, action);
  return action;
}

}  // namespace ro::ui
