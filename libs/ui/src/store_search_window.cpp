#include "ro/ui/store_search_window.h"

#include <algorithm>
#include <array>
#include <string>
#include <utility>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/ui/modal_box.h"
#include "ro/ui/skin_art.h"
#include "ro/ui/skin_tooltip.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"

namespace ro::ui {
namespace {

[[nodiscard]] float Scale() { return ImGui::GetStyle().FontScaleMain; }

// ── LES MESURES DU CLIENT D'ORIGINE ─────────────────────────────────────────
// La largeur de la fenêtre et sa hauteur : dix lignes de vingt-cinq pixels,
// l'en-tête et le formulaire au-dessus, le pied en dessous.
constexpr float kFirstWidthPixels = 440.0F;
constexpr float kFirstHeightPixels = 390.0F;
constexpr float kRowPixels = 25.0F;
// Les colonnes : icône, échoppe, objet, quantité, prix.
constexpr float kIconColumnPixels = 45.0F;
constexpr float kShopColumnPixels = 110.0F;
constexpr float kCountColumnPixels = 30.0F;
constexpr float kPriceColumnPixels = 75.0F;
// La largeur d'un champ de saisie.
constexpr float kFieldPixels = 76.0F;
// La ligne survolée, peinte d'un bleu pâle.
constexpr ImU32 kHoveredRowColor = IM_COL32(202, 223, 251, 255);
// L'unité qui suit un prix, espace comprise.
constexpr std::string_view kPriceUnit = " Z";

constexpr const char* kConfirmId = "##store_search_confirm";

// LES ORDRES DE GRANDEUR D'UN PRIX, du plus haut au plus bas, et leur encre.
struct PriceTier {
  std::int64_t from_zeny = 0;
  StoreSearchPriceInk ink;
};
constexpr ImU32 kNoShadow = IM_COL32(0, 0, 0, 0);
constexpr ImU32 kInkBlack = IM_COL32(0, 0, 0, 255);
constexpr ImU32 kInkRed = IM_COL32(255, 0, 0, 255);
constexpr ImU32 kInkBlue = IM_COL32(0, 0, 255, 255);
constexpr std::array<PriceTier, 10> kPriceTiers = {{
    {1'000'000'000, {kInkRed, IM_COL32(255, 0, 127, 255)}},
    {100'000'000, {kInkBlack, IM_COL32(200, 200, 100, 255)}},
    {10'000'000, {kInkRed, kNoShadow}},
    {1'000'000, {kInkBlack, IM_COL32(0, 255, 0, 255)}},
    {100'000, {kInkBlue, kNoShadow}},
    {10'000, {IM_COL32(255, 30, 255, 255), kNoShadow}},
    {1'000, {kInkRed, IM_COL32(255, 255, 0, 255)}},
    {100, {kInkBlue, IM_COL32(0, 255, 255, 255)}},
    {10, {kInkBlue, IM_COL32(200, 0, 200, 255)}},
    {0, {kInkBlack, IM_COL32(0, 255, 255, 255)}},
}};

void TextOf(std::string_view text) {
  ImGui::TextUnformatted(text.data(), text.data() + text.size());
}

void TextRightAligned(std::string_view text) {
  const float width = ImGui::CalcTextSize(text.data(), text.data() + text.size()).x;
  ImGui::SetCursorPosX(ImGui::GetCursorPosX() +
                       std::max(0.0F, ImGui::GetContentRegionAvail().x - width));
  TextOf(text);
}

// UN PRIX, aligné à droite de sa colonne : l'ombre d'abord, décalée d'un
// pixel, puis le texte.
void DrawPrice(std::int64_t price_zeny) {
  const std::string text = TradeAmountText(price_zeny, kPriceUnit);
  const ImVec2 size = ImGui::CalcTextSize(text.c_str());
  ImGui::SetCursorPosX(ImGui::GetCursorPosX() +
                       std::max(0.0F, ImGui::GetContentRegionAvail().x - size.x));
  const ImVec2 at = ImGui::GetCursorScreenPos();
  // ⚠ LES ENCRES DU JEU SONT CHOISIES POUR UN FOND CLAIR : elles se traduisent
  // pour celui de l'habillage.
  const StoreSearchPriceInk ink = StoreSearchPriceInkOf(price_zeny);
  const ImU32 body = PaintingLook().body;
  ImDrawList* into = ImGui::GetWindowDrawList();
  if ((ink.shadow & IM_COL32_A_MASK) != 0) {
    into->AddText(ImVec2(at.x + 1.0F, at.y), OnBackground(ink.shadow, body), text.c_str());
  }
  into->AddText(at, InkOn(ink.text, body), text.c_str());
  ImGui::Dummy(size);
}

void DrawIcon(const TradeItemView& item) {
  const float side = kTradeIconPixels * Scale();
  if (item.icon.usable()) {
    ImGui::Image(item.icon.id, ImVec2(side, side));
  } else {
    ImGui::Dummy(ImVec2(side, side));
  }
}

// La bulle et le clic droit d'une cellule qui montre l'objet.
void NoteItemCell(int row, const TradeItemView& item, StoreSearchAction& action) {
  if (!ImGui::IsItemHovered()) {
    return;
  }
  action.hovered_row = row;
  SkinTooltip(std::string(item.name).c_str());
  if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
    action.describe_row = row;
  }
}

void DrawForm(StoreSearchWindowState& state, const StoreSearchView& view, const SkinArt& art,
              StoreSearchAction& action) {
  const float scale = Scale();
  const bool named = state.item_name[0] != '\0';
  if (!ImGui::BeginTable("##store_search_form", 4, ImGuiTableFlags_SizingFixedFit)) {
    return;
  }
  // ⚠ CES QUATRE ÉTIQUETTES SONT EN DUR chez le client d'origine : ce sont
  // les seules de la fenêtre qui passent par notre catalogue.
  ImGui::TableSetupColumn(core::Tr("Type"));
  ImGui::TableSetupColumn(core::Tr("Item"));
  ImGui::TableSetupColumn(core::Tr("Carte sertie"));
  ImGui::TableSetupColumn(core::Tr("Zeny"));
  ImGui::TableHeadersRow();
  ImGui::TableNextRow();
  ImGui::TableNextColumn();
  if (SkinRadio(std::string(view.words.sell).append("##ss_sell").c_str(),
                state.kind == kStoreSearchSellKind, art)) {
    state.kind = kStoreSearchSellKind;
  }
  if (SkinRadio(std::string(view.words.buy).append("##ss_buy").c_str(),
                state.kind == kStoreSearchBuyKind, art)) {
    state.kind = kStoreSearchBuyKind;
  }
  ImGui::TableNextColumn();
  ImGui::SetNextItemWidth(kFieldPixels * scale);
  ImGui::InputText("##ss_item", state.item_name.data(), state.item_name.size());
  // LE RESTE DU FORMULAIRE SE GRISE tant qu'aucun objet n'est nommé.
  ImGui::BeginDisabled(!named);
  ImGui::TableNextColumn();
  ImGui::SetNextItemWidth(kFieldPixels * scale);
  ImGui::InputTextWithHint("##ss_card", std::string(view.words.card_hint).c_str(),
                           state.card_name.data(), state.card_name.size());
  if (!view.words.card_hint.empty() && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    SkinTooltip(std::string(view.words.card_hint).c_str());
  }
  ImGui::TableNextColumn();
  ImGui::SetNextItemWidth(kFieldPixels * scale);
  ImGui::InputText("##ss_min", state.min_price.data(), state.min_price.size());
  ImGui::SameLine();
  ImGui::TextUnformatted("~");
  ImGui::SameLine();
  ImGui::SetNextItemWidth(kFieldPixels * scale);
  ImGui::InputText("##ss_max", state.max_price.data(), state.max_price.size());
  ImGui::EndDisabled();
  ImGui::EndTable();

  static_cast<void>(SkinCheckBox(std::string(view.words.similar).append("##ss_similar").c_str(),
                                 &state.similar, art));
  ImGui::SameLine();
  ImGui::BeginDisabled(!named);
  if (SkinButton(core::Tr("Recherche"), art)) {
    action.search = true;
  }
  ImGui::EndDisabled();
  ImGui::SameLine();
  TextRightAligned(view.words.uses_left);
}

void DrawResults(StoreSearchWindowState& state, const StoreSearchView& view,
                 StoreSearchAction& action) {
  const float scale = Scale();
  if (!ImGui::BeginTable("##store_search_results", 5,
                         ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_BordersInnerV |
                             ImGuiTableFlags_BordersOuter)) {
    return;
  }
  ImGui::TableSetupColumn("##ss_icon", ImGuiTableColumnFlags_WidthFixed, kIconColumnPixels * scale);
  ImGui::TableSetupColumn(std::string(view.words.shop_name).append("##ss_shop").c_str(),
                          ImGuiTableColumnFlags_WidthFixed, kShopColumnPixels * scale);
  ImGui::TableSetupColumn(std::string(view.words.item).append("##ss_item_col").c_str(),
                          ImGuiTableColumnFlags_WidthStretch);
  ImGui::TableSetupColumn(std::string(view.words.count).append("##ss_count").c_str(),
                          ImGuiTableColumnFlags_WidthFixed, kCountColumnPixels * scale);
  ImGui::TableSetupColumn(std::string(view.words.price).append("##ss_price").c_str(),
                          ImGuiTableColumnFlags_WidthFixed, kPriceColumnPixels * scale);
  ImGui::TableHeadersRow();
  const int hovered_before = std::exchange(state.hovered_row, kStoreSearchNoRow);
  for (std::size_t rank = 0; rank < view.rows.size(); ++rank) {
    const StoreSearchRowView& row = view.rows[rank];
    const int row_index = static_cast<int>(rank);
    ImGui::PushID(row_index);
    ImGui::TableNextRow(ImGuiTableRowFlags_None, kRowPixels * scale);
    if (row_index == hovered_before) {
      ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,
                             OnBackground(kHoveredRowColor, PaintingLook().body));
    }
    ImGui::TableNextColumn();
    DrawIcon(row.item);
    NoteItemCell(row_index, row.item, action);
    ImGui::TableNextColumn();
    ImGui::AlignTextToFramePadding();
    // 🔴 LE DOUBLE-CLIC SUR L'ÉCHOPPE, et lui seul, la désigne au serveur.
    ImGui::Selectable(std::string(row.shop_name).append("##ss_pick").c_str(), false,
                      ImGuiSelectableFlags_AllowDoubleClick);
    if (ImGui::IsItemHovered()) {
      action.hovered_row = row_index;
      SkinTooltip(std::string(row.shop_name).c_str());
      if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        const StoreSearchPick pick{
            .row = row_index, .store_id = row.store_id, .owner_aid = row.owner_aid};
        if (view.click_opens_store) {
          state.pending = pick;
          state.pending_asked = false;
        } else {
          action.clicked = pick;
        }
      }
    }
    ImGui::TableNextColumn();
    ImGui::AlignTextToFramePadding();
    TextOf(row.item.name);
    NoteItemCell(row_index, row.item, action);
    ImGui::TableNextColumn();
    ImGui::AlignTextToFramePadding();
    const std::string amount = std::to_string(row.amount);
    TextRightAligned(amount);
    ImGui::TableNextColumn();
    ImGui::AlignTextToFramePadding();
    DrawPrice(row.price_zeny);
    if (ImGui::IsItemHovered()) {
      action.hovered_row = row_index;
    }
    ImGui::PopID();
  }
  ImGui::EndTable();
  state.hovered_row = action.hovered_row;
}

void DrawFooter(StoreSearchWindowState& state, const StoreSearchView& view, const SkinArt& art,
                StoreSearchAction& action) {
  if (!view.status.empty()) {
    ImGui::TextColored(kTradeRefusedText, "%.*s", static_cast<int>(view.status.size()),
                       view.status.data());
  } else {
    ImGui::NewLine();
  }
  const std::string previous = std::string(view.words.previous_page).append("##ss_prev");
  const std::string next = std::string(view.words.next_page).append("##ss_next");
  const std::string page = " " + std::to_string(state.page) + " ";
  const float width =
      SkinSmallButtonWidth(previous.c_str(), art) + ImGui::CalcTextSize(page.c_str()).x +
      SkinSmallButtonWidth(next.c_str(), art) + 2.0F * ImGui::GetStyle().ItemSpacing.x;
  ImGui::SetCursorPosX(ImGui::GetCursorPosX() +
                       std::max(0.0F, (ImGui::GetContentRegionAvail().x - width) / 2.0F));
  ImGui::BeginDisabled(state.page <= 1);
  if (SkinSmallButton(previous.c_str(), art)) {
    --state.page;
  }
  ImGui::EndDisabled();
  ImGui::SameLine();
  ImGui::AlignTextToFramePadding();
  ImGui::TextUnformatted(page.c_str());
  ImGui::SameLine();
  ImGui::BeginDisabled(!view.next_held && !view.next_askable);
  if (SkinSmallButton(next.c_str(), art)) {
    // LA PAGE AVANCE AVANT LA RÉPONSE : ce qui arrive s'y montre.
    ++state.page;
    action.next_page = !view.next_held;
  }
  ImGui::EndDisabled();
}

void DrawConfirmation(StoreSearchWindowState& state, const StoreSearchView& view,
                      const SkinArt& art, StoreSearchAction& action) {
  if (!state.pending.chosen()) {
    return;
  }
  if (!state.pending_asked) {
    AskModal(kConfirmId);
    state.pending_asked = true;
  }
  switch (DrawModalBox(kConfirmId, std::string(view.words.open_store).c_str(), core::Tr("Oui"),
                       core::Tr("Non"), art)) {
    case ModalAnswer::kYes:
      action.clicked = std::exchange(state.pending, StoreSearchPick());
      break;
    case ModalAnswer::kNo:
      state.pending = StoreSearchPick();
      break;
    case ModalAnswer::kWaiting:
      if (!ImGui::IsPopupOpen(kConfirmId)) {
        state.pending = StoreSearchPick();
      }
      break;
  }
}

}  // namespace

void OpenStoreSearchOn(StoreSearchWindowState& state, std::uint32_t opening_seq) {
  const std::uint32_t first_page_seen = state.first_page_seen;
  state = StoreSearchWindowState();
  state.open = true;
  state.opening_seen = opening_seq;
  state.first_page_seen = first_page_seen;
}

StoreSearchPriceInk StoreSearchPriceInkOf(std::int64_t price_zeny) {
  for (const PriceTier& tier : kPriceTiers) {
    if (price_zeny >= tier.from_zeny) {
      return tier.ink;
    }
  }
  return kPriceTiers.back().ink;
}

StoreSearchAction DrawStoreSearchWindow(StoreSearchWindowState& state, const StoreSearchView& view,
                                        const SkinArt& art) {
  StoreSearchAction action;
  if (!state.open) {
    return action;
  }
  const float scale = Scale();
  ImGui::SetNextWindowSize(ImVec2(kFirstWidthPixels * scale, kFirstHeightPixels * scale),
                           ImGuiCond_FirstUseEver);
  const std::string title = std::string(view.words.title) + kStoreSearchWindowId;
  const bool body_shown = BeginSkinnedWindow(title.c_str(), &state.open, art, SkinnedTitle::kBar,
                                             ImGuiWindowFlags_NoCollapse);
  if (body_shown && state.open) {
    DrawForm(state, view, art, action);
    ImGui::Separator();
    DrawResults(state, view, action);
    DrawFooter(state, view, art, action);
    DrawConfirmation(state, view, art, action);
  }
  EndSkinnedWindow();
  if (!state.open) {
    action = StoreSearchAction();
    action.close = true;
  }
  return action;
}

}  // namespace ro::ui
