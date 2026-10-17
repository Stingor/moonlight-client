#include "ro/ui/buying_store_sell_window.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <string>

#include "ro/core/i18n.h"
#include "ro/ui/modal_box.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"

#include "ui_scale.h"

namespace ro::ui {
namespace {

// ── LES MESURES (vente à un buying store), passées à l'échelle ──────────────
constexpr float kFirstWidthPixels = 520.0F;
constexpr float kFirstHeightPixels = 500.0F;
// Les deux listes du haut ont une hauteur FIXE et défilent : c'est « À vendre »
// qui profite de l'agrandissement.
constexpr float kPaneRows = 5.0F;
// Le pied : total, fonds, verdict, puis les boutons.
constexpr float kFooterTextRows = 3.0F;
constexpr float kWantedColumnPixels = 52.0F;
constexpr float kPriceColumnPixels = 110.0F;
constexpr float kStockColumnPixels = 46.0F;
constexpr float kAmountColumnPixels = 56.0F;
constexpr float kAmountFieldPixels = 52.0F;
// Les identités des trois tables, loin les unes des autres.
constexpr int kOfferIdBase = 2000;
constexpr int kCartIdBase = 3000;

constexpr const char* kSaleConfirmId = "##buying_store_sale_confirm";
constexpr const char* kItemMenuId = "##buying_store_sell_item_menu";

const BuyingStoreWantedView* WantedOf(std::span<const BuyingStoreWantedView> wanted,
                                      std::uint32_t item_id) {
  const auto found = std::ranges::find_if(wanted, [item_id](const BuyingStoreWantedView& line) {
    return line.item.item_id == item_id;
  });
  return found == wanted.end() ? nullptr : &*found;
}

const BuyingStoreOfferView* OfferAt(const BuyingStoreSellView& view, std::uint16_t client_slot) {
  const auto found =
      std::ranges::find(view.offers, client_slot, &BuyingStoreOfferView::client_slot);
  return found == view.offers.end() ? nullptr : &*found;
}

int& TypedAmount(BuyingStoreSellState& state, std::uint16_t client_slot, int fallback) {
  const auto found =
      std::ranges::find(state.typed, client_slot, &BuyingStoreTypedAmount::client_slot);
  if (found != state.typed.end()) {
    return found->amount;
  }
  state.typed.push_back(BuyingStoreTypedAmount{.client_slot = client_slot, .amount = fallback});
  return state.typed.back().amount;
}

std::int64_t UnitPrice(const BuyingStoreSellView& view, const BuyingStoreSaleCartLine& line) {
  const BuyingStoreWantedView* wanted = WantedOf(view.wanted, line.item_id);
  return wanted == nullptr ? 0 : wanted->price;
}

// Le geste d'une ligne. Le jeton du menu est la case du sac, nulle pour un item
// recherché : c'est elle qui dit le raffinement.
void NoteItem(const TradeItemGesture& gesture, const TradeItemView& item, std::uint32_t item_id,
              std::uint16_t slot, ItemLinkMenu& menu, BuyingStoreSellAction& action) {
  if (gesture.hovered) {
    action.hovered_id = item_id;
    action.hovered_slot = slot;
  }
  TradeItemView aimed = item;
  aimed.item_id = item_id;
  if (NoteTradeItemLink(gesture, aimed, slot, menu)) {
    action.describe_id = item_id;
    action.describe_slot = slot;
    action.item_gesture = ChatLinkGesture::kRepost;
  }
}

void DrawWanted(const BuyingStoreSellView& view, const SkinArt& art, float height,
                ItemLinkMenu& menu, BuyingStoreSellAction& action) {
  const float scale = Scale();
  ImGui::TextUnformatted(core::Tr("Items recherchés"));
  ImGui::BeginChild("##buying_store_wanted_pane", ImVec2(-1.0F, height), ImGuiChildFlags_Borders);
  if (view.wanted.empty()) {
    ImGui::TextDisabled("%s", core::Tr("Ce buying store ne demande plus rien."));
  } else if (ImGui::BeginTable("##buying_store_wanted", 3,
                               ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg)) {
    ImGui::TableSetupColumn(core::Tr("Item"), ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn(core::Tr("Voulu"), ImGuiTableColumnFlags_WidthFixed,
                            kWantedColumnPixels * scale);
    ImGui::TableSetupColumn(core::Tr("Prix"), ImGuiTableColumnFlags_WidthFixed,
                            kPriceColumnPixels * scale);
    ImGui::TableHeadersRow();
    for (std::size_t rank = 0; rank < view.wanted.size(); ++rank) {
      const BuyingStoreWantedView& line = view.wanted[rank];
      ImGui::PushID(static_cast<int>(rank));
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      NoteItem(DrawTradeItem(line.item), line.item, line.item.item_id, 0, menu, action);
      ImGui::TableNextColumn();
      ImGui::AlignTextToFramePadding();
      ImGui::TextColored(kTradeBlackText, "%d", line.wanted);
      ImGui::TableNextColumn();
      ImGui::AlignTextToFramePadding();
      ImGui::TextColored(kTradeBlackText, "%s", TradeAmountText(line.price, "z").c_str());
      ImGui::PopID();
    }
    ImGui::EndTable();
  }
  PaintWindowScrollbar(art);
  ImGui::EndChild();
}

void DrawOffers(BuyingStoreSellState& state, const BuyingStoreSellView& view, const SkinArt& art,
                float height, BuyingStoreSellAction& action) {
  const float scale = Scale();
  ImGui::TextUnformatted(core::Tr("Mes items"));
  ImGui::BeginChild("##buying_store_offer_pane", ImVec2(-1.0F, height), ImGuiChildFlags_Borders);
  if (view.offers.empty()) {
    ImGui::TextDisabled("%s", core::Tr("Rien à proposer."));
  } else if (ImGui::BeginTable("##buying_store_offers", 4,
                               ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg)) {
    ImGui::TableSetupColumn(core::Tr("Item"), ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn(core::Tr("Stock"), ImGuiTableColumnFlags_WidthFixed,
                            kStockColumnPixels * scale);
    ImGui::TableSetupColumn(core::Tr("Qté"), ImGuiTableColumnFlags_WidthFixed,
                            kAmountColumnPixels * scale);
    ImGui::TableSetupColumn("##add", ImGuiTableColumnFlags_WidthFixed,
                            SkinSmallButtonWidth(core::Tr("Ajouter"), art));
    ImGui::TableHeadersRow();
    for (std::size_t rank = 0; rank < view.offers.size(); ++rank) {
      const BuyingStoreOfferView& offer = view.offers[rank];
      const int addable = BuyingStoreAddable(view.wanted, state.cart, offer);
      ImGui::PushID(kOfferIdBase + static_cast<int>(rank));
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      NoteItem(DrawTradeItem(offer.item), offer.item, offer.item.item_id, offer.client_slot,
               state.item_menu, action);
      // UNE PILE REFUSÉE RESTE, GRISÉE, AVEC SA RAISON : absente, elle passerait
      // pour un item que l'acheteur ne veut pas.
      if (!offer.barred.empty()) {
        ImGui::SameLine();
        ImGui::TextDisabled("(%.*s)", static_cast<int>(offer.barred.size()), offer.barred.data());
      }
      ImGui::TableNextColumn();
      ImGui::AlignTextToFramePadding();
      ImGui::TextColored(kTradeBlackText, "%d", offer.owned);
      ImGui::TableNextColumn();
      const bool sellable = offer.barred.empty() && addable > 0 && !view.awaiting;
      if (sellable) {
        int& typed = TypedAmount(state, offer.client_slot, addable);
        ImGui::SetNextItemWidth(kAmountFieldPixels * scale);
        ImGui::InputInt("##amount", &typed, 0, 0);
        typed = std::clamp(typed, 1, addable);
      } else {
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("-");
      }
      ImGui::TableNextColumn();
      if (!sellable) {
        ImGui::BeginDisabled();
      }
      if (SkinSmallButton(core::Tr("Ajouter"), art)) {
        // Ctrl + clic : tout ce que l'acheteur peut encore prendre.
        const int amount =
            ImGui::GetIO().KeyCtrl ? addable : TypedAmount(state, offer.client_slot, addable);
        AddToBuyingStoreSale(state, view.wanted, offer, amount);
      }
      if (!sellable) {
        ImGui::EndDisabled();
      }
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        if (!offer.barred.empty()) {
          const std::string barred(offer.barred);
          SkinTooltip(barred.c_str());
        } else if (addable <= 0) {
          SkinTooltip(
              core::Tr("L'acheteur n'en veut pas davantage, ou la pile est déjà en vente."));
        }
      }
      ImGui::PopID();
    }
    ImGui::EndTable();
  }
  PaintWindowScrollbar(art);
  ImGui::EndChild();
  ImGui::TextDisabled("%s", core::Tr("Ctrl+clic sur « Ajouter » : tout ce que l'acheteur peut "
                                     "encore prendre."));
}

void DrawCart(BuyingStoreSellState& state, const BuyingStoreSellView& view, const SkinArt& art,
              float footer, BuyingStoreSellAction& action) {
  const float scale = Scale();
  ImGui::TextUnformatted(core::Tr("À vendre"));
  ImGui::BeginChild("##buying_store_cart_pane", ImVec2(-1.0F, -footer), ImGuiChildFlags_Borders);
  std::size_t removed = state.cart.size();
  if (state.cart.empty()) {
    ImGui::TextDisabled("%s", core::Tr("Rien en vente."));
  } else if (ImGui::BeginTable("##buying_store_cart", 4,
                               ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg)) {
    ImGui::TableSetupColumn(core::Tr("Item"), ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn(core::Tr("Qté"), ImGuiTableColumnFlags_WidthFixed,
                            kStockColumnPixels * scale);
    ImGui::TableSetupColumn(core::Tr("Total"), ImGuiTableColumnFlags_WidthFixed,
                            kPriceColumnPixels * scale);
    ImGui::TableSetupColumn("##remove", ImGuiTableColumnFlags_WidthFixed,
                            SkinSmallButtonWidth(core::Tr("Retirer"), art));
    ImGui::TableHeadersRow();
    for (std::size_t rank = 0; rank < state.cart.size(); ++rank) {
      const BuyingStoreSaleCartLine& line = state.cart[rank];
      const BuyingStoreOfferView* offer = OfferAt(view, line.client_slot);
      ImGui::PushID(kCartIdBase + static_cast<int>(rank));
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      const TradeItemView item =
          offer != nullptr ? offer->item : TradeItemView{.item_id = line.item_id};
      NoteItem(DrawTradeItem(item), item, line.item_id, line.client_slot, state.item_menu, action);
      ImGui::TableNextColumn();
      ImGui::AlignTextToFramePadding();
      ImGui::TextColored(kTradeBlackText, "%d", line.amount);
      ImGui::TableNextColumn();
      ImGui::AlignTextToFramePadding();
      ImGui::TextColored(kTradeBlackText, "%s",
                         TradeAmountText(UnitPrice(view, line) * line.amount, "z").c_str());
      ImGui::TableNextColumn();
      if (view.awaiting) {
        ImGui::BeginDisabled();
      }
      if (SkinSmallButton(core::Tr("Retirer"), art)) {
        removed = rank;
      }
      if (view.awaiting) {
        ImGui::EndDisabled();
      }
      ImGui::PopID();
    }
    ImGui::EndTable();
  }
  PaintWindowScrollbar(art);
  ImGui::EndChild();
  if (removed < state.cart.size()) {
    state.cart.erase(state.cart.begin() + static_cast<std::ptrdiff_t>(removed));
  }
}

void DrawFooter(BuyingStoreSellState& state, const BuyingStoreSellView& view, const SkinArt& art) {
  ImGui::Text(core::Tr("Total : %s"), TradeAmountText(view.cart_total, "z").c_str());
  ImGui::Text(core::Tr("Fonds de l'acheteur : %s"), TradeAmountText(view.buyer_funds, "z").c_str());
  // UNE LIGNE TOUJOURS ÉMISE : le pied a une hauteur réservée, une ligne qui
  // apparaîtrait décalerait les boutons.
  if (!view.blockers.empty() && !state.cart.empty()) {
    ImGui::TextColored(kTradeRefusedText, "%.*s", static_cast<int>(view.blockers[0].size()),
                       view.blockers[0].data());
  } else if (view.awaiting) {
    ImGui::TextDisabled("%s", core::Tr("En attente du serveur..."));
  } else {
    ImGui::TextDisabled(" ");
  }

  const bool blocked = state.cart.empty() || !view.blockers.empty() || view.awaiting;
  if (blocked) {
    ImGui::BeginDisabled();
  }
  if (SkinButton(core::Tr("Vendre"), art)) {
    state.pending = state.cart;
    AskModal(kSaleConfirmId);
  }
  if (blocked) {
    ImGui::EndDisabled();
  }
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
}

// LE RÉCAPITULATIF DE LA VENTE : chaque ligne, son montant, et le total. Le
// client d'origine ne demande rien ; une vente ne se défait pas.
void DrawSaleConfirmation(BuyingStoreSellState& state, const BuyingStoreSellView& view,
                          const SkinArt& art, BuyingStoreSellAction& action) {
  std::string question;
  std::array<char, 160> line_text{};
  if (view.buyer.empty()) {
    question = core::Tr("Vendre ceci ?");
  } else {
    std::snprintf(line_text.data(), line_text.size(), core::Tr("Vendre ceci à %.*s ?"),
                  static_cast<int>(view.buyer.size()), view.buyer.data());
    question = line_text.data();
  }
  std::int64_t total = 0;
  for (const BuyingStoreSaleCartLine& line : state.pending) {
    const BuyingStoreOfferView* offer = OfferAt(view, line.client_slot);
    const std::string_view name = offer != nullptr ? offer->item.name : std::string_view("?");
    const std::int64_t amount = UnitPrice(view, line) * line.amount;
    total += amount;
    std::snprintf(line_text.data(), line_text.size(), "\n%.*s x%d : %s",
                  static_cast<int>(name.size()), name.data(), line.amount,
                  TradeAmountText(amount, "z").c_str());
    question.append(line_text.data());
  }
  std::snprintf(line_text.data(), line_text.size(), core::Tr("Total : %s"),
                TradeAmountText(total, "z").c_str());
  question.append("\n").append(line_text.data());
  switch (DrawModalBox(kSaleConfirmId, question.c_str(), core::Tr("Vendre"), core::Tr("Annuler"),
                       art)) {
    case ModalAnswer::kYes:
      action.sell = std::move(state.pending);
      state.pending.clear();
      break;
    case ModalAnswer::kNo:
      state.pending.clear();
      break;
    case ModalAnswer::kWaiting:
      break;
  }
}

}  // namespace

bool OpenBuyingStoreSellOn(BuyingStoreSellState& state, std::uint32_t buyer_account_id,
                           std::uint32_t store_id) {
  state.open = true;
  if (state.buyer_seen == buyer_account_id && state.store_seen == store_id) {
    return false;
  }
  state.buyer_seen = buyer_account_id;
  state.store_seen = store_id;
  state.cart.clear();
  state.typed.clear();
  state.pending.clear();
  return true;
}

int BuyingStoreAddable(std::span<const BuyingStoreWantedView> wanted,
                       std::span<const BuyingStoreSaleCartLine> cart,
                       const BuyingStoreOfferView& offer) {
  const BuyingStoreWantedView* line = WantedOf(wanted, offer.item.item_id);
  if (line == nullptr || !offer.barred.empty()) {
    return 0;
  }
  int item_in_sale = 0;
  int slot_in_sale = 0;
  for (const BuyingStoreSaleCartLine& sold : cart) {
    if (sold.item_id == offer.item.item_id) {
      item_in_sale += sold.amount;
    }
    if (sold.client_slot == offer.client_slot) {
      slot_in_sale += sold.amount;
    }
  }
  const int addable = std::min({line->wanted - item_in_sale, offer.owned - slot_in_sale,
                                kBuyingStoreMostSellAmount - slot_in_sale});
  return std::max(addable, 0);
}

int AddToBuyingStoreSale(BuyingStoreSellState& state, std::span<const BuyingStoreWantedView> wanted,
                         const BuyingStoreOfferView& offer, int amount) {
  const int added = std::min(std::max(amount, 0), BuyingStoreAddable(wanted, state.cart, offer));
  if (added <= 0) {
    return 0;
  }
  const auto same =
      std::ranges::find(state.cart, offer.client_slot, &BuyingStoreSaleCartLine::client_slot);
  if (same != state.cart.end()) {
    same->amount += added;
  } else {
    state.cart.push_back(BuyingStoreSaleCartLine{
        .client_slot = offer.client_slot, .item_id = offer.item.item_id, .amount = added});
  }
  // La quantité tapée se recale sur ce qui reste.
  std::erase_if(state.typed, [&offer](const BuyingStoreTypedAmount& typed) {
    return typed.client_slot == offer.client_slot;
  });
  return added;
}

BuyingStoreSellAction DrawBuyingStoreSellWindow(BuyingStoreSellState& state,
                                                const BuyingStoreSellView& view, const SkinArt& art,
                                                ItemLinkFacts link_facts) {
  BuyingStoreSellAction action;
  if (!state.open) {
    return action;
  }
  const float scale = Scale();
  ImGui::SetNextWindowSize(ImVec2(kFirstWidthPixels * scale, kFirstHeightPixels * scale),
                           ImGuiCond_FirstUseEver);
  // « ### » FIGE L'IDENTITÉ : le nom de l'acheteur peut arriver après la liste.
  std::array<char, 128> shown{};
  if (view.buyer.empty()) {
    std::snprintf(shown.data(), shown.size(), "%s", core::Tr("Vendre"));
  } else {
    std::snprintf(shown.data(), shown.size(), core::Tr("Vendre à %.*s"),
                  static_cast<int>(view.buyer.size()), view.buyer.data());
  }
  const std::string title = std::string(shown.data()) + kBuyingStoreSellWindowName;
  const bool body_shown = BeginSkinnedWindow(title.c_str(), &state.open, art, SkinnedTitle::kBar,
                                             ImGuiWindowFlags_NoCollapse);
  if (body_shown && state.open) {
    if (!view.store_title.empty()) {
      ImGui::TextColored(kTradeBlackText, "%.*s", static_cast<int>(view.store_title.size()),
                         view.store_title.data());
    }
    if (!view.outcome.empty()) {
      if (!view.store_title.empty()) {
        ImGui::SameLine();
      }
      ImGui::TextColored(view.outcome_done ? kTradeDoneText : kTradeRefusedText, "%.*s",
                         static_cast<int>(view.outcome.size()), view.outcome.data());
    }
    const float pane = ImGui::GetTextLineHeightWithSpacing() * kPaneRows;
    const float footer =
        ImGui::GetTextLineHeightWithSpacing() * kFooterTextRows + SkinFooterHeight(art);
    DrawWanted(view, art, pane, state.item_menu, action);
    DrawOffers(state, view, art, pane, action);
    DrawCart(state, view, art, footer, action);
    DrawFooter(state, view, art);
    DrawSaleConfirmation(state, view, art, action);
    if (const ChatLinkAsk chosen = DrawTradeItemMenu(kItemMenuId, state.item_menu, link_facts);
        chosen.gesture != ChatLinkGesture::kNothing) {
      action.describe_id = chosen.item_id;
      action.describe_slot = static_cast<std::uint16_t>(chosen.token);
      action.item_gesture = chosen.gesture;
    }
  }
  EndSkinnedWindow();
  if (!state.open) {
    action.close = true;
    action.sell.clear();
  }
  return action;
}

}  // namespace ro::ui
