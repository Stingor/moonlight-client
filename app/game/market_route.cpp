// Les décisions du Marché. Voir l'en-tête.

#include "game/market_route.h"

#include <array>
#include <cstdio>

#include "ro/core/case_fold.h"
#include "ro/core/i18n.h"
#include "ro/ui/trade_widgets.h"

namespace moonclient {
namespace {

// La place d'un prix composé.
constexpr std::size_t kPriceTextBytes = 96;
// L'unité qui suit un prix en zeny, espace comprise.
constexpr std::string_view kZenyUnit = " z";
// Le stock illimité d'un PNJ.
constexpr std::string_view kUnlimited = "—";

const ro::sim::MarketOffer* OfferAt(std::span<const ro::sim::MarketOffer> offers,
                                    std::span<const std::size_t> page_ranks, int row) {
  if (row < 0 || static_cast<std::size_t>(row) >= page_ranks.size()) {
    return nullptr;
  }
  const std::size_t rank = page_ranks[static_cast<std::size_t>(row)];
  return rank < offers.size() ? &offers[rank] : nullptr;
}

}  // namespace

MarketDestination MarketDestinationOf(const ro::sim::MarketOffer& offer) {
  return MarketDestination{
      .map_name = offer.map_name, .cell_x = offer.cell_x, .cell_y = offer.cell_y};
}

MarketRoute RouteMarketAnswer(const ro::ui::MarketAction& answer,
                              std::span<const ro::sim::MarketOffer> offers,
                              std::span<const std::size_t> page_ranks, ro::sim::MarketKind kind) {
  MarketRoute route;
  if (answer.close) {
    return route;
  }
  route.search = answer.search;
  route.refresh = answer.refresh && !answer.search;
  if (const ro::sim::MarketOffer* offer = OfferAt(offers, page_ranks, answer.go_to_row);
      offer != nullptr) {
    route.go_to = MarketDestinationOf(*offer);
  }
  if (const ro::sim::MarketOffer* offer = OfferAt(offers, page_ranks, answer.view_store_row);
      offer != nullptr) {
    route.store_query = ro::sim::MarketStoreQuery(kind, offer->store_id, offer->owner_aid);
    route.store_offer = offer;
  }
  return route;
}

MarketRefusalStep StepOfMarketRefusal(ro::sim::MarketStatus status, bool for_store) {
  if (status == ro::sim::MarketStatus::kNotInTown) {
    return MarketRefusalStep::kCloseWithNotice;
  }
  return for_store ? MarketRefusalStep::kStoreStatus : MarketRefusalStep::kStatus;
}

std::optional<ro::sim::MarketKind> MarketKindOfSignboard(ro::sim::SignboardKind signboard) {
  switch (signboard) {
    case ro::sim::SignboardKind::kVendingShop:
      return ro::sim::MarketKind::kVending;
    case ro::sim::SignboardKind::kBuyingStore:
      return ro::sim::MarketKind::kBuying;
    case ro::sim::SignboardKind::kChatRoom:
      break;
  }
  return std::nullopt;
}

int MarketTabOf(ro::sim::MarketKind kind) {
  switch (kind) {
    case ro::sim::MarketKind::kBuying:
      return ro::ui::kMarketBuyTab;
    case ro::sim::MarketKind::kNpc:
      return ro::ui::kMarketNpcTab;
    case ro::sim::MarketKind::kVending:
      break;
  }
  return ro::ui::kMarketSellTab;
}

std::string MarketOpeningQuery(std::optional<std::uint32_t> item_id) {
  return item_id.has_value() ? std::to_string(*item_id) : std::string();
}

std::string MarketPriceText(const ro::sim::MarketOffer& offer,
                            std::string_view currency_item_name) {
  const std::string amount = ro::ui::TradeAmountText(offer.price_zeny, {});
  std::array<char, kPriceTextBytes> text{};
  switch (offer.currency) {
    case ro::sim::MarketCurrency::kCash:
      std::snprintf(text.data(), text.size(), ro::core::Tr("%s cash"), amount.c_str());
      break;
    case ro::sim::MarketCurrency::kItem:
      std::snprintf(text.data(), text.size(), "%s x %.*s", amount.c_str(),
                    static_cast<int>(currency_item_name.size()), currency_item_name.data());
      break;
    case ro::sim::MarketCurrency::kPoints:
      std::snprintf(text.data(), text.size(), "%s %s", amount.c_str(), offer.currency_name.c_str());
      break;
    case ro::sim::MarketCurrency::kBarter:
      if (offer.price_zeny == 0) {
        return ro::core::Tr("troc");
      }
      std::snprintf(text.data(), text.size(), ro::core::Tr("troc + %s z"), amount.c_str());
      break;
    case ro::sim::MarketCurrency::kZeny:
      return amount + std::string(kZenyUnit);
  }
  return text.data();
}

std::string MarketAmountText(const ro::sim::MarketOffer& offer, ro::sim::MarketKind kind) {
  if (kind == ro::sim::MarketKind::kNpc && offer.item.amount == 0) {
    return std::string(kUnlimited);
  }
  return std::to_string(offer.item.amount);
}

bool IsMarketCommand(std::string_view line) { return ro::core::EqualsFolded(line, kMarketCommand); }

}  // namespace moonclient
