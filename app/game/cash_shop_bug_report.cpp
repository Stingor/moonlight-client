#include "game/cash_shop_bug_report.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdio>
#include <format>
#include <string>
#include <string_view>

#include "ro/core/i18n.h"

namespace moonclient {
namespace {

using ro::ui::CashShopCartLine;
using ro::ui::CashShopPurchaseNote;

// Le libellé d'un rapport — « Boutique de vote : panier de 3 lignes » —,
// jamais plus long.
constexpr std::size_t kLabelBytes = 192;
// Les lignes du panier que le rapport cite. ⚠ UNE BORNE : le contexte tient en
// un kilo-octet côté serveur.
constexpr std::size_t kCartLinesCited = 4;
// Le nom de la boutique, la recherche et le refus, en octets avant échappement.
constexpr std::size_t kTitleBytes = 32;
constexpr std::size_t kSearchBytes = 32;
constexpr std::size_t kRefusalBytes = 80;

[[nodiscard]] const char* Flag(bool value) { return value ? "true" : "false"; }

[[nodiscard]] const char* PurchaseWord(CashShopPurchaseNote note) {
  switch (note) {
    case CashShopPurchaseNote::kNone:
      return "none";
    case CashShopPurchaseNote::kBought:
      return "bought";
    case CashShopPurchaseNote::kRefused:
      return "refused";
  }
  return "?";
}

// Le total du panier, tel que la fenêtre l'affiche.
[[nodiscard]] long long CartTotal(const ro::ui::CashShopWindowState& window) {
  long long total = 0;
  for (const CashShopCartLine& line : window.cart) {
    total += static_cast<long long>(line.price) * line.amount;
  }
  return total;
}

// Le panier : ses premières lignes, son total, et s'il est payable.
[[nodiscard]] std::string CartFields(const ro::ui::CashShopWindowState& window,
                                     const ro::ui::CashShopView& view) {
  const long long total = CartTotal(window);
  const long long spendable =
      ro::ui::CashShopSpendable(view.vote_points, view.event_points, window.use_event_points);
  std::string fields = std::format(R"("cart":{{"total":{},"affordable":{},"lines":[)", total,
                                   Flag(total <= spendable));
  const std::size_t cited = std::min(window.cart.size(), kCartLinesCited);
  for (std::size_t index = 0; index < cited; ++index) {
    const CashShopCartLine& line = window.cart[index];
    fields += std::format(R"({}{{"item_id":{},"tab":{},"amount":{},"price":{}}})",
                          index == 0 ? "" : ",", line.item_id, line.tab, line.amount, line.price);
  }
  fields += std::format(R"(],"more":{}}})", window.cart.size() - cited);
  return fields;
}

}  // namespace

BugReportContext CashShopBugReport(const ro::ui::CashShopWindowState& window,
                                   const ro::ui::CashShopView& view) {
  const std::string_view search(window.filter.InputBuf);
  const std::size_t tab_cards = window.tab < view.tabs.size() ? view.tabs[window.tab].size() : 0;
  std::string fields = std::format(
      R"("cash_shop":{{"title":"{}","tab":{},"tab_cards":{},"slot":{},"sort":{},"ascending":{},)"
      R"("search":"{}","vote_points":{},"event_points":{},"use_event_points":{},)"
      R"("event_points_spent":{},"purchase":"{}",)",
      JsonClipped(view.title, kTitleBytes), window.tab, tab_cards, window.slot, window.sort,
      Flag(window.ascending), JsonClipped(search, kSearchBytes), view.vote_points,
      view.event_points, Flag(window.use_event_points),
      ro::ui::CashShopEventPointsToSpend(CartTotal(window), view.event_points,
                                         window.use_event_points),
      PurchaseWord(view.purchase));
  // ⚠ LE REFUS NE SE CITE QUE QUAND LA FENÊTRE L'AFFICHE : ailleurs, le texte
  // restant serait celui d'un achat que le joueur n'a pas vu échouer.
  if (view.purchase == CashShopPurchaseNote::kRefused) {
    fields += std::format(R"("refusal":"{}",)", JsonClipped(view.refusal, kRefusalBytes));
  }
  fields += CartFields(window, view);
  fields += '}';

  // Le libellé : le panier quand il est rempli, sinon l'onglet.
  std::array<char, kLabelBytes> label{};
  if (!window.cart.empty()) {
    std::snprintf(label.data(), label.size(),
                  ro::core::Tr("Boutique de vote : panier de %d ligne(s), %lld pts"),
                  static_cast<int>(window.cart.size()), CartTotal(window));
  } else {
    std::snprintf(label.data(), label.size(), ro::core::Tr("Boutique de vote : onglet %d"),
                  static_cast<int>(window.tab));
  }
  return FieldsBugReport(label.data(), fields);
}

}  // namespace moonclient
