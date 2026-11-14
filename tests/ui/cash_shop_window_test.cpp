// LA FENÊTRE DE LA BOUTIQUE : ce que le panier accepte, ce que l'achat
// dépense, et le rangement par emplacement.
//
// 🔴 LE PANIER NE DÉPASSE PAS QUATRE-VINGT-DIX-NEUF PAR LIGNE : au-delà, le
// serveur refuse l'achat ENTIER, et le joueur ne verrait qu'un « l'échange a
// échoué » sans rien pour l'expliquer.

#include "ro/ui/cash_shop_window.h"

#include <array>
#include <cstring>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/ui_context.h"

using ro::ui::AddToCashShopCart;
using ro::ui::CashShopCard;
using ro::ui::CashShopEventPointsToSpend;
using ro::ui::CashShopSlotOf;
using ro::ui::CashShopSpendable;
using ro::ui::CashShopTabShown;
using ro::ui::CashShopView;
using ro::ui::CashShopWindowAction;
using ro::ui::CashShopWindowState;
using ro::ui::kCashShopMostPerLine;
using ro::ui::kCashShopSlotOther;
using ro::ui::kCashShopTabCount;

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;

// Les onglets que ce serveur remplit : nouveautés, populaires, consommables,
// divers.
constexpr std::size_t kNewTab = 0;
constexpr std::size_t kLimitedTab = 2;
constexpr std::size_t kConsumablesTab = 6;

}  // namespace

TEST_CASE("Boutique : seuls quatre onglets se montrent", "[ui][cash_shop]") {
  CHECK(CashShopTabShown(kNewTab));
  CHECK(CashShopTabShown(1));
  CHECK_FALSE(CashShopTabShown(kLimitedTab));
  CHECK(CashShopTabShown(kConsumablesTab));
  CHECK(CashShopTabShown(7));
  CHECK_FALSE(CashShopTabShown(8));
  CHECK_FALSE(CashShopTabShown(kCashShopTabCount));
}

TEST_CASE("Boutique : un costume se range avant la pièce qu'il imite", "[ui][cash_shop]") {
  // Une coiffe qui porte aussi le masque de costume se range en costume.
  CHECK(CashShopSlotOf(0x0400 | 0x0100).key == CashShopSlotOf(0x0400).key);
  CHECK(CashShopSlotOf(0x2000 | 0x0004).key == CashShopSlotOf(0x2000).key);
  CHECK(std::strcmp(CashShopSlotOf(0x0100).label, "Head top") == 0);
  CHECK(CashShopSlotOf(0).key == kCashShopSlotOther);
}

TEST_CASE("Boutique : le panier plafonne à ce que le serveur accepte", "[ui][cash_shop]") {
  CashShopWindowState state;
  for (int click = 0; click < kCashShopMostPerLine + 5; ++click) {
    AddToCashShopCart(state, 5000, kNewTab, 10);
  }
  REQUIRE(state.cart.size() == 1);
  CHECK(state.cart[0].amount == kCashShopMostPerLine);

  SECTION("le même item pris dans un autre onglet est une autre ligne") {
    AddToCashShopCart(state, 5000, kConsumablesTab, 10);
    CHECK(state.cart.size() == 2);
  }
}

TEST_CASE("Boutique : les points d'événement ne se dépensent qu'au plus le total",
          "[ui][cash_shop]") {
  CHECK(CashShopEventPointsToSpend(100, 500, false) == 0);
  CHECK(CashShopEventPointsToSpend(100, 500, true) == 100);
  CHECK(CashShopEventPointsToSpend(100, 30, true) == 30);
  CHECK(CashShopEventPointsToSpend(0, 30, true) == 0);
  CHECK(CashShopSpendable(10, 5, false) == 10);
  CHECK(CashShopSpendable(10, 5, true) == 15);
}

TEST_CASE("Boutique : ouverte sur un item, elle pose son onglet, son nom et le panier",
          "[ui][cash_shop]") {
  CashShopWindowState state;
  state.slot = 3;
  ro::ui::RevealCashShopItem(state, 5000, kConsumablesTab, 25, "Potion rouge");
  CHECK(state.tab == kConsumablesTab);
  CHECK(state.forced_tab == kConsumablesTab);
  CHECK(state.slot == ro::ui::kCashShopSlotAll);
  CHECK(std::strcmp(state.filter.InputBuf, "Potion rouge") == 0);
  REQUIRE(state.cart.size() == 1);
  CHECK(state.cart[0].price == 25);
}

TEST_CASE("Boutique : une ouverture sur un onglet masqué garde l'onglet d'avant",
          "[ui][cash_shop]") {
  CashShopWindowState state;
  state.tab = kConsumablesTab;
  ro::ui::OpenCashShopOn(state, 1, 0, static_cast<std::uint32_t>(kLimitedTab));
  CHECK(state.open);
  CHECK(state.tab == kConsumablesTab);
  ro::ui::OpenCashShopOn(state, 2, 0, static_cast<std::uint32_t>(kNewTab));
  CHECK(state.tab == kNewTab);
}

TEST_CASE("Boutique : ouverte, elle rapporte ses cartes ; fermée, elle ne dessine rien",
          "[ui][cash_shop]") {
  ro::ui::UiContext ui;
  const ro::ui::SkinArt art;
  const std::array<CashShopCard, 1> cards = {
      CashShopCard{.item_id = 5000, .price = 10, .name = "Potion"}};
  CashShopView view{.title = "Vote Shop", .currency = "Vote"};
  view.tabs[kNewTab] = cards;
  CashShopWindowState state;
  ro::ui::OpenCashShopOn(state, 1, 0, 0);

  ui.BeginFrame(kViewport, kFrameSeconds);
  const CashShopWindowAction drawn = ro::ui::DrawCashShopWindow(state, view, art);
  ui.EndFrame();
  CHECK_FALSE(drawn.close);
  // ⚠ LA CARTE DESSINÉE EST RAPPORTÉE, pour que `app` charge son image.
  REQUIRE(drawn.drawn.size() == 1);
  CHECK(drawn.drawn[0] == 5000);

  state.open = false;
  ui.BeginFrame(kViewport, kFrameSeconds);
  const CashShopWindowAction closed = ro::ui::DrawCashShopWindow(state, view, art);
  ui.EndFrame();
  CHECK_FALSE(closed.close);
  CHECK(closed.drawn.empty());
}
