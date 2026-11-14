// LE RAPPORT D'UN BUG DE LA BOUTIQUE DE VOTE : un JSON valable, qui cite la
// vue, les soldes, le panier et le refus affiché, borné quoi que le panier
// contienne.

#include "game/cash_shop_bug_report.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

#include "web/json_value.h"

using moonclient::CashShopBugReport;
using ro::ui::CashShopCard;
using ro::ui::CashShopCartLine;
using ro::ui::CashShopPurchaseNote;
using ro::ui::CashShopView;
using ro::ui::CashShopWindowState;

namespace {

constexpr std::size_t kComfortableBytes = 900;

constexpr std::size_t kCostumeTab = 7;
constexpr std::uint32_t kHatId = 20500;
constexpr std::int32_t kHatPrice = 150;

[[nodiscard]] bool Has(const moonclient::BugReportContext& report, std::string_view piece) {
  return report.json.find(piece) != std::string::npos;
}

}  // namespace

TEST_CASE("Rapport de la boutique : le panier, les soldes et la case Event", "[app][bugreport]") {
  const std::array cards = {CashShopCard{.item_id = kHatId, .price = kHatPrice}};
  CashShopView view;
  view.title = "Vote Shop";
  view.vote_points = 100;
  view.event_points = 80;
  view.tabs[kCostumeTab] = cards;
  CashShopWindowState window;
  window.tab = kCostumeTab;
  window.use_event_points = true;
  window.cart.push_back(
      CashShopCartLine{.item_id = kHatId, .tab = kCostumeTab, .amount = 1, .price = kHatPrice});

  const moonclient::BugReportContext report = CashShopBugReport(window, view);
  REQUIRE(moonclient::web::ParseJson(report.json).has_value());
  CHECK(Has(report, R"("tab":7,"tab_cards":1)"));
  CHECK(Has(report, R"("vote_points":100,"event_points":80,"use_event_points":true)"));
  // Le total est de 150 : l'événement en paie 80, au plus le solde.
  CHECK(Has(report, R"("event_points_spent":80)"));
  CHECK(Has(report, R"("cart":{"total":150,"affordable":true,"lines":[{"item_id":20500,)"));
  CHECK(Has(report, R"("purchase":"none")"));
  CHECK_FALSE(Has(report, R"("refusal")"));
  CHECK(report.label.find("150") != std::string::npos);
}

TEST_CASE("Rapport de la boutique : sans la case, le panier n'est pas payable",
          "[app][bugreport]") {
  CashShopView view;
  view.vote_points = 100;
  view.event_points = 80;
  view.purchase = CashShopPurchaseNote::kRefused;
  view.refusal = "Points \"insuffisants\"";
  CashShopWindowState window;
  window.cart.push_back(CashShopCartLine{.item_id = kHatId, .amount = 1, .price = kHatPrice});

  const moonclient::BugReportContext report = CashShopBugReport(window, view);
  REQUIRE(moonclient::web::ParseJson(report.json).has_value());
  CHECK(Has(report, R"("event_points_spent":0)"));
  CHECK(Has(report, R"("affordable":false)"));
  CHECK(Has(report, R"("purchase":"refused","refusal":"Points \"insuffisants\"")"));
}

TEST_CASE("Rapport de la boutique : un panier plein et de longs textes restent bornés",
          "[app][bugreport]") {
  constexpr std::size_t kManyLines = 30;
  const std::string long_text(512, 'x');
  CashShopView view;
  view.title = long_text;
  view.purchase = CashShopPurchaseNote::kRefused;
  view.refusal = long_text;
  CashShopWindowState window;
  std::snprintf(window.filter.InputBuf, sizeof(window.filter.InputBuf), "%s", long_text.c_str());
  for (std::size_t line = 0; line < kManyLines; ++line) {
    window.cart.push_back(CashShopCartLine{.item_id = 4000000000U,
                                           .tab = kCostumeTab,
                                           .amount = ro::ui::kCashShopMostPerLine,
                                           .price = 2000000000});
  }

  const moonclient::BugReportContext report = CashShopBugReport(window, view);
  REQUIRE(moonclient::web::ParseJson(report.json).has_value());
  CHECK(report.json.size() < kComfortableBytes);
  CHECK(Has(report, R"("more":26)"));
}
