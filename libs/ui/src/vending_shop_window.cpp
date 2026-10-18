#include "ro/ui/vending_shop_window.h"

#include <cfloat>
#include <cstdio>
#include <string>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/ui/inventory_window.h"
#include "ro/ui/modal_box.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"

#include "ui_scale.h"
#include "ui_text.h"

namespace ro::ui {
namespace {

// ── LES MESURES DE LA FENÊTRE ───────────────────────────────────────────────
// Les deux panneaux s'ajustent à leur contenu, entre ces bornes.
constexpr float kLeastWidthPixels = 260.0F;
constexpr float kMostWidthPixels = 900.0F;
constexpr float kAmountColumnPixels = 45.0F;
constexpr float kPriceColumnPixels = 90.0F;
// Les deux colonnes que le journal ajoute : l'heure et l'acheteur.
constexpr float kTimeColumnPixels = 100.0F;
constexpr float kBuyerColumnPixels = 90.0F;

constexpr const char* kCloseConfirmId = "##vending_shop_close";
constexpr const char* kShopItemMenuId = "##vending_shop_item_menu";
constexpr const char* kLogItemMenuId = "##vending_log_item_menu";

void Constrain() {
  const float scale = Scale();
  ImGui::SetNextWindowSizeConstraints(ImVec2(kLeastWidthPixels * scale, 0.0F),
                                      ImVec2(kMostWidthPixels * scale, FLT_MAX));
}

void DrawShopLines(VendingShopWindowState& state, const VendingShopView& view,
                   VendingShopAction& action) {
  const float scale = Scale();
  if (!ImGui::BeginTable("##vending_shop_lines", 4,
                         ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg)) {
    return;
  }
  ImGui::TableSetupColumn(core::Tr("Item"), ImGuiTableColumnFlags_WidthFixed);
  ImGui::TableSetupColumn(core::Tr("Reste"), ImGuiTableColumnFlags_WidthFixed,
                          kAmountColumnPixels * scale);
  ImGui::TableSetupColumn(core::Tr("Prix"), ImGuiTableColumnFlags_WidthFixed,
                          kPriceColumnPixels * scale);
  ImGui::TableSetupColumn(core::Tr("Total"), ImGuiTableColumnFlags_WidthFixed,
                          kPriceColumnPixels * scale);
  ImGui::TableHeadersRow();
  for (const VendingShopLineView& line : view.lines) {
    ImGui::PushID(line.cart_slot);
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    const TradeItemGesture gesture = DrawTradeItem(line.item);
    if (gesture.hovered) {
      action.hovered_slot = line.cart_slot;
    }
    if (NoteTradeItemLink(gesture, line.item, line.cart_slot, state.item_menu)) {
      action.describe_slot = line.cart_slot;
      action.item_gesture = ChatLinkGesture::kRepost;
    }
    ImGui::TableNextColumn();
    ImGui::AlignTextToFramePadding();
    ImGui::Text("%d", line.amount);
    ImGui::TableNextColumn();
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(TradeAmountText(line.price, kZenySuffix).c_str());
    ImGui::TableNextColumn();
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(TradeAmountText(line.price * line.amount, kZenySuffix).c_str());
    ImGui::PopID();
  }
  ImGui::EndTable();
}

}  // namespace

VendingShopAction DrawVendingShopWindow(VendingShopWindowState& state, const VendingShopView& view,
                                        const SkinArt& art, ItemLinkFacts link_facts) {
  VendingShopAction action;
  // 🔴 UNE FENÊTRE BAISSÉE N'EST PAS UNE ÉCHOPPE FERMÉE : la croix ou Échap ne
  // font que POSER LA QUESTION. La fenêtre se relève, et c'est la réponse qui
  // décide.
  const bool asked_to_close = !state.open;
  state.open = true;
  Constrain();
  const std::string title = std::string(core::Tr("Mon shop")) + kVendingShopWindowId;
  bool keep = true;
  const bool body_shown =
      BeginSkinnedWindow(title.c_str(), &keep, art, SkinnedTitle::kBar,
                         ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse);
  if (body_shown) {
    if (!view.title.empty()) {
      ColoredText(kTradeBlackText, view.title);
      ImGui::Separator();
    }
    if (view.lines.empty()) {
      ImGui::TextDisabled("%s", core::Tr("Tout est vendu."));
    } else {
      DrawShopLines(state, view, action);
    }
    ImGui::Separator();
    ImGui::Text(core::Tr("Encaissé : %s"), TradeAmountText(view.earned, kZenySuffix).c_str());
    const bool clicked = SkinButton(core::Tr("Fermer le shop"), art);
    if (ImGui::IsItemHovered()) {
      SkinTooltip(core::Tr("Met fin à la vente et rend au cart les items invendus."));
    }
    if ((clicked || !keep || asked_to_close) && !state.confirming) {
      state.confirming = true;
      AskModal(kCloseConfirmId);
    }
    if (state.confirming) {
      switch (DrawModalBox(kCloseConfirmId, core::Tr("Mettre fin au shop ?"), core::Tr("Oui"),
                           core::Tr("Non"), art)) {
        case ModalAnswer::kYes:
          state.confirming = false;
          action.close_shop = true;
          break;
        case ModalAnswer::kNo:
          state.confirming = false;
          break;
        case ModalAnswer::kWaiting:
          // Refermée sans réponse : l'échoppe reste ouverte.
          if (!ImGui::IsPopupOpen(kCloseConfirmId)) {
            state.confirming = false;
          }
          break;
      }
    }
    if (const ChatLinkAsk chosen = DrawTradeItemMenu(kShopItemMenuId, state.item_menu, link_facts);
        chosen.gesture != ChatLinkGesture::kNothing) {
      action.describe_slot = static_cast<std::uint16_t>(chosen.token);
      action.item_gesture = chosen.gesture;
    }
  }
  EndSkinnedWindow();
  return action;
}

VendingLogAction DrawVendingLogWindow(VendingLogWindowState& state, const VendingLogView& view,
                                      const SkinArt& art, ItemLinkFacts link_facts) {
  VendingLogAction action;
  if (!state.open) {
    return action;
  }
  const float scale = Scale();
  Constrain();
  const std::string title = std::string(core::Tr("Historique des ventes")) + kVendingLogWindowId;
  const bool body_shown =
      BeginSkinnedWindow(title.c_str(), &state.open, art, SkinnedTitle::kBar,
                         ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse);
  if (body_shown && state.open) {
    if (view.sales.empty()) {
      ImGui::TextDisabled("%s", core::Tr("Aucune vente."));
    } else if (ImGui::BeginTable("##vending_log_lines", 6,
                                 ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg)) {
      ImGui::TableSetupColumn(core::Tr("Heure"), ImGuiTableColumnFlags_WidthFixed,
                              kTimeColumnPixels * scale);
      ImGui::TableSetupColumn(core::Tr("Item"), ImGuiTableColumnFlags_WidthFixed);
      ImGui::TableSetupColumn(core::Tr("Qté"), ImGuiTableColumnFlags_WidthFixed,
                              kAmountColumnPixels * scale);
      ImGui::TableSetupColumn(core::Tr("Prix"), ImGuiTableColumnFlags_WidthFixed,
                              kPriceColumnPixels * scale);
      ImGui::TableSetupColumn(core::Tr("Montant"), ImGuiTableColumnFlags_WidthFixed,
                              kPriceColumnPixels * scale);
      ImGui::TableSetupColumn(core::Tr("Acheteur"), ImGuiTableColumnFlags_WidthFixed,
                              kBuyerColumnPixels * scale);
      ImGui::TableHeadersRow();
      for (const VendingLogLineView& sale : view.sales) {
        ImGui::PushID(static_cast<int>(sale.rank));
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        Unformatted(sale.sold_at);
        ImGui::TableNextColumn();
        const TradeItemGesture gesture = DrawTradeItem(sale.item);
        if (gesture.hovered) {
          action.hovered_rank = sale.rank + 1;
        }
        if (NoteTradeItemLink(gesture, sale.item, static_cast<std::uint32_t>(sale.rank + 1),
                              state.item_menu)) {
          action.describe_rank = sale.rank + 1;
          action.item_gesture = ChatLinkGesture::kRepost;
        }
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::Text("%d", sale.amount);
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        // Le prix UNITAIRE se déduit : le rapport ne dit que le montant.
        if (sale.amount > 0) {
          ImGui::TextUnformatted(TradeAmountText(sale.zeny / sale.amount, kZenySuffix).c_str());
        } else {
          ImGui::TextDisabled("-");
        }
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(TradeAmountText(sale.zeny, kZenySuffix).c_str());
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        // ⚠ LE NOM DE L'ACHETEUR N'EST PAS CONNU : le rapport ne porte que son
        // identifiant de personnage.
        ImGui::TextDisabled(core::Tr("CID %u"), sale.buyer_char_id);
        ImGui::PopID();
      }
      ImGui::EndTable();
    }
    ImGui::Separator();
    ImGui::Text(core::Tr("Total des ventes : %s"),
                TradeAmountText(view.total, kZenySuffix).c_str());
    if (SkinButton(core::Tr("Fermer"), art)) {
      state.open = false;
    }
    if (const ChatLinkAsk chosen = DrawTradeItemMenu(kLogItemMenuId, state.item_menu, link_facts);
        chosen.gesture != ChatLinkGesture::kNothing) {
      action.describe_rank = chosen.token;
      action.item_gesture = chosen.gesture;
    }
  }
  EndSkinnedWindow();
  if (!state.open) {
    action.close = true;
  }
  return action;
}

}  // namespace ro::ui
