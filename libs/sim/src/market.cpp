// Le Marché : la réponse qui se remplit, la saisie jugée, le tri et les pages.
// Voir l'en-tête.

#include "ro/sim/market.h"

#include <algorithm>
#include <charconv>
#include <system_error>
#include <utility>

#include "ro/core/search_key.h"

namespace ro::sim {
namespace {

// UNE SAISIE TOUTE EN CHIFFRES est un identifiant d'objet ; rien sinon.
std::optional<std::uint32_t> ReadItemId(std::string_view text) {
  if (text.empty()) {
    return std::nullopt;
  }
  std::uint32_t item_id = 0;
  const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), item_id);
  if (error != std::errc() || end != text.data() + text.size()) {
    return std::nullopt;
  }
  return item_id;
}

std::string_view Trimmed(std::string_view text) {
  constexpr std::string_view kBlanks = " \t";
  const std::size_t first = text.find_first_not_of(kBlanks);
  if (first == std::string_view::npos) {
    return {};
  }
  const std::size_t last = text.find_last_not_of(kBlanks);
  return text.substr(first, last - first + 1);
}

std::uint8_t RefineOf(int refine) {
  return static_cast<std::uint8_t>(std::clamp(refine, 0, kMarketHighestRefine));
}

}  // namespace

void MarketAnswer::Await(const MarketQuery& query) {
  query_id_ = query.query_id;
  kind_ = query.kind;
  store_id_ = query.store_id;
  owner_aid_ = query.owner_aid;
  waiting_ = true;
  complete_ = false;
  truncated_ = false;
  parts_seen_ = 0;
  offers_.clear();
}

bool MarketAnswer::Take(MarketReplyPart part) {
  if (!waiting_ || part.query_id != query_id_) {
    return false;
  }
  // ⚠ LES TRAMES ARRIVENT DANS L'ORDRE : une trame hors rang dit qu'une autre
  // s'est perdue, et la réponse ne serait pas entière.
  if (part.part != parts_seen_) {
    return false;
  }
  ++parts_seen_;
  truncated_ = part.truncated;
  offers_.insert(offers_.end(), std::make_move_iterator(part.offers.begin()),
                 std::make_move_iterator(part.offers.end()));
  if (parts_seen_ >= part.part_count) {
    waiting_ = false;
    complete_ = true;
    ++complete_seq_;
  }
  return true;
}

std::uint16_t Market::NextQueryId() noexcept {
  ++last_query_id_;
  if (last_query_id_ == 0) {
    ++last_query_id_;
  }
  return last_query_id_;
}

void Market::NoteSent(const MarketQuery& query) {
  if (query.store_id != 0) {
    store_.Await(query);
  } else {
    search_.Await(query);
  }
}

void Market::Tell(MarketReplyPart part) {
  if (part.status == MarketStatus::kOpen) {
    open_query_ = std::move(part.query);
    ++open_seq_;
    return;
  }
  const bool for_store = part.store_id != 0;
  MarketAnswer& answer = for_store ? store_ : search_;
  if (part.status == MarketStatus::kOk) {
    static_cast<void>(answer.Take(std::move(part)));
    return;
  }
  // UN REFUS d'une demande périmée ne dit plus rien ; un refus sans demande
  // (numéro zéro) se dit toujours.
  if (part.query_id != 0 && part.query_id != answer.query_id()) {
    return;
  }
  // La réponse n'arrivera pas : on ne l'attend plus.
  if (part.query_id != 0) {
    answer.Abandon();
  }
  refusal_ = part.status;
  refusal_for_store_ = for_store;
  ++refusal_seq_;
}

MarketPlan PlanMarketSearch(const MarketForm& form,
                            std::span<const StoreSearchCatalogueEntry> catalogue) {
  MarketPlan plan;
  MarketQuery& query = plan.query;
  query.kind = form.kind;
  query.category = form.category;
  if (form.refine_min > form.refine_max) {
    plan.fault = MarketFault::kBadRefine;
    return plan;
  }
  query.refine_min = RefineOf(form.refine_min);
  query.refine_max = RefineOf(form.refine_max);
  const std::optional<std::int32_t> min_price = ReadStoreSearchPrice(Trimmed(form.min_price));
  const std::optional<std::int32_t> max_price = ReadStoreSearchPrice(Trimmed(form.max_price));
  if (!min_price.has_value() || !max_price.has_value() || *min_price < 0 || *max_price < 0 ||
      (*max_price != 0 && *max_price < *min_price)) {
    plan.fault = MarketFault::kBadPrice;
    return plan;
  }
  query.price_min_zeny = static_cast<std::uint32_t>(*min_price);
  query.price_max_zeny = static_cast<std::uint32_t>(*max_price);
  query.flags = static_cast<std::uint8_t>((form.with_cards ? kMarketWithCards : 0) |
                                          (form.with_options ? kMarketWithOptions : 0));
  // UNE BOUTIQUE D'ACHAT OU DE PNJ NE PORTE QUE DU NEUF : ni raffinement, ni
  // carte, ni option. Ces filtres ne partent pas, ils videraient la liste.
  if (form.kind != MarketKind::kVending) {
    query.refine_min = 0;
    query.refine_max = kMarketHighestRefine;
    query.flags = 0;
  }
  const std::string_view text = Trimmed(form.text);
  if (text.empty()) {
    return plan;
  }
  if (const std::optional<std::uint32_t> item_id = ReadItemId(text); item_id.has_value()) {
    query.item_ids.push_back(*item_id);
    return plan;
  }
  const std::string needle = core::SearchKey(text);
  for (const StoreSearchCatalogueEntry& entry : catalogue) {
    if (!core::SearchKeyContains(core::SearchKey(entry.name), needle)) {
      continue;
    }
    if (query.item_ids.size() == kMarketMostIds) {
      plan.fault = MarketFault::kTooManyItems;
      query.item_ids.clear();
      return plan;
    }
    query.item_ids.push_back(entry.item_id);
  }
  if (query.item_ids.empty()) {
    plan.fault = MarketFault::kUnknownItem;
  }
  return plan;
}

MarketQuery MarketCompareQuery(MarketKind kind, std::uint32_t owner_aid) {
  MarketQuery query;
  query.kind = kind;
  query.flags = kMarketCompare;
  query.owner_aid = owner_aid;
  return query;
}

MarketQuery MarketStoreQuery(MarketKind kind, std::uint32_t store_id, std::uint32_t owner_aid) {
  MarketQuery query;
  query.kind = kind;
  query.store_id = store_id;
  query.owner_aid = owner_aid;
  return query;
}

std::vector<std::size_t> MarketOrder(std::span<const MarketOffer> offers,
                                     std::span<const std::string> item_keys, MarketSort sort) {
  std::vector<std::size_t> order(offers.size());
  for (std::size_t rank = 0; rank < order.size(); ++rank) {
    order[rank] = rank;
  }
  const auto key_of = [&item_keys](std::size_t rank) -> std::string_view {
    return rank < item_keys.size() ? std::string_view(item_keys[rank]) : std::string_view();
  };
  // -1, 0 ou 1 selon la colonne seule.
  const auto compare = [&](std::size_t left, std::size_t right) -> int {
    const MarketOffer& a = offers[left];
    const MarketOffer& b = offers[right];
    const auto sign = [](auto x, auto y) { return x < y ? -1 : (y < x ? 1 : 0); };
    switch (sort.column) {
      case MarketColumn::kItem:
        return sign(key_of(left), key_of(right));
      case MarketColumn::kPrice:
        // Une monnaie après l'autre, les zeny d'abord : 200 points ne se
        // comparent pas à 200 zeny.
        if (a.currency != b.currency) {
          return sign(a.currency, b.currency);
        }
        return sign(a.price_zeny, b.price_zeny);
      case MarketColumn::kAmount:
        return sign(a.item.amount, b.item.amount);
      case MarketColumn::kShop:
        return sign(core::SearchKey(a.shop_name), core::SearchKey(b.shop_name));
      case MarketColumn::kMap:
        return sign(a.map_name, b.map_name);
    }
    return 0;
  };
  std::stable_sort(order.begin(), order.end(), [&](std::size_t left, std::size_t right) {
    const int by_column = compare(left, right);
    if (by_column != 0) {
      return sort.descending ? by_column > 0 : by_column < 0;
    }
    return offers[left].price_zeny < offers[right].price_zeny;
  });
  return order;
}

int MarketPageCount(std::size_t offer_count) noexcept {
  const std::size_t pages = (offer_count + kMarketRowsPerPage - 1) / kMarketRowsPerPage;
  return static_cast<int>(std::max<std::size_t>(pages, 1));
}

std::span<const std::size_t> MarketPage(std::span<const std::size_t> order, int page) noexcept {
  const int shown = std::clamp(page, 1, MarketPageCount(order.size()));
  const std::size_t first = static_cast<std::size_t>(shown - 1) * kMarketRowsPerPage;
  if (first >= order.size()) {
    return {};
  }
  return order.subspan(first, std::min(kMarketRowsPerPage, order.size() - first));
}

}  // namespace ro::sim
