#include "ro/ui/npc_shop_window.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <numeric>
#include <string>
#include <utility>

#include "ro/core/i18n.h"
#include "ro/ui/modal_box.h"
#include "ro/ui/shortcut_bar.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"

namespace ro::ui {
namespace {

[[nodiscard]] float Scale() { return ImGui::GetStyle().FontScaleMain; }

// ── LES MESURES DE LA FENÊTRE ───────────────────────────────────────────────
constexpr float kFirstLeftPixels = 100.0F;
constexpr float kFirstTopPixels = 100.0F;
constexpr float kFirstWidthPixels = 720.0F;
constexpr float kFirstHeightPixels = 480.0F;
// Le panier à droite, et ce que la liste garde au moins quand il paraît.
constexpr float kCartWidthPixels = 200.0F;
constexpr float kLeastListWidthPixels = 160.0F;
constexpr float kCartGapPixels = 8.0F;
// Sous les lignes du panier : le total, le verdict et le bouton.
constexpr float kCartFooterLines = 3.0F;
// Le stock d'un market ; le prix et les paliers se mesurent sur leurs lignes.
constexpr float kStockColumnPixels = 60.0F;
// Une consigne grisée composée avant d'être écrite.
constexpr std::size_t kHintBytes = 128;
constexpr float kCloseButtonPixels = 90.0F;
constexpr float kChildRoundingPixels = 6.0F;
// Le rang de départ des identités du panier, loin de celles des listes.
constexpr int kCartIdBase = 2000;

constexpr const char* kSaleConfirmId = "##npc_shop_confirm_sale";

enum class OfferColumn : int { kItem, kPrice, kStock, kSteps };

// L'ordre dans lequel se montrent les lignes, selon le tri de la table.
std::vector<std::size_t> SortedOffers(std::span<const NpcShopOfferView> offers) {
  std::vector<std::size_t> order(offers.size());
  std::iota(order.begin(), order.end(), std::size_t{0});
  const ImGuiTableSortSpecs* specs = ImGui::TableGetSortSpecs();
  if (specs == nullptr || specs->SpecsCount == 0) {
    return order;
  }
  const ImGuiTableColumnSortSpecs& spec = specs->Specs[0];
  const bool ascending = spec.SortDirection != ImGuiSortDirection_Descending;
  const auto column = static_cast<OfferColumn>(spec.ColumnUserID);
  const auto less = [column](const NpcShopOfferView& a, const NpcShopOfferView& b) {
    switch (column) {
      case OfferColumn::kPrice:
        return a.price < b.price;
      case OfferColumn::kStock:
        return a.stock < b.stock;
      case OfferColumn::kItem:
      case OfferColumn::kSteps:
        break;
    }
    return a.item.name < b.item.name;
  };
  std::ranges::stable_sort(order, [&](std::size_t left, std::size_t right) {
    return ascending ? less(offers[left], offers[right]) : less(offers[right], offers[left]);
  });
  return order;
}

std::vector<std::size_t> SortedSells(std::span<const NpcShopSellView> sells) {
  std::vector<std::size_t> order(sells.size());
  std::iota(order.begin(), order.end(), std::size_t{0});
  const ImGuiTableSortSpecs* specs = ImGui::TableGetSortSpecs();
  if (specs == nullptr || specs->SpecsCount == 0) {
    return order;
  }
  const ImGuiTableColumnSortSpecs& spec = specs->Specs[0];
  const bool ascending = spec.SortDirection != ImGuiSortDirection_Descending;
  const bool by_price = static_cast<OfferColumn>(spec.ColumnUserID) == OfferColumn::kPrice;
  std::ranges::stable_sort(order, [&](std::size_t left, std::size_t right) {
    const NpcShopSellView& a = sells[ascending ? left : right];
    const NpcShopSellView& b = sells[ascending ? right : left];
    return by_price ? a.price * a.amount < b.price * b.amount : a.item.name < b.item.name;
  });
  return order;
}

bool Matches(const ImGuiTextFilter& filter, std::string_view name) {
  return filter.PassFilter(name.data(), name.data() + name.size());
}

// Un panier qui mérite une confirmation : il vend un objet précieux, ou tout
// ce qui est vendable.
bool SaleNeedsConfirmation(const NpcShopWindowState& state, const NpcShopView& view,
                           std::span<const NpcShopCartLine> lines) {
  if (state.close_after_sale) {
    return true;
  }
  return std::ranges::any_of(lines, [&view](const NpcShopCartLine& line) {
    return std::ranges::any_of(view.sells, [&line](const NpcShopSellView& sold) {
      return sold.client_slot == line.client_slot && sold.precious;
    });
  });
}

void AskSale(NpcShopWindowState& state, const NpcShopView& view, std::vector<NpcShopCartLine> lines,
             bool immediate, NpcShopWindowAction& action) {
  if (SaleNeedsConfirmation(state, view, lines)) {
    // 🔴 LA BOÎTE NE S'OUVRE PAS ICI : on est dans une fenêtre ENFANT — la table
    // ou le panier —, et ImGui nomme un surgissant dans la pile d'identités du
    // moment. La boîte, dessinée au niveau de la fenêtre, ne se trouverait
    // jamais, et la vente attendrait sans rien dire. Elle s'ouvre au niveau de
    // la fenêtre, à la même image.
    state.pending_sale = std::move(lines);
    state.sale_asked = true;
    return;
  }
  action.sell = std::move(lines);
  action.immediate = immediate;
}

void NoteItem(const TradeItemGesture& gesture, std::uint32_t item_id, std::uint16_t slot,
              NpcShopWindowAction& action) {
  if (gesture.hovered) {
    action.hovered_id = item_id;
    action.hovered_slot = slot;
  }
  if (gesture.describe) {
    action.describe_id = item_id;
    action.describe_slot = slot;
  }
}

void DrawHeader(const NpcShopView& view) {
  ImGui::TextColored(kTradeBlackText, "%.*s", static_cast<int>(view.purse_line.size()),
                     view.purse_line.data());
  if (!view.outcome.empty()) {
    ImGui::SameLine();
    ImGui::TextColored(view.outcome_done ? kTradeDoneText : kTradeRefusedText, "  %.*s",
                       static_cast<int>(view.outcome.size()), view.outcome.data());
  }
}

void DrawTabs(NpcShopWindowState& state, const NpcShopView& view, const SkinArt& art,
              NpcShopWindowAction& action) {
  if (!BeginSkinTabBar("##npc_shop_tabs", art)) {
    return;
  }
  const auto tab = [&](NpcShopTab which, const char* label) {
    ImGuiTabItemFlags flags = 0;
    if (state.force_tab && state.tab == which) {
      flags |= ImGuiTabItemFlags_SetSelected;
    }
    if (ImGui::BeginTabItem(label, nullptr, flags)) {
      if (state.tab != which && !state.force_tab) {
        state.tab = which;
        state.cart.clear();
        state.close_after_sale = false;
        action.tab_changed = true;
      }
      ImGui::EndTabItem();
    }
  };
  if (view.can_buy) {
    tab(NpcShopTab::kBuy, core::Tr("Acheter"));
  }
  if (view.can_sell) {
    tab(NpcShopTab::kSell, core::Tr("Vendre"));
  }
  state.force_tab = false;
  EndSkinTabBar();
}

void DrawBuyList(NpcShopWindowState& state, const NpcShopView& view, const SkinArt& art,
                 NpcShopWindowAction& action) {
  const float scale = Scale();
  const bool market = view.kind == NpcShopWindowKind::kMarket;
  const int columns = market ? 4 : 3;
  // Les colonnes fixes se règlent sur leur ligne la plus large : l'item prend
  // tout le reste, au lieu d'un nom rogné à côté d'une colonne à moitié vide.
  float price_width = ImGui::CalcTextSize(core::Tr("Prix")).x;
  float steps_width = 0.0F;
  for (const NpcShopOfferView& offer : view.offers) {
    price_width = std::max(price_width, TradePriceWidth(offer.base_price, offer.price, view.unit));
    if (offer.most >= 1) {
      steps_width = std::max(steps_width, TradeStepsWidth(TradeStepsFor(offer.most, false), art));
    }
  }
  if (!ImGui::BeginTable("##npc_shop_buy", columns,
                         ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
                             ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_Sortable)) {
    return;
  }
  ImGui::TableSetupScrollFreeze(0, 1);
  ImGui::TableSetupColumn(core::Tr("Item"), ImGuiTableColumnFlags_WidthStretch, 0.0F,
                          static_cast<ImGuiID>(OfferColumn::kItem));
  ImGui::TableSetupColumn(core::Tr("Prix"), ImGuiTableColumnFlags_WidthFixed, price_width,
                          static_cast<ImGuiID>(OfferColumn::kPrice));
  if (market) {
    ImGui::TableSetupColumn(core::Tr("Stock"), ImGuiTableColumnFlags_WidthFixed,
                            kStockColumnPixels * scale, static_cast<ImGuiID>(OfferColumn::kStock));
  }
  ImGui::TableSetupColumn("##steps",
                          ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoSort,
                          steps_width, static_cast<ImGuiID>(OfferColumn::kSteps));
  ImGui::TableHeadersRow();

  for (const std::size_t index : SortedOffers(view.offers)) {
    const NpcShopOfferView& offer = view.offers[index];
    if (!Matches(state.filter, offer.item.name)) {
      continue;
    }
    ImGui::PushID(static_cast<int>(index));
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    NoteItem(DrawTradeItem(offer.item), offer.item.item_id, 0, action);
    ImGui::TableNextColumn();
    ImGui::AlignTextToFramePadding();
    const bool affordable = view.purse == kTradeNoPurse || offer.price <= view.purse;
    DrawTradePrice(offer.base_price, offer.price,
                   affordable ? kTradeBlackText : kTradeUnaffordableText, view.unit);
    if (market) {
      ImGui::TableNextColumn();
      ImGui::AlignTextToFramePadding();
      if (offer.stock == 0) {
        ImGui::TextDisabled("%s", core::Tr("épuisé"));
      } else if (offer.stock == kNpcShopNoStock) {
        ImGui::TextDisabled("-");
      } else {
        ImGui::TextColored(kTradeBlackText, "%s", TradeAmountText(offer.stock, {}).c_str());
      }
    }
    ImGui::TableNextColumn();
    if (offer.most >= 1 && !view.awaiting) {
      const TradeStepClick click =
          DrawTradeSteps(TradeStepsFor(offer.most, false), art, offer.price, view.purse);
      if (click.amount > 0) {
        const NpcShopCartLine line{.item_id = offer.item.item_id,
                                   .amount = std::min(click.amount, offer.most),
                                   .unit_price = offer.price,
                                   .most = offer.most};
        if (click.immediate) {
          action.buy = {line};
          action.immediate = true;
        } else {
          AddToNpcShopCart(state.cart, line);
        }
      }
    }
    ImGui::PopID();
  }
  if (view.offers.empty()) {
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextDisabled("%s", view.offers_listed
                                  ? core::Tr("(rien à acheter)")
                                  : core::Tr("(liste d'achat en attente du serveur...)"));
  }
  PaintWindowScrollbar(art);
  ImGui::EndTable();
}

void DrawSellList(NpcShopWindowState& state, const NpcShopView& view, const SkinArt& art,
                  NpcShopWindowAction& action) {
  if (!view.sells.empty() && !view.awaiting) {
    if (SkinButton(core::Tr("Tout ajouter au panier"), art)) {
      state.cart.clear();
      for (const NpcShopSellView& sold : view.sells) {
        state.cart.push_back(NpcShopCartLine{.item_id = sold.item.item_id,
                                             .client_slot = sold.client_slot,
                                             .amount = sold.amount,
                                             .unit_price = sold.price,
                                             .most = sold.amount});
      }
      state.close_after_sale = true;
    }
    if (ImGui::IsItemHovered()) {
      SkinTooltip(
          core::Tr("Ajoute tout ce qui est vendable ; la boutique se fermera après la "
                   "vente, qui se confirme d'abord."));
    }
  }
  if (view.hidden_favorites > 0) {
    if (!view.sells.empty()) {
      ImGui::SameLine();
    }
    ImGui::AlignTextToFramePadding();
    std::array<char, kHintBytes> hidden{};
    std::snprintf(hidden.data(), hidden.size(),
                  core::Tr("%d favori(s) masqué(s) par le verrou de vente"), view.hidden_favorites);
    DrawTradeHint(hidden.data());
  }
  if (!view.server_filter_note.empty()) {
    DrawTradeHint(view.server_filter_note);
  }
  float price_width = ImGui::CalcTextSize(core::Tr("Vente")).x;
  float steps_width = 0.0F;
  for (const NpcShopSellView& sold : view.sells) {
    price_width = std::max(price_width, TradePriceWidth(sold.base_price, sold.price, view.unit));
    steps_width = std::max(steps_width, TradeStepsWidth(TradeStepsFor(sold.amount, true), art));
  }
  if (!ImGui::BeginTable("##npc_shop_sell", 3,
                         ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
                             ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_Sortable)) {
    return;
  }
  ImGui::TableSetupScrollFreeze(0, 1);
  ImGui::TableSetupColumn(core::Tr("Item"), ImGuiTableColumnFlags_WidthStretch, 0.0F,
                          static_cast<ImGuiID>(OfferColumn::kItem));
  ImGui::TableSetupColumn(core::Tr("Vente"), ImGuiTableColumnFlags_WidthFixed, price_width,
                          static_cast<ImGuiID>(OfferColumn::kPrice));
  ImGui::TableSetupColumn("##steps",
                          ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoSort,
                          steps_width, static_cast<ImGuiID>(OfferColumn::kSteps));
  ImGui::TableHeadersRow();

  for (const std::size_t index : SortedSells(view.sells)) {
    const NpcShopSellView& sold = view.sells[index];
    if (!Matches(state.filter, sold.item.name)) {
      continue;
    }
    ImGui::PushID(static_cast<int>(sold.client_slot));
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    std::array<char, 16> count{};
    std::snprintf(count.data(), count.size(), "x%d", sold.amount);
    NoteItem(DrawTradeItem(sold.item, count.data()), sold.item.item_id, sold.client_slot, action);
    ImGui::TableNextColumn();
    ImGui::AlignTextToFramePadding();
    DrawTradePrice(sold.base_price, sold.price, kTradeBlackText, view.unit);
    ImGui::TableNextColumn();
    if (!view.awaiting) {
      const TradeStepClick click =
          DrawTradeSteps(TradeStepsFor(sold.amount, true), art, sold.price, kTradeNoPurse);
      if (click.amount > 0) {
        const NpcShopCartLine line{.item_id = sold.item.item_id,
                                   .client_slot = sold.client_slot,
                                   .amount = std::min(click.amount, sold.amount),
                                   .unit_price = sold.price,
                                   .most = sold.amount};
        if (click.immediate) {
          AskSale(state, view, {line}, true, action);
        } else {
          AddToNpcShopCart(state.cart, line);
        }
      }
    }
    ImGui::PopID();
  }
  if (view.sells.empty()) {
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextDisabled("%s", view.sells_listed
                                  ? core::Tr("(rien à vendre)")
                                  : core::Tr("(liste de vente en attente du serveur...)"));
  }
  PaintWindowScrollbar(art);
  ImGui::EndTable();
}

// UN ITEM DU SAC LÂCHÉ SUR LA LISTE DE VENTE : toute sa pile entre au panier,
// si le PNJ l'achète. Le geste que le client d'origine attend de sa fenêtre
// de vente.
//
// ⚠ LA LISTE DU SERVEUR RESTE LA SEULE PORTE : une case qu'il n'a pas mise en
// vente ne s'ajoute pas — il refuserait la vente entière —, et le refus se dit.
void AcceptBagDrop(NpcShopWindowState& state, const NpcShopView& view,
                   NpcShopWindowAction& action) {
  if (!ImGui::BeginDragDropTarget()) {
    return;
  }
  if (const ImGuiPayload* dropped = ImGui::AcceptDragDropPayload(kItemDragPayload);
      dropped != nullptr && dropped->DataSize == sizeof(ItemDrag)) {
    ItemDrag carried;
    std::memcpy(&carried, dropped->Data, sizeof(carried));
    const auto sold = std::ranges::find_if(view.sells, [&carried](const NpcShopSellView& on) {
      return on.client_slot == carried.client_slot;
    });
    if (carried.from != ItemHolder::kInventory || carried.worn || sold == view.sells.end()) {
      action.drop_refused = true;
    } else {
      AddToNpcShopCart(state.cart, NpcShopCartLine{.item_id = sold->item.item_id,
                                                   .client_slot = sold->client_slot,
                                                   .amount = sold->amount,
                                                   .unit_price = sold->price,
                                                   .most = sold->amount});
    }
  }
  ImGui::EndDragDropTarget();
}

// Le nom d'une ligne du panier : celui de la liste qui l'a fournie.
std::string_view CartName(const NpcShopView& view, const NpcShopWindowState& state,
                          const NpcShopCartLine& line) {
  if (state.tab == NpcShopTab::kSell) {
    for (const NpcShopSellView& sold : view.sells) {
      if (sold.client_slot == line.client_slot) {
        return sold.item.name;
      }
    }
  } else {
    for (const NpcShopOfferView& offer : view.offers) {
      if (offer.item.item_id == line.item_id) {
        return offer.item.name;
      }
    }
  }
  return "?";
}

void DrawCart(NpcShopWindowState& state, const NpcShopView& view, const SkinArt& art, float width,
              float footer, NpcShopWindowAction& action) {
  const bool selling = state.tab == NpcShopTab::kSell;
  ImGui::SameLine();
  ImGui::BeginChild("##npc_shop_cart", ImVec2(width, -footer), ImGuiChildFlags_Borders);
  ImGui::AlignTextToFramePadding();
  ImGui::TextUnformatted(selling ? core::Tr("Panier de vente") : core::Tr("Panier d'achat"));
  ImGui::SameLine();
  if (SkinButton(core::Tr("Vider"), art)) {
    state.cart.clear();
    state.close_after_sale = false;
  }
  ImGui::Separator();

  const float reserved = kCartFooterLines * ImGui::GetFrameHeightWithSpacing();
  ImGui::BeginChild("##npc_shop_cart_lines", ImVec2(0.0F, -reserved));
  std::size_t removed = state.cart.size();
  for (std::size_t index = 0; index < state.cart.size(); ++index) {
    NpcShopCartLine& line = state.cart[index];
    ImGui::PushID(kCartIdBase + static_cast<int>(index));
    const std::string_view name = CartName(view, state, line);
    ImGui::TextWrapped("%.*s", static_cast<int>(name.size()), name.data());
    DrawTradeQuantity(line.amount, line.most, art);
    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(kTradeBlackText, "%s",
                       TradeAmountText(line.unit_price * line.amount, view.unit).c_str());
    ImGui::SameLine();
    const float step = ImGui::GetFrameHeight();
    const float right = ImGui::GetContentRegionMax().x - step;
    if (right > ImGui::GetCursorPosX()) {
      ImGui::SetCursorPosX(right);
    }
    if (SkinButton("x", art, ImVec2(step, step))) {
      removed = index;
    }
    ImGui::Separator();
    ImGui::PopID();
  }
  PaintWindowScrollbar(art);
  ImGui::EndChild();
  if (removed < state.cart.size()) {
    state.cart.erase(state.cart.begin() + static_cast<std::ptrdiff_t>(removed));
    if (state.cart.empty()) {
      state.close_after_sale = false;
    }
  }

  ImGui::Separator();
  ImGui::Text(core::Tr("Total : %s"), TradeAmountText(view.cart_total, view.unit).c_str());
  if (!view.cart_weight.empty()) {
    ImGui::SameLine();
    ImGui::TextDisabled("%.*s", static_cast<int>(view.cart_weight.size()), view.cart_weight.data());
  }
  // 🔴 LE BOUTON SE RETIENT tant qu'une raison ferait refuser le panier : le
  // serveur refuse l'achat ENTIER, ou tronque la recette. La première raison
  // s'écrit sous le total ; toutes se lisent au survol du bouton.
  if (!view.blockers.empty()) {
    ImGui::TextColored(kTradeRefusedText, "%.*s", static_cast<int>(view.blockers[0].size()),
                       view.blockers[0].data());
  }
  const bool blocked = state.cart.empty() || !view.blockers.empty() || view.awaiting;
  if (blocked) {
    ImGui::BeginDisabled();
  }
  const char* verb = selling ? core::Tr("Vendre") : core::Tr("Acheter");
  if (SkinButton(verb, art, ImVec2(ImGui::GetContentRegionAvail().x, 0.0F))) {
    if (selling) {
      AskSale(state, view, state.cart, false, action);
    } else {
      action.buy = state.cart;
    }
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
  ImGui::EndChild();
}

void DrawSaleConfirmation(NpcShopWindowState& state, const NpcShopView& view, const SkinArt& art,
                          NpcShopWindowAction& action) {
  std::int64_t total = 0;
  for (const NpcShopCartLine& line : state.pending_sale) {
    total += line.unit_price * line.amount;
  }
  std::array<char, 256> question{};
  std::snprintf(question.data(), question.size(),
                core::Tr("Vendre %zu ligne(s) pour %s ?\nParmi elles, des items raffinés ou "
                         "sertis, ou tout ce qui est vendable."),
                state.pending_sale.size(), TradeAmountText(total, view.unit).c_str());
  switch (
      DrawModalBox(kSaleConfirmId, question.data(), core::Tr("Vendre"), core::Tr("Annuler"), art)) {
    case ModalAnswer::kYes:
      action.sell = std::move(state.pending_sale);
      state.pending_sale.clear();
      break;
    case ModalAnswer::kNo:
      state.pending_sale.clear();
      break;
    case ModalAnswer::kWaiting:
      break;
  }
}

}  // namespace

void OpenNpcShopOn(NpcShopWindowState& state, std::uint32_t opening_seq, bool buy_first) {
  state.open = true;
  state.opening_seen = opening_seq;
  state.tab = buy_first ? NpcShopTab::kBuy : NpcShopTab::kSell;
  state.force_tab = true;
  state.cart.clear();
  state.close_after_sale = false;
  state.pending_sale.clear();
}

bool AddToNpcShopCart(std::vector<NpcShopCartLine>& cart, const NpcShopCartLine& line) {
  const auto same = std::ranges::find_if(cart, [&line](const NpcShopCartLine& on) {
    return on.item_id == line.item_id && on.client_slot == line.client_slot;
  });
  if (same == cart.end()) {
    NpcShopCartLine added = line;
    added.amount = std::clamp(line.amount, 1, std::max(line.most, 1));
    cart.push_back(added);
    return true;
  }
  if (same->amount >= same->most) {
    return false;
  }
  same->amount = std::min(same->amount + line.amount, same->most);
  return true;
}

NpcShopWindowAction DrawNpcShopWindow(NpcShopWindowState& state, const NpcShopView& view,
                                      const SkinArt& art) {
  NpcShopWindowAction action;
  if (!state.open) {
    return action;
  }
  const float scale = Scale();
  ImGui::SetNextWindowPos(ImVec2(kFirstLeftPixels, kFirstTopPixels), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(kFirstWidthPixels * scale, kFirstHeightPixels * scale),
                           ImGuiCond_FirstUseEver);
  const std::string_view name =
      view.title.empty() ? std::string_view(core::Tr("Shop")) : view.title;
  const std::string title = std::string(name) + "###" + kNpcShopWindowId;
  const bool body_shown = BeginSkinnedWindow(title.c_str(), &state.open, art, SkinnedTitle::kBar,
                                             ImGuiWindowFlags_NoCollapse);
  if (body_shown && state.open) {
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, kChildRoundingPixels * scale);
    DrawTabs(state, view, art, action);
    DrawHeader(view);
    ImGui::SetNextItemWidth(-1.0F);
    if (ImGui::InputTextWithHint("##npc_shop_filter", core::Tr("Filtrer..."), state.filter.InputBuf,
                                 IM_ARRAYSIZE(state.filter.InputBuf))) {
      state.filter.Build();
    }
    DrawTradeHint(core::Tr("Clic = panier   -   Ctrl+clic = achat/vente immédiat"));
    ImGui::Separator();

    const float footer = TradeFooterHeight(art);
    const bool show_cart = !state.cart.empty();
    const float cart_width = kCartWidthPixels * scale;
    const float list_width =
        show_cart ? std::max(kLeastListWidthPixels * scale,
                             ImGui::GetContentRegionAvail().x - cart_width - kCartGapPixels * scale)
                  : 0.0F;
    ImGui::BeginChild("##npc_shop_list", ImVec2(list_width, -footer), ImGuiChildFlags_Borders);
    if (state.tab == NpcShopTab::kSell) {
      DrawSellList(state, view, art, action);
    } else {
      DrawBuyList(state, view, art, action);
    }
    ImGui::EndChild();
    if (state.tab == NpcShopTab::kSell && !view.awaiting) {
      AcceptBagDrop(state, view, action);
    }
    if (show_cart) {
      DrawCart(state, view, art, cart_width, footer, action);
    }

    ImGui::Separator();
    if (SkinButton(core::Tr("Fermer"), art, ImVec2(kCloseButtonPixels * scale, 0.0F))) {
      state.open = false;
    }
    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("%s", core::Tr("Survol : aperçu   -   Clic droit : description"));
    if (std::exchange(state.sale_asked, false)) {
      AskModal(kSaleConfirmId);
    }
    DrawSaleConfirmation(state, view, art, action);
    ImGui::PopStyleVar();
  }
  EndSkinnedWindow();
  if (!state.open) {
    action.close = true;
    action.buy.clear();
    action.sell.clear();
  }
  return action;
}

}  // namespace ro::ui
