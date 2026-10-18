#include "ro/ui/vending_setup_window.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cstdio>
#include <string>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/ui/inventory_window.h"
#include "ro/ui/modal_box.h"
#include "ro/ui/skin_popup.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"

#include "trade_gestures.h"
#include "ui_scale.h"
#include "ui_text.h"

namespace ro::ui {
namespace {

// ── LES MESURES DE LA FENÊTRE ───────────────────────────────────────────────
// ⚠ TOUTES À L'ÉCHELLE de l'interface, jamais en pixels bruts.
constexpr float kFirstWidthPixels = 560.0F;
constexpr float kFirstHeightPixels = 430.0F;
constexpr float kLeastWidthPixels = 430.0F;
constexpr float kLeastHeightPixels = 300.0F;
// Le décalage du champ de nom derrière son libellé.
constexpr float kNameLabelPixels = 45.0F;
// Les colonnes du stock en liste.
constexpr float kStockLeftColumnPixels = 46.0F;
constexpr float kStockAmountColumnPixels = 56.0F;
// Les colonnes des lignes posées.
constexpr float kLineAmountColumnPixels = 56.0F;
constexpr float kLinePriceColumnPixels = 96.0F;
constexpr float kLineTotalColumnPixels = 88.0F;
// Le champ de la boîte « combien en poser », et son pas rapide.
constexpr float kAskFieldPixels = 120.0F;
constexpr int kAskFastStep = 10;
// Le stock garde au moins quatre lignes, les lignes posées trois.
constexpr float kStockLeastLines = 4.0F;
constexpr float kPlacedLeastLines = 3.0F;
// Les rangs d'identité des cases de la grille, loin de ceux des tables.
constexpr int kTileIdBase = 20000;

constexpr const char* kStockPayload = "VEND_AVAIL";
constexpr const char* kLinePayload = "VEND_ROW";
constexpr const char* kAskPopupId = "##vending_setup_ask";
constexpr const char* kConfirmId = "##vending_setup_confirm";
constexpr const char* kItemMenuId = "##vending_setup_item_menu";

const VendingStockView* StockAt(const VendingSetupView& view, std::uint16_t cart_slot) {
  const auto found = std::ranges::find_if(
      view.stock, [cart_slot](const VendingStockView& row) { return row.cart_slot == cart_slot; });
  return found == view.stock.end() ? nullptr : &*found;
}

// Combien poser de cette pile, en liste : par défaut, tout ce qui reste.
int& StockAmountFor(VendingSetupWindowState& state, std::uint16_t cart_slot, int remaining) {
  auto found = std::ranges::find_if(
      state.stock_amounts, [cart_slot](const auto& kept) { return kept.first == cart_slot; });
  if (found == state.stock_amounts.end()) {
    state.stock_amounts.emplace_back(cart_slot, remaining);
    found = state.stock_amounts.end() - 1;
  }
  found->second = std::clamp(found->second, std::min(1, remaining), std::max(remaining, 0));
  return found->second;
}

// Ce que le joueur a demandé pendant l'image, appliqué APRÈS les tables : une
// ligne qui disparaît au milieu de sa propre boucle décalerait toutes les
// identités qui suivent.
struct SetupGesture {
  std::uint16_t place_slot = 0;
  std::uint16_t take_slot = 0;
};

void DrawName(VendingSetupWindowState& state, const VendingSetupView& view) {
  const float scale = Scale();
  ImGui::AlignTextToFramePadding();
  ImGui::TextUnformatted(core::Tr("Nom"));
  ImGui::SameLine(kNameLabelPixels * scale);
  ImGui::SetNextItemWidth(-1.0F);
  ImGui::InputTextWithHint("##vending_title", core::Tr("Le nom que l'enseigne montrera"),
                           state.title.data(), state.title.size());
  // 🔴 LE TITRE TEL QU'IL PARTIRA : le fil le coupe à quatre-vingts octets, et
  // l'enseigne montrera la coupe, pas ce qu'on a tapé.
  if (view.title_cut) {
    ImGui::TextColored(kTradeRefusedText, core::Tr("Coupé à : « %.*s »"),
                       static_cast<int>(view.fitted_title.size()), view.fitted_title.data());
  } else {
    ImGui::TextDisabled(core::Tr("%zu octet(s) restant(s) pour le nom"), view.title_bytes_left);
  }
}

void DrawStockHeader(VendingSetupWindowState& state, const VendingSetupView& view,
                     const SkinArt& art, VendingSetupAction& action) {
  ImGui::AlignTextToFramePadding();
  ImGui::TextUnformatted(core::Tr("Items du cart"));
  ImGui::SameLine();
  ImGui::TextDisabled("(%zu)", view.stock.size());
  const char* grid_label = core::Tr("Grille");
  const float width = SkinCheckBoxWidth(grid_label, art);
  ImGui::SameLine(std::max(ImGui::GetCursorPosX(), ImGui::GetContentRegionMax().x - width));
  if (SkinCheckBox(grid_label, &state.grid, art)) {
    action.settings_changed = true;
  }
  if (ImGui::IsItemHovered()) {
    SkinTooltip(
        core::Tr("Montre le stock en grille d'icônes, comme le client d'origine.\nLe glisser "
                 "marche dans les deux modes."));
  }
}

// Un geste de pose sur une ligne du stock, s'il est permis.
bool CanPlace(const VendingSetupWindowState& state, const VendingSetupView& view,
              const VendingStockView& row, int remaining) {
  const bool has_line = std::ranges::any_of(state.lines, [&row](const VendingSetupLineState& line) {
    return line.cart_slot == row.cart_slot;
  });
  const bool full = !has_line && state.lines.size() >= view.permitted_lines;
  return row.refusal.empty() && remaining > 0 && !full && !view.awaiting;
}

void StockTooltip(const VendingSetupWindowState& state, const VendingSetupView& view,
                  const VendingStockView& row, int remaining) {
  if (!row.refusal.empty()) {
    SkinTooltip(std::string(row.refusal).c_str());
  } else if (remaining <= 0) {
    SkinTooltip(core::Tr("Toute la pile est déjà dans le shop."));
  } else if (state.lines.size() >= view.permitted_lines) {
    SkinTooltip(core::Tr("Toutes les lignes sont prises."));
  }
}

void DrawStockList(VendingSetupWindowState& state, const VendingSetupView& view, const SkinArt& art,
                   SetupGesture& gesture, VendingSetupAction& action) {
  const float scale = Scale();
  if (!ImGui::BeginTable("##vending_stock_list", 4,
                         ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg)) {
    return;
  }
  ImGui::TableSetupColumn(core::Tr("Item"), ImGuiTableColumnFlags_WidthStretch);
  ImGui::TableSetupColumn(core::Tr("Reste"), ImGuiTableColumnFlags_WidthFixed,
                          kStockLeftColumnPixels * scale);
  ImGui::TableSetupColumn(core::Tr("Qté"), ImGuiTableColumnFlags_WidthFixed,
                          kStockAmountColumnPixels * scale);
  ImGui::TableSetupColumn("##act", ImGuiTableColumnFlags_WidthFixed,
                          SkinSmallButtonWidth(core::Tr("Poser"), art));
  ImGui::TableHeadersRow();
  for (const VendingStockView& row : view.stock) {
    const int remaining = row.amount - PlacedOnVendingSetup(state, row.cart_slot);
    ImGui::PushID(row.cart_slot);
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    NoteItemBySlot(DrawTradeItem(row.item), row.item, row.cart_slot, state.item_menu, action);
    if (row.refusal.empty() && remaining > 0 &&
        ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
      ImGui::SetDragDropPayload(kStockPayload, &row.cart_slot, sizeof(row.cart_slot));
      Unformatted(row.item.name);
      ImGui::EndDragDropSource();
    }
    ImGui::TableNextColumn();
    ImGui::AlignTextToFramePadding();
    ImGui::Text("%d", std::max(remaining, 0));
    ImGui::TableNextColumn();
    int& amount = StockAmountFor(state, row.cart_slot, remaining);
    ImGui::SetNextItemWidth(-1.0F);
    if (ImGui::InputInt("##q", &amount, 0, 0)) {
      amount = std::clamp(amount, std::min(1, remaining), std::max(remaining, 0));
    }
    ImGui::TableNextColumn();
    const bool can = CanPlace(state, view, row, remaining);
    ImGui::BeginDisabled(!can);
    if (SkinSmallButton(core::Tr("Poser"), art)) {
      gesture.place_slot = row.cart_slot;
    }
    ImGui::EndDisabled();
    if (!can && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
      StockTooltip(state, view, row, remaining);
    }
    ImGui::PopID();
  }
  ImGui::EndTable();
}

void DrawStockGrid(VendingSetupWindowState& state, const VendingSetupView& view,
                   SetupGesture& gesture, VendingSetupAction& action) {
  const float step = kTradeTilePixels * Scale() + ImGui::GetStyle().ItemSpacing.x;
  const int per_row =
      std::max(1, static_cast<int>(ImGui::GetContentRegionAvail().x / std::max(step, 1.0F)));
  int column = 0;
  for (const VendingStockView& row : view.stock) {
    const int remaining = row.amount - PlacedOnVendingSetup(state, row.cart_slot);
    if (column > 0) {
      ImGui::SameLine();
    }
    ImGui::PushID(kTileIdBase + row.cart_slot);
    const TradeItemGesture tile = DrawTradeTile(row.item, row.refine_level, remaining);
    NoteItemBySlot(tile, row.item, row.cart_slot, state.item_menu, action);
    const bool can = CanPlace(state, view, row, remaining);
    if (can && ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
      ImGui::SetDragDropPayload(kStockPayload, &row.cart_slot, sizeof(row.cart_slot));
      Unformatted(row.item.name);
      ImGui::EndDragDropSource();
    }
    if (tile.hovered && can && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
      gesture.place_slot = row.cart_slot;
    }
    if (tile.hovered && !can) {
      StockTooltip(state, view, row, remaining);
    }
    ImGui::PopID();
    column = (column + 1) % per_row;
  }
}

void DrawLines(VendingSetupWindowState& state, const VendingSetupView& view, const SkinArt& art,
               SetupGesture& gesture, VendingSetupAction& action) {
  const float scale = Scale();
  if (!ImGui::BeginTable("##vending_setup_lines", 5,
                         ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg)) {
    return;
  }
  ImGui::TableSetupColumn(core::Tr("Item"), ImGuiTableColumnFlags_WidthStretch);
  ImGui::TableSetupColumn(core::Tr("Qté"), ImGuiTableColumnFlags_WidthFixed,
                          kLineAmountColumnPixels * scale);
  ImGui::TableSetupColumn(core::Tr("Prix"), ImGuiTableColumnFlags_WidthFixed,
                          kLinePriceColumnPixels * scale);
  ImGui::TableSetupColumn(core::Tr("Total"), ImGuiTableColumnFlags_WidthFixed,
                          kLineTotalColumnPixels * scale);
  ImGui::TableSetupColumn("##act", ImGuiTableColumnFlags_WidthFixed,
                          SkinSmallButtonWidth("x", art));
  ImGui::TableHeadersRow();
  for (std::size_t rank = 0; rank < state.lines.size(); ++rank) {
    VendingSetupLineState& line = state.lines[rank];
    const VendingStockView* row = StockAt(view, line.cart_slot);
    ImGui::PushID(line.cart_slot);
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    if (row != nullptr) {
      NoteItemBySlot(DrawTradeItem(row->item), row->item, line.cart_slot, state.item_menu, action);
    } else {
      ImGui::AlignTextToFramePadding();
      ImGui::TextDisabled("%s", core::Tr("(n'est plus dans le cart)"));
    }
    if (!view.awaiting && ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
      ImGui::SetDragDropPayload(kLinePayload, &line.cart_slot, sizeof(line.cart_slot));
      ImGui::TextUnformatted(core::Tr("Retirer du shop"));
      ImGui::EndDragDropSource();
    }
    // LA FAUTE DE LA LIGNE, dite sur elle : la raison générale du bouton ne dit
    // pas laquelle des douze corriger.
    if (rank < view.line_faults.size() && !view.line_faults[rank].empty()) {
      ImGui::SameLine();
      ImGui::TextColored(kTradeRefusedText, "(!)");
      if (ImGui::IsItemHovered()) {
        SkinTooltip(std::string(view.line_faults[rank]).c_str());
      }
    }
    ImGui::TableNextColumn();
    const int stack = row != nullptr ? row->amount : line.amount;
    ImGui::SetNextItemWidth(-1.0F);
    if (ImGui::InputInt("##amount", &line.amount, 0, 0)) {
      line.amount = std::clamp(line.amount, 1, std::max(stack, 1));
    }
    ImGui::TableNextColumn();
    const std::optional<std::int64_t> price = VendingLinePrice(line);
    const bool unreadable = !price.has_value() && line.price_text[0] != '\0';
    if (unreadable) {
      ImGui::PushStyleColor(ImGuiCol_Text, kTradeRefusedText);
    }
    ImGui::SetNextItemWidth(-1.0F);
    ImGui::InputTextWithHint("##price", "0", line.price_text.data(), line.price_text.size());
    if (unreadable) {
      ImGui::PopStyleColor();
    }
    if (ImGui::IsItemHovered() && price.has_value() && *price >= kVendingHighPriceZeny) {
      SkinTooltip(SpelledTradeAmount(*price).c_str());
    }
    ImGui::TableNextColumn();
    ImGui::AlignTextToFramePadding();
    if (price.has_value()) {
      ImGui::TextColored(kTradeBlackText, "%s",
                         TradeAmountText(*price * line.amount, kZenySuffix).c_str());
    } else {
      ImGui::TextDisabled("-");
    }
    ImGui::TableNextColumn();
    if (SkinSmallButton("x", art)) {
      gesture.take_slot = line.cart_slot;
    }
    if (ImGui::IsItemHovered()) {
      SkinTooltip(core::Tr("Retirer du shop"));
    }
    ImGui::PopID();
  }
  ImGui::EndTable();
}

// LA BOÎTE « COMBIEN EN POSER », celle de la grille pour une pile de plus d'un.
void DrawAskPopup(VendingSetupWindowState& state, const VendingSetupView& view,
                  const SkinArt& art) {
  if (state.ask_slot == 0) {
    return;
  }
  const VendingStockView* row = StockAt(view, state.ask_slot);
  const int remaining =
      row == nullptr ? 0 : row->amount - PlacedOnVendingSetup(state, state.ask_slot);
  if (!BeginSkinPopup(kAskPopupId)) {
    // Fermée en cliquant ailleurs : c'est un renoncement.
    state.ask_slot = 0;
    return;
  }
  if (row == nullptr || remaining <= 0) {
    state.ask_slot = 0;
    ImGui::CloseCurrentPopup();
    EndSkinPopup();
    return;
  }
  Unformatted(row->item.name);
  ImGui::TextDisabled(core::Tr("Reste : %d"), remaining);
  ImGui::Separator();
  // ⚠ LE CHAMP SANS PAS, ENTRE DEUX BOUTONS HABILLÉS : les flèches du champ à
  // pas d'ImGui ne s'habillent pas. Ils sont posés à la main, et non par
  // `SkinInputInt`, parce que le champ prend le focus à l'ouverture et que
  // Entrée y vaut « Poser ».
  const int ask_step = ImGui::GetIO().KeyCtrl ? kAskFastStep : 1;
  ImGui::PushItemFlag(ImGuiItemFlags_ButtonRepeat, true);
  if (SkinSmallButton("-", art)) {
    state.ask_amount -= ask_step;
  }
  ImGui::SameLine();
  ImGui::SetNextItemWidth(kAskFieldPixels * Scale());
  if (ImGui::IsWindowAppearing()) {
    // ⚠ VISE L'ÉLÉMENT SUIVANT, donc posé AVANT lui.
    ImGui::SetKeyboardFocusHere();
  }
  bool place = ImGui::InputInt("##vending_ask_amount", &state.ask_amount, 0, 0,
                               ImGuiInputTextFlags_EnterReturnsTrue);
  ImGui::SameLine();
  if (SkinSmallButton("+", art)) {
    state.ask_amount += ask_step;
  }
  ImGui::PopItemFlag();
  state.ask_amount = std::clamp(state.ask_amount, 1, remaining);
  if (SkinSmallButton(core::Tr("Tout"), art)) {
    state.ask_amount = remaining;
  }
  ImGui::SameLine();
  place |= SkinSmallButton(core::Tr("Poser"), art);
  ImGui::SameLine();
  const bool cancel = SkinSmallButton(core::Tr("Annuler"), art);
  if (place) {
    PlaceOnVendingSetup(state, state.ask_slot, state.ask_amount, row->amount, view.permitted_lines);
  }
  if (place || cancel) {
    state.ask_slot = 0;
    ImGui::CloseCurrentPopup();
  }
  EndSkinPopup();
}

void ApplyGesture(VendingSetupWindowState& state, const VendingSetupView& view,
                  const SetupGesture& gesture) {
  if (gesture.place_slot != 0) {
    const VendingStockView* row = StockAt(view, gesture.place_slot);
    if (row == nullptr) {
      return;
    }
    const int remaining = row->amount - PlacedOnVendingSetup(state, row->cart_slot);
    // En grille, une pile de plus d'un demande combien ; en liste, le champ
    // de la ligne l'a déjà dit.
    if (state.grid && remaining > 1) {
      state.ask_slot = row->cart_slot;
      state.ask_amount = remaining;
      ImGui::OpenPopup(kAskPopupId);
    } else {
      const int amount = state.grid ? remaining : StockAmountFor(state, row->cart_slot, remaining);
      PlaceOnVendingSetup(state, row->cart_slot, amount, row->amount, view.permitted_lines);
    }
  } else if (gesture.take_slot != 0) {
    TakeFromVendingSetup(state, gesture.take_slot);
  }
}

// La question des prix élevés : chaque ligne, son montant en chiffres PUIS en
// toutes lettres, pour qu'un zéro de trop se voie.
std::string HighPriceQuestion(const VendingSetupWindowState& state, const VendingSetupView& view) {
  std::string question = core::Tr("Ces prix sont élevés. Les confirmer ?");
  for (const std::size_t rank : VendingHighPricedLines(state)) {
    const VendingSetupLineState& line = state.lines[rank];
    const VendingStockView* row = StockAt(view, line.cart_slot);
    const std::int64_t price = VendingLinePrice(line).value_or(0);
    question += "\n\n";
    question += row != nullptr ? std::string(row->item.name) : std::string("?");
    question += " : ";
    question += TradeAmountText(price, kZenySuffix);
    question += "\n(";
    question += SpelledTradeAmount(price);
    question += " zeny)";
  }
  return question;
}

void DrawFooter(VendingSetupWindowState& state, const VendingSetupView& view, const SkinArt& art,
                VendingSetupAction& action) {
  ImGui::Separator();
  ImGui::Text(core::Tr("Recette si tout se vend : %s"),
              TradeAmountText(view.takings, kZenySuffix).c_str());
  if (view.zeny != kTradeNoPurse) {
    ImGui::Text(core::Tr("Zeny : %s"), TradeAmountText(view.zeny, kZenySuffix).c_str());
    ImGui::SameLine();
  }
  ImGui::TextDisabled(core::Tr("%zu/%zu ligne(s)"), state.lines.size(), view.permitted_lines);
  if (SkinCheckBox(core::Tr("Confirmer les prix élevés"), &state.confirm_high_prices, art)) {
    action.settings_changed = true;
  }
  ImGui::SameLine();
  ImGui::TextDisabled("(?)");
  if (ImGui::IsItemHovered()) {
    SkinTooltip(
        core::Tr("Demande une confirmation, montant écrit en toutes lettres, pour tout prix "
                 "d'au moins 10,000,000z."));
  }
  // 🔴 LE BOUTON SE RETIENT tant que le serveur refuserait, ou fausserait : la
  // première raison s'écrit ici, toutes se lisent au survol du bouton.
  if (!view.blockers.empty()) {
    ColoredText(kTradeRefusedText, view.blockers[0]);
  } else if (view.awaiting) {
    ImGui::TextDisabled("%s", core::Tr("Demande envoyée : en attente du serveur..."));
  } else {
    ImGui::TextDisabled(" ");
  }
  ImGui::Separator();
  const bool blocked = !view.blockers.empty() || view.awaiting;
  ImGui::BeginDisabled(blocked);
  if (SkinButton(core::Tr("Ouvrir le shop"), art)) {
    if (state.confirm_high_prices && !VendingHighPricedLines(state).empty()) {
      state.confirming = true;
      AskModal(kConfirmId);
    } else {
      action.open_shop = true;
    }
  }
  ImGui::EndDisabled();
  if (!view.blockers.empty() && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    std::string reasons;
    for (const std::string_view reason : view.blockers) {
      if (!reasons.empty()) {
        reasons.push_back('\n');
      }
      reasons.append(reason);
    }
    SkinTooltip(reasons.c_str());
  }
  if (!view.resume_title.empty()) {
    ImGui::SameLine();
    ImGui::BeginDisabled(view.awaiting);
    if (SkinButton(core::Tr("Reprendre la dernière échoppe"), art)) {
      action.resume = true;
    }
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
      std::array<char, 256> hint{};
      std::snprintf(hint.data(), hint.size(),
                    core::Tr("« %.*s » : reprend son nom, pose ses items encore présents dans "
                             "le cart, et leurs prix."),
                    static_cast<int>(view.resume_title.size()), view.resume_title.data());
      SkinTooltip(hint.data());
    }
  }
  ImGui::SameLine();
  if (SkinButton(core::Tr("Annuler"), art)) {
    state.open = false;
  }
}

void DrawConfirmation(VendingSetupWindowState& state, const VendingSetupView& view,
                      const SkinArt& art, VendingSetupAction& action) {
  if (!state.confirming) {
    return;
  }
  const std::string question = HighPriceQuestion(state, view);
  switch (DrawModalBox(kConfirmId, question.c_str(), core::Tr("Ouvrir le shop"),
                       core::Tr("Revoir les prix"), art)) {
    case ModalAnswer::kYes:
      state.confirming = false;
      action.open_shop = true;
      break;
    case ModalAnswer::kNo:
      state.confirming = false;
      break;
    case ModalAnswer::kWaiting:
      // Refermée sans réponse : rien ne s'ouvre.
      if (!ImGui::IsPopupOpen(kConfirmId)) {
        state.confirming = false;
      }
      break;
  }
}

}  // namespace

void OpenVendingSetupOn(VendingSetupWindowState& state, std::uint32_t preparation_seq) {
  state.open = true;
  state.preparation_seen = preparation_seq;
  state.lines.clear();
  state.stock_amounts.clear();
  state.ask_slot = 0;
  state.confirming = false;
}

int PlacedOnVendingSetup(const VendingSetupWindowState& state, std::uint16_t cart_slot) {
  const auto line = std::ranges::find_if(state.lines, [cart_slot](const VendingSetupLineState& on) {
    return on.cart_slot == cart_slot;
  });
  return line == state.lines.end() ? 0 : line->amount;
}

bool PlaceOnVendingSetup(VendingSetupWindowState& state, std::uint16_t cart_slot, int amount,
                         int stack, std::size_t permitted_lines) {
  if (cart_slot == 0 || amount <= 0 || stack <= 0) {
    return false;
  }
  // 🔴 UNE PILE, UNE LIGNE : le serveur ne refuse pas deux lignes de la même
  // case, et compterait deux fois la même pile. Poser encore s'ajoute.
  const auto line = std::ranges::find_if(state.lines, [cart_slot](const VendingSetupLineState& on) {
    return on.cart_slot == cart_slot;
  });
  if (line != state.lines.end()) {
    if (line->amount >= stack) {
      return false;
    }
    line->amount = std::min(line->amount + amount, stack);
    return true;
  }
  if (state.lines.size() >= permitted_lines) {
    return false;
  }
  VendingSetupLineState placed;
  placed.cart_slot = cart_slot;
  placed.amount = std::min(amount, stack);
  state.lines.push_back(placed);
  return true;
}

void TakeFromVendingSetup(VendingSetupWindowState& state, std::uint16_t cart_slot) {
  std::erase_if(state.lines,
                [cart_slot](const VendingSetupLineState& on) { return on.cart_slot == cart_slot; });
}

std::optional<std::int64_t> VendingLinePrice(const VendingSetupLineState& line) {
  const auto end = std::ranges::find(line.price_text, '\0');
  return ParseTradeAmount(std::string_view(
      line.price_text.data(), static_cast<std::size_t>(end - line.price_text.begin())));
}

std::vector<std::size_t> VendingHighPricedLines(const VendingSetupWindowState& state) {
  std::vector<std::size_t> ranks;
  for (std::size_t rank = 0; rank < state.lines.size(); ++rank) {
    if (VendingLinePrice(state.lines[rank]).value_or(0) >= kVendingHighPriceZeny) {
      ranks.push_back(rank);
    }
  }
  return ranks;
}

VendingSetupAction DrawVendingSetupWindow(VendingSetupWindowState& state,
                                          const VendingSetupView& view, const SkinArt& art,
                                          ItemLinkFacts link_facts) {
  VendingSetupAction action;
  if (!state.open) {
    return action;
  }
  const float scale = Scale();
  ImGui::SetNextWindowSize(ImVec2(kFirstWidthPixels * scale, kFirstHeightPixels * scale),
                           ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSizeConstraints(ImVec2(kLeastWidthPixels * scale, kLeastHeightPixels * scale),
                                      ImVec2(FLT_MAX, FLT_MAX));
  const std::string title = std::string(core::Tr("Ouvrir un shop")) + kVendingSetupWindowId;
  const bool body_shown =
      BeginSkinnedWindow(title.c_str(), &state.open, art, SkinnedTitle::kBar,
                         ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar |
                             ImGuiWindowFlags_NoScrollWithMouse);
  if (body_shown && state.open) {
    SetupGesture gesture;
    ImGui::BeginDisabled(view.awaiting);
    DrawName(state, view);
    ImGui::EndDisabled();
    ImGui::Separator();
    DrawStockHeader(state, view, art, action);

    // LES HAUTEURS : les lignes posées prennent ce qu'elles occupent, entre
    // trois lignes et ce que le stock laisse ; le SURPLUS VA AU STOCK.
    const float line_height = ImGui::GetTextLineHeightWithSpacing();
    const float panes = ImGui::GetContentRegionAvail().y - state.footer_pixels - line_height;
    const float stock_least = kStockLeastLines * line_height;
    const float lines_least = kPlacedLeastLines * line_height;
    const float lines_wanted = std::clamp(state.lines_content_pixels, lines_least,
                                          std::max(lines_least, panes - stock_least));
    const float stock_height = std::max(panes - lines_wanted, stock_least);

    ImGui::BeginChild("##vending_stock", ImVec2(-1.0F, stock_height), ImGuiChildFlags_Borders);
    if (view.stock.empty()) {
      ImGui::TextDisabled("%s", core::Tr("Aucun item dans le cart."));
    } else if (state.grid) {
      DrawStockGrid(state, view, gesture, action);
    } else {
      DrawStockList(state, view, art, gesture, action);
    }
    PaintWindowScrollbar(art);
    ImGui::EndChild();
    // RETIRER PAR GLISSER : une ligne posée lâchée sur le stock y revient.
    if (ImGui::BeginDragDropTarget()) {
      if (const ImGuiPayload* dropped = ImGui::AcceptDragDropPayload(kLinePayload)) {
        gesture.take_slot = *static_cast<const std::uint16_t*>(dropped->Data);
      }
      ImGui::EndDragDropTarget();
    }

    ImGui::TextUnformatted(core::Tr("Dans le shop"));
    ImGui::SameLine();
    ImGui::TextDisabled("(%zu/%zu)", state.lines.size(), view.permitted_lines);
    // LA ZONE DE DÉPÔT, visible seulement pendant un glisser depuis le stock.
    const ImGuiPayload* dragged = ImGui::GetDragDropPayload();
    if (dragged != nullptr && dragged->IsDataType(kStockPayload)) {
      const bool full = state.lines.size() >= view.permitted_lines;
      ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(
                                                 full ? ImGuiCol_FrameBg : ImGuiCol_HeaderHovered));
      ImGui::Button(full ? core::Tr("Toutes les lignes sont prises")
                         : core::Tr("Déposer ici pour mettre en vente"),
                    ImVec2(-1.0F, ImGui::GetFrameHeight()));
      ImGui::PopStyleColor();
      if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* dropped = ImGui::AcceptDragDropPayload(kStockPayload)) {
          gesture.place_slot = *static_cast<const std::uint16_t*>(dropped->Data);
        }
        ImGui::EndDragDropTarget();
      }
    }

    const float lines_height =
        std::max(ImGui::GetContentRegionAvail().y - state.footer_pixels, lines_least);
    ImGui::BeginChild("##vending_lines", ImVec2(-1.0F, lines_height));
    ImGui::BeginDisabled(view.awaiting);
    if (state.lines.empty()) {
      ImGui::TextDisabled("%s", state.grid
                                    ? core::Tr("Glisse un item ci-dessus, ou double-clique-le.")
                                    : core::Tr("Choisis un item ci-dessus et clique « Poser »."));
    } else {
      DrawLines(state, view, art, gesture, action);
    }
    ImGui::EndDisabled();
    state.lines_content_pixels = ImGui::GetCursorPosY() + ImGui::GetStyle().WindowPadding.y;
    PaintWindowScrollbar(art);
    ImGui::EndChild();

    const float footer_top = ImGui::GetCursorPosY();
    ApplyGesture(state, view, gesture);
    DrawAskPopup(state, view, art);
    DrawFooter(state, view, art, action);
    DrawConfirmation(state, view, art, action);
    state.footer_pixels = ImGui::GetCursorPosY() - footer_top;
    if (const ChatLinkAsk chosen = DrawTradeItemMenu(kItemMenuId, state.item_menu, link_facts);
        chosen.gesture != ChatLinkGesture::kNothing) {
      action.describe_slot = static_cast<std::uint16_t>(chosen.token);
      action.item_gesture = chosen.gesture;
    }
  }
  EndSkinnedWindow();
  if (!state.open) {
    action.cancel = true;
    action.open_shop = false;
    action.resume = false;
  }
  return action;
}

}  // namespace ro::ui
