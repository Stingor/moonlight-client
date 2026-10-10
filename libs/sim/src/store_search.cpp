// La recherche d'échoppes. Voir l'en-tête.

#include "ro/sim/store_search.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <utility>

namespace ro::sim {

void StoreSearch::TellOpened(StoreSearchEffect effect, int remaining_uses) {
  open_ = true;
  ++opening_seq_;
  effect_ = effect;
  remaining_uses_ = remaining_uses;
  results_.clear();
  more_on_server_ = false;
  standing_failure_.reset();
  spotlit_owner_aid_ = 0;
  marker_.reset();
}

void StoreSearch::TellResults(bool first_page, bool more_on_server, int remaining_uses,
                              std::vector<StoreSearchResult> page) {
  if (!open_) {
    return;
  }
  if (first_page) {
    results_.clear();
    ++first_page_seq_;
  }
  results_.insert(results_.end(), std::make_move_iterator(page.begin()),
                  std::make_move_iterator(page.end()));
  more_on_server_ = more_on_server;
  remaining_uses_ = remaining_uses;
  standing_failure_.reset();
  ++answer_seq_;
}

void StoreSearch::TellFailure(std::uint8_t reason) {
  if (!open_) {
    return;
  }
  failure_ = reason;
  standing_failure_ = reason;
  ++failure_seq_;
  ++answer_seq_;
  switch (reason) {
    case kStoreSearchNothingFound:
      results_.clear();
      more_on_server_ = false;
      ++first_page_seq_;
      --remaining_uses_;
      break;
    case kStoreSearchStoreUnavailable:
      spotlit_owner_aid_ = 0;
      marker_.reset();
      break;
    default:
      break;
  }
}

void StoreSearch::TellStorePosition(std::int16_t x, std::int16_t y) {
  if (!open_) {
    return;
  }
  if (x < 0 || y < 0) {
    marker_.reset();
    return;
  }
  marker_ = StoreSearchCell{.x = x, .y = y};
}

void StoreSearch::NoteClickSent(std::uint32_t owner_aid) {
  marker_.reset();
  spotlit_owner_aid_ = owner_aid;
}

void StoreSearch::Close() {
  open_ = false;
  standing_failure_.reset();
  results_.clear();
  more_on_server_ = false;
  spotlit_owner_aid_ = 0;
  marker_.reset();
}

void StoreSearch::CloseForMapChange() { Close(); }

bool StoreSearchNameMatches(std::string_view name, std::string_view query, bool similar) {
  if (similar && !query.empty() && name.find(query) != std::string_view::npos) {
    return true;
  }
  return name == query;
}

std::vector<std::uint32_t> FindStoreSearchIds(std::span<const StoreSearchCatalogueEntry> catalogue,
                                              std::string_view query, bool cards_only,
                                              bool similar) {
  std::vector<std::uint32_t> found;
  for (const StoreSearchCatalogueEntry& entry : catalogue) {
    if (cards_only && !entry.card) {
      continue;
    }
    // ⚠ UN NOM VIDE NE SE COMPARE PAS : la table d'origine ne parcourt que les
    // objets qui en ont un.
    if (entry.name.empty()) {
      continue;
    }
    if (StoreSearchNameMatches(entry.name, query, similar) ||
        (entry.card && !entry.card_word.empty() &&
         StoreSearchNameMatches(entry.card_word, query, similar))) {
      found.push_back(entry.item_id);
    }
  }
  return found;
}

std::optional<std::int32_t> ReadStoreSearchPrice(std::string_view text) {
  if (text.empty()) {
    return 0;
  }
  std::int32_t value = 0;
  const char* const end = text.data() + text.size();
  const auto [stop, error] = std::from_chars(text.data(), end, value);
  if (error != std::errc() || stop != end) {
    return std::nullopt;
  }
  // L'ÉCRITURE DOIT ÊTRE LA SEULE qui donne ce nombre : relue puis réécrite,
  // elle doit revenir identique.
  std::array<char, sizeof("-2147483648")> canonical{};
  const auto written = std::to_chars(canonical.data(), canonical.data() + canonical.size(), value);
  if (std::string_view(canonical.data(),
                       static_cast<std::size_t>(written.ptr - canonical.data())) != text) {
    return std::nullopt;
  }
  return value;
}

StoreSearchPlan PlanStoreSearch(const StoreSearchForm& form,
                                std::span<const StoreSearchCatalogueEntry> catalogue) {
  StoreSearchPlan plan;
  plan.kind = form.kind;
  if (form.item_name.empty()) {
    plan.fault = StoreSearchFault::kNoItemName;
    return plan;
  }
  plan.item_ids = FindStoreSearchIds(catalogue, form.item_name, false, form.similar);
  if (plan.item_ids.empty()) {
    plan.fault = StoreSearchFault::kUnknownItem;
    return plan;
  }
  if (!form.card_name.empty()) {
    plan.card_ids = FindStoreSearchIds(catalogue, form.card_name, true, form.similar);
    if (plan.card_ids.empty()) {
      plan.fault = StoreSearchFault::kUnknownCard;
      return plan;
    }
  }
  if (plan.item_ids.size() + plan.card_ids.size() > kStoreSearchMostIds) {
    plan.fault = StoreSearchFault::kTooManyIds;
    return plan;
  }
  const std::optional<std::int32_t> lowest = ReadStoreSearchPrice(form.min_price);
  const std::optional<std::int32_t> highest = ReadStoreSearchPrice(form.max_price);
  // ⚠ UN MINIMUM AU-DESSUS DU MAXIMUM n'est refusé que si le maximum est saisi :
  // vide, il vaut zéro et ne borne rien.
  if (!lowest.has_value() || !highest.has_value() ||
      (*lowest > *highest && !form.max_price.empty())) {
    plan.fault = StoreSearchFault::kBadPrice;
    return plan;
  }
  plan.min_price_zeny = static_cast<std::uint32_t>(*lowest);
  plan.max_price_zeny = static_cast<std::uint32_t>(*highest);
  if (plan.max_price_zeny == 0 && plan.min_price_zeny != 0) {
    plan.max_price_zeny = kVendingMostPriceZeny;
  }
  return plan;
}

std::span<const StoreSearchResult> StoreSearchPage(std::span<const StoreSearchResult> results,
                                                   int page) {
  if (page < 1) {
    return {};
  }
  const std::size_t first = static_cast<std::size_t>(page - 1) * kStoreSearchResultsPerPage;
  if (first >= results.size()) {
    return {};
  }
  return results.subspan(first, std::min(kStoreSearchResultsPerPage, results.size() - first));
}

bool StoreSearchPageHeld(std::size_t result_count, int page) {
  return page >= 1 && static_cast<std::size_t>(page) * kStoreSearchResultsPerPage < result_count;
}

bool StoreSearchMayAskNextPage(std::size_t result_count, int page, bool more_on_server) {
  return more_on_server && page >= 1 &&
         static_cast<std::size_t>(page) * kStoreSearchResultsPerPage == result_count;
}

StoreSearchWindowStep NextStoreSearchWindowStep(const StoreSearchWindowFacts& facts) {
  if (!facts.search_open) {
    return StoreSearchWindowStep::kHide;
  }
  if (!facts.shows_current_opening) {
    return StoreSearchWindowStep::kReopen;
  }
  if (facts.close_asked || !facts.window_open) {
    return StoreSearchWindowStep::kSendClose;
  }
  return StoreSearchWindowStep::kFollow;
}

}  // namespace ro::sim