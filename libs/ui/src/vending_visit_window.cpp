#include "ro/ui/vending_visit_window.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cstdio>
#include <string>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/ui/inventory_window.h"
#include "ro/ui/modal_box.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"

#include "trade_gestures.h"
#include "ui_scale.h"
#include "ui_text.h"

namespace ro::ui {
namespace {

// ── LES MESURES DE LA FENÊTRE ───────────────────────────────────────────────
constexpr float kFirstWidthPixels = 560.0F;
constexpr float kFirstHeightPixels = 460.0F;
constexpr float kLeastWidthPixels = 380.0F;
constexpr float kLeastHeightPixels = 240.0F;
constexpr float kStockColumnPixels = 45.0F;
constexpr float kAmountColumnPixels = 60.0F;
constexpr float kAmountFieldPixels = 56.0F;
// Les rangs d'identité du panier, loin de ceux des offres.
constexpr int kBasketIdBase = 1000;

constexpr const char* kConfirmId = "##vending_visit_confirm";
constexpr const char* kItemMenuId = "##vending_visit_item_menu";

const VendingOfferView* OfferAt(std::span<const VendingOfferView> offers, std::uint16_t shop_slot) {
  const auto found = std::ranges::find_if(
      offers, [shop_slot](const VendingOfferView& offer) { return offer.shop_slot == shop_slot; });
  return found == offers.end() ? nullptr : &*found;
}

int& OfferAmountFor(VendingVisitWindowState& state, std::uint16_t shop_slot, int stock) {
  auto found = std::ranges::find_if(
      state.offer_amounts, [shop_slot](const auto& kept) { return kept.first == shop_slot; });
  if (found == state.offer_amounts.end()) {
    state.offer_amounts.emplace_back(shop_slot, 1);
    found = state.offer_amounts.end() - 1;
  }
  found->second = std::clamp(found->second, 1, std::max(stock, 1));
  return found->second;
}

// Un achat part, ou attend sa confirmation s'il coûte assez pour mériter
// qu'on relise.
void AskPurchase(VendingVisitWindowState& state, const VendingVisitView& view,
                 std::vector<VendingBasketEntry> lines, bool immediate,
                 VendingVisitAction& action) {
  if (VendingBasketTotal(lines, view.offers) >= kVendingConfirmTotalZeny) {
    state.pending = std::move(lines);
    state.pending_immediate = immediate;
    // ⚠ LA BOÎTE NE S'OUVRE PAS ICI : on peut être dans une ligne de table, et
    // elle s'ouvrirait sous une autre identité que celle qui la dessine.
    state.pending_asked = false;
    return;
  }
  action.buy = std::move(lines);
  action.immediate = immediate;
}

void DrawOffers(VendingVisitWindowState& state, const VendingVisitView& view, const SkinArt& art,
                VendingVisitAction& action) {
  const float scale = Scale();
  float price_width = ImGui::CalcTextSize(core::Tr("Prix")).x;
  for (const VendingOfferView& offer : view.offers) {
    price_width = std::max(price_width, TradePriceWidth(offer.price, offer.price, kZenySuffix));
  }
  if (!ImGui::BeginTable("##vending_offers", 5,
                         ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg)) {
    return;
  }
  ImGui::TableSetupColumn(core::Tr("Item"), ImGuiTableColumnFlags_WidthStretch);
  ImGui::TableSetupColumn(core::Tr("Stock"), ImGuiTableColumnFlags_WidthFixed,
                          kStockColumnPixels * scale);
  ImGui::TableSetupColumn(core::Tr("Prix"), ImGuiTableColumnFlags_WidthFixed, price_width);
  ImGui::TableSetupColumn(core::Tr("Qté"), ImGuiTableColumnFlags_WidthFixed,
                          kAmountColumnPixels * scale);
  ImGui::TableSetupColumn("##add", ImGuiTableColumnFlags_WidthFixed,
                          SkinSmallButtonWidth(core::Tr("Ajouter"), art));
  ImGui::TableHeadersRow();
  const bool ctrl = ImGui::GetIO().KeyCtrl;
  for (const VendingOfferView& offer : view.offers) {
    ImGui::PushID(offer.shop_slot);
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    NoteItemBySlot(DrawTradeItem(offer.item), offer.item, offer.shop_slot, state.item_menu, action);
    ImGui::TableNextColumn();
    ImGui::AlignTextToFramePadding();
    ImGui::Text("%d", offer.stock);
    ImGui::TableNextColumn();
    ImGui::AlignTextToFramePadding();
    const bool affordable = view.zeny == kTradeNoPurse || offer.price <= view.zeny;
    DrawTradePrice(offer.price, offer.price, affordable ? kTradeBlackText : kTradeUnaffordableText,
                   kZenySuffix);
    ImGui::TableNextColumn();
    int& amount = OfferAmountFor(state, offer.shop_slot, offer.stock);
    ImGui::SetNextItemWidth(kAmountFieldPixels * scale);
    if (ImGui::InputInt("##q", &amount, 0, 0)) {
      amount = std::clamp(amount, 1, std::max(offer.stock, 1));
    }
    ImGui::TableNextColumn();
    // 🔴 CTRL ENFONCÉ, LE BOUTON ACHÈTE TOUTE LA LIGNE — et c'est alors qu'il se
    // grise si elle coûte plus que la bourse.
    const bool too_dear =
        ctrl && view.zeny != kTradeNoPurse && offer.price * offer.stock > view.zeny;
    ImGui::BeginDisabled(view.awaiting || offer.stock <= 0 || too_dear);
    if (SkinSmallButton(core::Tr("Ajouter"), art)) {
      if (ctrl) {
        AskPurchase(state, view, {{.shop_slot = offer.shop_slot, .amount = offer.stock}}, true,
                    action);
      } else {
        AddToVendingBasket(state.basket, offer.shop_slot, amount, offer.stock);
      }
    }
    ImGui::EndDisabled();
    if (too_dear) {
      SkinTooltip(core::Tr("Toute la ligne coûte plus que vos zeny."));
    }
    ImGui::PopID();
  }
  ImGui::EndTable();
}

void DrawBasket(VendingVisitWindowState& state, const VendingVisitView& view, const SkinArt& art,
                VendingVisitAction& action) {
  const float scale = Scale();
  if (state.basket.empty()) {
    ImGui::TextDisabled("%s", core::Tr("Panier vide."));
    return;
  }
  float total_width = ImGui::CalcTextSize(core::Tr("Total")).x;
  for (const VendingBasketEntry& entry : state.basket) {
    const VendingOfferView* offer = OfferAt(view.offers, entry.shop_slot);
    const std::int64_t price = offer != nullptr ? offer->price : 0;
    total_width = std::max(total_width, TradePriceWidth(0, price * entry.amount, kZenySuffix));
  }
  if (!ImGui::BeginTable("##vending_basket", 4,
                         ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg)) {
    return;
  }
  ImGui::TableSetupColumn(core::Tr("Panier"), ImGuiTableColumnFlags_WidthStretch);
  ImGui::TableSetupColumn(core::Tr("Qté"), ImGuiTableColumnFlags_WidthFixed,
                          kStockColumnPixels * scale);
  ImGui::TableSetupColumn(core::Tr("Total"), ImGuiTableColumnFlags_WidthFixed, total_width);
  ImGui::TableSetupColumn("##del", ImGuiTableColumnFlags_WidthFixed,
                          SkinSmallButtonWidth(core::Tr("Retirer"), art));
  ImGui::TableHeadersRow();
  std::size_t removed = state.basket.size();
  for (std::size_t rank = 0; rank < state.basket.size(); ++rank) {
    const VendingBasketEntry& entry = state.basket[rank];
    const VendingOfferView* offer = OfferAt(view.offers, entry.shop_slot);
    ImGui::PushID(kBasketIdBase + static_cast<int>(rank));
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    if (offer != nullptr) {
      NoteItemBySlot(DrawTradeItem(offer->item), offer->item, entry.shop_slot, state.item_menu,
                     action);
    } else {
      ImGui::AlignTextToFramePadding();
      ImGui::TextDisabled("%s", core::Tr("(n'est plus en vente)"));
    }
    ImGui::TableNextColumn();
    ImGui::AlignTextToFramePadding();
    ImGui::Text("%d", entry.amount);
    ImGui::TableNextColumn();
    ImGui::AlignTextToFramePadding();
    const std::int64_t price = offer != nullptr ? offer->price : 0;
    ImGui::TextUnformatted(TradeAmountText(price * entry.amount, kZenySuffix).c_str());
    ImGui::TableNextColumn();
    ImGui::BeginDisabled(view.awaiting);
    if (SkinSmallButton(core::Tr("Retirer"), art)) {
      removed = rank;
    }
    ImGui::EndDisabled();
    ImGui::PopID();
  }
  ImGui::EndTable();
  if (removed < state.basket.size()) {
    state.basket.erase(state.basket.begin() + static_cast<std::ptrdiff_t>(removed));
  }
}

void DrawConfirmation(VendingVisitWindowState& state, const VendingVisitView& view,
                      const SkinArt& art, VendingVisitAction& action) {
  if (state.pending.empty()) {
    return;
  }
  if (!state.pending_asked) {
    AskModal(kConfirmId);
    state.pending_asked = true;
  }
  int count = 0;
  for (const VendingBasketEntry& entry : state.pending) {
    count += entry.amount;
  }
  const std::int64_t total = VendingBasketTotal(state.pending, view.offers);
  std::array<char, 512> question{};
  std::snprintf(question.data(), question.size(),
                core::Tr("Acheter %d item(s) pour %s ?\n(%s zeny)\nC'est le prix que le "
                         "serveur facturera."),
                count, TradeAmountText(total, kZenySuffix).c_str(),
                SpelledTradeAmount(total).c_str());
  switch (
      DrawModalBox(kConfirmId, question.data(), core::Tr("Acheter"), core::Tr("Annuler"), art)) {
    case ModalAnswer::kYes:
      action.buy = std::move(state.pending);
      action.immediate = state.pending_immediate;
      state.pending.clear();
      break;
    case ModalAnswer::kNo:
      state.pending.clear();
      break;
    case ModalAnswer::kWaiting:
      // Refermée sans réponse : l'achat ne part pas.
      if (!ImGui::IsPopupOpen(kConfirmId)) {
        state.pending.clear();
      }
      break;
  }
}

}  // namespace

void OpenVendingVisitOn(VendingVisitWindowState& state, std::uint32_t opening_seq) {
  state.open = true;
  state.opening_seen = opening_seq;
  state.basket.clear();
  state.offer_amounts.clear();
  state.pending.clear();
  state.pending_immediate = false;
  state.pending_asked = false;
}

bool AddToVendingBasket(std::vector<VendingBasketEntry>& basket, std::uint16_t shop_slot,
                        int amount, int stock) {
  if (shop_slot == 0 || amount <= 0 || stock <= 0) {
    return false;
  }
  // UNE OFFRE, UNE LIGNE : le serveur cumule de toute façon, et fusionnées
  // elles ne comptent qu'une ligne sur les douze permises.
  const auto same = std::ranges::find_if(
      basket, [shop_slot](const VendingBasketEntry& kept) { return kept.shop_slot == shop_slot; });
  if (same == basket.end()) {
    basket.push_back({.shop_slot = shop_slot, .amount = std::min(amount, stock)});
    return true;
  }
  if (same->amount >= stock) {
    return false;
  }
  same->amount = std::min(same->amount + amount, stock);
  return true;
}

std::int64_t VendingBasketTotal(std::span<const VendingBasketEntry> basket,
                                std::span<const VendingOfferView> offers) {
  std::int64_t total = 0;
  for (const VendingBasketEntry& entry : basket) {
    if (const VendingOfferView* offer = OfferAt(offers, entry.shop_slot)) {
      total += offer->price * entry.amount;
    }
  }
  return total;
}

void FitVendingBasket(std::vector<VendingBasketEntry>& basket,
                      std::span<const VendingOfferView> offers) {
  std::erase_if(basket, [offers](VendingBasketEntry& entry) {
    const VendingOfferView* offer = OfferAt(offers, entry.shop_slot);
    if (offer == nullptr || offer->stock <= 0) {
      return true;
    }
    entry.amount = std::min(entry.amount, offer->stock);
    return false;
  });
}

VendingVisitAction DrawVendingVisitWindow(VendingVisitWindowState& state,
                                          const VendingVisitView& view, const SkinArt& art,
                                          ItemLinkFacts link_facts) {
  VendingVisitAction action;
  if (!state.open) {
    return action;
  }
  const float scale = Scale();
  ImGui::SetNextWindowSize(ImVec2(kFirstWidthPixels * scale, kFirstHeightPixels * scale),
                           ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSizeConstraints(ImVec2(kLeastWidthPixels * scale, kLeastHeightPixels * scale),
                                      ImVec2(FLT_MAX, FLT_MAX));
  // ⚠ `###` : le nom du vendeur peut arriver une image plus tard, et la fenêtre
  // ne doit pas changer d'identité pour autant.
  std::string title;
  if (view.vendor_name.empty()) {
    title = core::Tr("Shop");
  } else {
    std::array<char, 128> named{};
    std::snprintf(named.data(), named.size(), core::Tr("Shop de %.*s"),
                  static_cast<int>(view.vendor_name.size()), view.vendor_name.data());
    title = named.data();
  }
  title += kVendingVisitWindowId;
  const bool body_shown = BeginSkinnedWindow(title.c_str(), &state.open, art, SkinnedTitle::kBar,
                                             ImGuiWindowFlags_NoCollapse);
  if (body_shown && state.open) {
    if (!view.title.empty()) {
      ColoredText(kTradeBlackText, view.title);
    }
    if (!view.outcome.empty()) {
      ColoredText(kTradeRefusedText, view.outcome);
    }
    if (view.offers.empty()) {
      ImGui::TextDisabled("%s", core::Tr("Le shop est vide."));
    } else {
      DrawOffers(state, view, art, action);
      DrawTradeHint(
          core::Tr("Ctrl+clic sur « Ajouter » : achète toute la ligne "
                   "immédiatement."));
    }
    ImGui::Separator();
    DrawBasket(state, view, art, action);
    ImGui::Text(core::Tr("Total : %s"), TradeAmountText(view.basket_total, kZenySuffix).c_str());
    if (view.zeny != kTradeNoPurse) {
      ImGui::SameLine();
      ImGui::TextDisabled(core::Tr("Zeny : %s"), TradeAmountText(view.zeny, kZenySuffix).c_str());
    }
    // 🔴 « ACHETER » SE RETIENT tant que le serveur refuserait, ou ignorerait
    // l'achat sans un mot. La première raison s'écrit ici, toutes au survol.
    if (!state.basket.empty() && !view.blockers.empty()) {
      ColoredText(kTradeRefusedText, view.blockers[0]);
    } else if (view.awaiting) {
      ImGui::TextDisabled("%s", core::Tr("Achat envoyé : en attente de la liste..."));
    }
    const bool blocked = state.basket.empty() || !view.blockers.empty() || view.awaiting;
    ImGui::BeginDisabled(blocked);
    if (SkinButton(core::Tr("Acheter"), art)) {
      AskPurchase(state, view, state.basket, false, action);
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
    ImGui::SameLine();
    if (SkinButton(core::Tr("Fermer"), art)) {
      state.open = false;
    }
    DrawConfirmation(state, view, art, action);
    if (const ChatLinkAsk chosen = DrawTradeItemMenu(kItemMenuId, state.item_menu, link_facts);
        chosen.gesture != ChatLinkGesture::kNothing) {
      action.describe_slot = static_cast<std::uint16_t>(chosen.token);
      action.item_gesture = chosen.gesture;
    }
  }
  EndSkinnedWindow();
  if (!state.open) {
    action.close = true;
    action.buy.clear();
  }
  return action;
}

}  // namespace ro::ui
