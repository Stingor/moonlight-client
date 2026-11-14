// LES FENÊTRES DE L'ÉCHOPPE DE VENTE : la préparation qui se remplit, le prix
// qu'on tape avec ses séparateurs, le panier d'une visite, et les boutons que
// le verdict retient.
//
// 🔴 UN PRIX MAL LU D'UN FACTEUR MILLE EST LE PIRE QU'UNE SAISIE PUISSE FAIRE :
// « 1,5 » ne vaut pas quinze, il ne vaut rien, et le bouton reste retenu.

#include <array>
#include <cstring>
#include <functional>
#include <string_view>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/trade_widgets.h"
#include "ro/ui/ui_context.h"
#include "ro/ui/vending_setup_window.h"
#include "ro/ui/vending_shop_window.h"
#include "ro/ui/vending_visit_window.h"

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;

void TypePrice(ro::ui::VendingSetupLineState& line, const char* text) {
  line.price_text = {};
  std::memcpy(line.price_text.data(), text, std::strlen(text));
}

// LE BALAYAGE qui trouve un bouton : pas vertical, marge au bord gauche, et
// abscisses essayées, en fraction de la largeur.
constexpr float kSweepStepPixels = 3.0F;
constexpr float kEdgeMarginPixels = 14.0F;
constexpr std::array<float, 4> kSweepColumns = {0.0F, 0.1F, 0.25F, 0.4F};

// La fenêtre active dont le nom contient `part`.
[[nodiscard]] ImGuiWindow* WindowNamed(std::string_view part) {
  for (ImGuiWindow* window : GImGui->Windows) {
    if (window->WasActive && std::string_view(window->Name).find(part) != std::string_view::npos) {
      return window;
    }
  }
  return nullptr;
}

// LE PLUS BAS DES ÉLÉMENTS INTERACTIFS DE GAUCHE : le bouton qui agit, dans un
// pied de fenêtre comme dans une boîte de confirmation. La souris y reste.
[[nodiscard]] bool PointLowestLeftControl(const std::function<void()>& frame,
                                          std::string_view window_part) {
  const ImGuiWindow* window = WindowNamed(window_part);
  REQUIRE(window != nullptr);
  const ImVec2 top_left = window->Pos;
  const ImVec2 size = window->Size;
  ImGuiIO& io = ImGui::GetIO();
  // ⚠ LE BORD DU BAS EST LA POIGNÉE DE REDIMENSIONNEMENT : on part au-dessus.
  for (float y = top_left.y + size.y - kEdgeMarginPixels; y > top_left.y; y -= kSweepStepPixels) {
    for (const float share : kSweepColumns) {
      io.AddMousePosEvent(top_left.x + kEdgeMarginPixels + size.x * share, y);
      frame();
      if (GImGui->HoveredId != 0) {
        return true;
      }
    }
  }
  return false;
}

// Un clic là où se trouve la souris : l'enfoncement, puis le relâchement.
void Click(const std::function<void()>& frame) {
  ImGuiIO& io = ImGui::GetIO();
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
  frame();
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
  frame();
  frame();
}

}  // namespace

TEST_CASE("Échoppe : un montant se tape avec ses séparateurs de milliers", "[ui][vending]") {
  using ro::ui::ParseTradeAmount;
  CHECK(ParseTradeAmount("1500000") == 1'500'000);
  CHECK(ParseTradeAmount("1,500,000") == 1'500'000);
  CHECK(ParseTradeAmount("1.500.000") == 1'500'000);
  CHECK(ParseTradeAmount("1 500 000") == 1'500'000);
  CHECK(ParseTradeAmount("  12  ") == 12);
  // Un séparateur ailleurs qu'entre des groupes de trois ne se lit pas.
  CHECK_FALSE(ParseTradeAmount("1,5").has_value());
  CHECK_FALSE(ParseTradeAmount("1,5000").has_value());
  CHECK_FALSE(ParseTradeAmount("1234,567").has_value());
  CHECK_FALSE(ParseTradeAmount(",100").has_value());
  CHECK_FALSE(ParseTradeAmount("100,").has_value());
  CHECK_FALSE(ParseTradeAmount("12z").has_value());
  CHECK_FALSE(ParseTradeAmount("").has_value());
  CHECK_FALSE(ParseTradeAmount("-5").has_value());
  // Trop grand, il SATURE au lieu de se replier.
  CHECK(ParseTradeAmount("99999999999999999999") == ro::ui::kTradeTypedAmountCeiling);
}

TEST_CASE("Échoppe : un montant s'écrit en toutes lettres groupées", "[ui][vending]") {
  CHECK(ro::ui::SpelledTradeAmount(0) == "0");
  CHECK(ro::ui::SpelledTradeAmount(5) == "5");
  CHECK(ro::ui::SpelledTradeAmount(1'000'000) == "1 million");
  CHECK(ro::ui::SpelledTradeAmount(1'250'300'005) == "1 milliard 250 millions 300 mille 5");
  CHECK(ro::ui::SpelledTradeAmount(12'000'000) == "12 millions");
}

TEST_CASE("Échoppe : la préparation pose une pile par ligne, bornée", "[ui][vending]") {
  ro::ui::VendingSetupWindowState state;
  ro::ui::OpenVendingSetupOn(state, 3);
  CHECK(state.open);
  CHECK(state.preparation_seen == 3);

  CHECK(ro::ui::PlaceOnVendingSetup(state, 2, 5, 10, 3));
  // Reposer la même pile s'ajoute à sa ligne, jamais au-delà de la pile.
  CHECK(ro::ui::PlaceOnVendingSetup(state, 2, 20, 10, 3));
  REQUIRE(state.lines.size() == 1);
  CHECK(state.lines[0].amount == 10);
  CHECK_FALSE(ro::ui::PlaceOnVendingSetup(state, 2, 1, 10, 3));
  CHECK(ro::ui::PlacedOnVendingSetup(state, 2) == 10);

  CHECK(ro::ui::PlaceOnVendingSetup(state, 3, 1, 1, 3));
  CHECK(ro::ui::PlaceOnVendingSetup(state, 4, 1, 1, 3));
  // Toutes les lignes sont prises.
  CHECK_FALSE(ro::ui::PlaceOnVendingSetup(state, 5, 1, 1, 3));
  CHECK(state.lines.size() == 3);

  ro::ui::TakeFromVendingSetup(state, 3);
  CHECK(state.lines.size() == 2);
  CHECK(ro::ui::PlacedOnVendingSetup(state, 3) == 0);

  SECTION("une préparation neuve vide les lignes mais garde le nom") {
    std::memcpy(state.title.data(), "Potions", 7);
    ro::ui::OpenVendingSetupOn(state, 4);
    CHECK(state.lines.empty());
    CHECK(std::string_view(state.title.data()) == "Potions");
  }
}

TEST_CASE("Échoppe : un prix d'au moins dix millions se confirme", "[ui][vending]") {
  ro::ui::VendingSetupWindowState state;
  ro::ui::PlaceOnVendingSetup(state, 2, 1, 1, 12);
  ro::ui::PlaceOnVendingSetup(state, 3, 1, 1, 12);
  TypePrice(state.lines[0], "9,999,999");
  TypePrice(state.lines[1], "10,000,000");
  const std::vector<std::size_t> high = ro::ui::VendingHighPricedLines(state);
  REQUIRE(high.size() == 1);
  CHECK(high[0] == 1);
  CHECK(ro::ui::VendingLinePrice(state.lines[0]) == 9'999'999);
}

TEST_CASE("Échoppe : la préparation se dessine, retenue par son verdict", "[ui][vending]") {
  ro::ui::UiContext ui;
  const ro::ui::SkinArt art;
  const std::array<ro::ui::VendingStockView, 2> stock = {
      ro::ui::VendingStockView{
          .item = {.item_id = 501, .name = "Red Potion"}, .cart_slot = 2, .amount = 50},
      ro::ui::VendingStockView{.item = {.item_id = 1201, .name = "Knife [3]"},
                               .cart_slot = 3,
                               .amount = 1,
                               .refusal = "Non identifié."}};
  const std::array<std::string_view, 1> blockers = {"Donne un nom au shop."};
  const std::array<std::string_view, 1> faults = {""};
  ro::ui::VendingSetupView view;
  view.stock = stock;
  view.permitted_lines = 5;
  view.title_bytes_left = 79;
  view.blockers = blockers;
  view.line_faults = faults;

  ro::ui::VendingSetupWindowState state;
  ro::ui::OpenVendingSetupOn(state, 1);
  ro::ui::PlaceOnVendingSetup(state, 2, 10, 50, view.permitted_lines);
  TypePrice(state.lines[0], "1,000");

  for (const bool grid : {false, true}) {
    state.grid = grid;
    for (int frame = 0; frame < 2; ++frame) {
      ui.BeginFrame(kViewport, kFrameSeconds);
      const ro::ui::VendingSetupAction drawn = ro::ui::DrawVendingSetupWindow(state, view, art);
      ui.EndFrame();
      // Rien ne part sans geste, et surtout pas une échoppe que le verdict
      // retient.
      CHECK_FALSE(drawn.open_shop);
      CHECK_FALSE(drawn.cancel);
    }
  }
  CHECK(state.open);
  CHECK(state.lines.size() == 1);

  // Baissée — la croix, Échap —, elle rend un abandon.
  state.open = false;
  ui.BeginFrame(kViewport, kFrameSeconds);
  const ro::ui::VendingSetupAction closed = ro::ui::DrawVendingSetupWindow(state, view, art);
  ui.EndFrame();
  CHECK_FALSE(closed.open_shop);
}

TEST_CASE("Échoppe : fermer mon échoppe se confirme, même par la croix", "[ui][vending]") {
  ro::ui::UiContext ui;
  const ro::ui::SkinArt art;
  const std::array<ro::ui::VendingShopLineView, 1> lines = {ro::ui::VendingShopLineView{
      .item = {.item_id = 501, .name = "Red Potion"}, .cart_slot = 2, .amount = 3, .price = 50}};
  ro::ui::VendingShopView view;
  view.title = "Potions";
  view.lines = lines;
  view.earned = 150;

  ro::ui::VendingShopWindowState state;
  state.open = true;
  ui.BeginFrame(kViewport, kFrameSeconds);
  CHECK_FALSE(ro::ui::DrawVendingShopWindow(state, view, art).close_shop);
  ui.EndFrame();

  // 🔴 L'INTERRUPTEUR BAISSÉ — Échap — NE FERME RIEN : il pose la question, et
  // la fenêtre se relève.
  state.open = false;
  for (int frame = 0; frame < 3; ++frame) {
    ui.BeginFrame(kViewport, kFrameSeconds);
    CHECK_FALSE(ro::ui::DrawVendingShopWindow(state, view, art).close_shop);
    ui.EndFrame();
  }
  CHECK(state.open);
  CHECK(state.confirming);
}

TEST_CASE("Échoppe : le journal montre l'heure et l'acheteur", "[ui][vending]") {
  ro::ui::UiContext ui;
  const ro::ui::SkinArt art;
  const std::array<ro::ui::VendingLogLineView, 1> sales = {
      ro::ui::VendingLogLineView{.item = {.item_id = 501, .name = "Red Potion"},
                                 .amount = 2,
                                 .zeny = 100,
                                 .buyer_char_id = 150001,
                                 .sold_at = "31/12 23:59:59"}};
  ro::ui::VendingLogView view;
  view.sales = sales;
  view.total = 100;
  ro::ui::VendingLogWindowState state;
  state.open = true;
  ui.BeginFrame(kViewport, kFrameSeconds);
  CHECK_FALSE(ro::ui::DrawVendingLogWindow(state, view, art).close);
  ui.EndFrame();
  state.open = false;
  ui.BeginFrame(kViewport, kFrameSeconds);
  CHECK_FALSE(ro::ui::DrawVendingLogWindow(state, view, art).close);
  ui.EndFrame();
}

TEST_CASE("Échoppe : le panier d'une visite fusionne et se borne au stock", "[ui][vending]") {
  std::vector<ro::ui::VendingBasketEntry> basket;
  CHECK(ro::ui::AddToVendingBasket(basket, 2, 3, 5));
  CHECK(ro::ui::AddToVendingBasket(basket, 2, 3, 5));
  REQUIRE(basket.size() == 1);
  CHECK(basket[0].amount == 5);
  CHECK_FALSE(ro::ui::AddToVendingBasket(basket, 2, 1, 5));
  CHECK(ro::ui::AddToVendingBasket(basket, 4, 1, 1));

  const std::array<ro::ui::VendingOfferView, 1> offers = {
      ro::ui::VendingOfferView{.shop_slot = 2, .stock = 2, .price = 1'000}};
  CHECK(ro::ui::VendingBasketTotal(basket, offers) == 5'000);
  // La liste revenue après un achat : l'offre 4 est partie, l'offre 2 n'en a
  // plus que deux.
  ro::ui::FitVendingBasket(basket, offers);
  REQUIRE(basket.size() == 1);
  CHECK(basket[0].amount == 2);
}

TEST_CASE("Échoppe : la visite se dessine, et un gros achat se confirme", "[ui][vending]") {
  ro::ui::UiContext ui;
  const ro::ui::SkinArt art;
  const std::array<ro::ui::VendingOfferView, 1> offers = {
      ro::ui::VendingOfferView{.item = {.item_id = 1201, .name = "+7 Knife [3]"},
                               .shop_slot = 2,
                               .stock = 1,
                               .price = 20'000'000}};
  ro::ui::VendingVisitView view;
  view.vendor_name = "Marchand";
  view.title = "Armes";
  view.offers = offers;
  view.zeny = 50'000'000;
  view.basket_total = 20'000'000;

  ro::ui::VendingVisitWindowState state;
  ro::ui::OpenVendingVisitOn(state, 1);
  ro::ui::AddToVendingBasket(state.basket, 2, 1, 1);
  for (int frame = 0; frame < 2; ++frame) {
    ui.BeginFrame(kViewport, kFrameSeconds);
    const ro::ui::VendingVisitAction drawn = ro::ui::DrawVendingVisitWindow(state, view, art);
    ui.EndFrame();
    CHECK(drawn.buy.empty());
    CHECK_FALSE(drawn.close);
  }
  CHECK(state.open);
  CHECK(state.basket.size() == 1);
}

TEST_CASE("Échoppe : la boîte d'avis se vide quand on la ferme", "[ui][vending]") {
  ro::ui::UiContext ui;
  const ro::ui::SkinArt art;
  ro::ui::TradeNoticeState notice;
  notice.text = "L'ouverture a échoué.";
  ui.BeginFrame(kViewport, kFrameSeconds);
  ro::ui::DrawTradeNotice(notice, "Shop###vending_notice_test", art);
  ui.EndFrame();
  CHECK(notice.asked);
  CHECK_FALSE(notice.text.empty());
}

TEST_CASE("Échoppe : Échap pose la question, et « Oui » ferme mon échoppe", "[ui][vending]") {
  ro::ui::UiContext ui;
  const ro::ui::SkinArt art;
  const std::array<ro::ui::VendingShopLineView, 1> lines = {ro::ui::VendingShopLineView{
      .item = {.item_id = 501, .name = "Red Potion"}, .cart_slot = 2, .amount = 3, .price = 50}};
  ro::ui::VendingShopView view;
  view.lines = lines;
  ro::ui::VendingShopWindowState state;
  state.open = false;
  bool closed = false;
  const auto frame = [&] {
    ui.BeginFrame(kViewport, kFrameSeconds);
    closed |= ro::ui::DrawVendingShopWindow(state, view, art).close_shop;
    ui.EndFrame();
  };
  frame();
  frame();
  CHECK_FALSE(closed);
  CHECK(state.confirming);
  // 🔴 LA BOÎTE EST BIEN OUVERTE : un `AskModal` lancé sous une autre identité
  // que celle qui la dessine laisserait le geste attendre sans rien dire.
  REQUIRE(PointLowestLeftControl(frame, "vending_shop_close"));
  Click(frame);
  CHECK(closed);
}

TEST_CASE("Échoppe : un gros achat passe par la boîte, puis part", "[ui][vending]") {
  ro::ui::UiContext ui;
  const ro::ui::SkinArt art;
  const std::array<ro::ui::VendingOfferView, 1> offers = {
      ro::ui::VendingOfferView{.item = {.item_id = 1201, .name = "+7 Knife [3]"},
                               .shop_slot = 2,
                               .stock = 1,
                               .price = 20'000'000}};
  ro::ui::VendingVisitView view;
  view.offers = offers;
  view.zeny = 50'000'000;
  view.basket_total = 20'000'000;
  ro::ui::VendingVisitWindowState state;
  ro::ui::OpenVendingVisitOn(state, 1);
  ro::ui::AddToVendingBasket(state.basket, 2, 1, 1);
  std::vector<ro::ui::VendingBasketEntry> bought;
  const auto frame = [&] {
    ui.BeginFrame(kViewport, kFrameSeconds);
    ro::ui::VendingVisitAction drawn = ro::ui::DrawVendingVisitWindow(state, view, art);
    ui.EndFrame();
    if (!drawn.buy.empty()) {
      bought = std::move(drawn.buy);
    }
  };
  frame();
  frame();
  // « Acheter » est le plus bas à gauche : il ne fait qu'ouvrir la boîte.
  REQUIRE(PointLowestLeftControl(frame, "vending_visit"));
  Click(frame);
  CHECK(bought.empty());
  REQUIRE_FALSE(state.pending.empty());
  REQUIRE(PointLowestLeftControl(frame, "vending_visit_confirm"));
  Click(frame);
  REQUIRE(bought.size() == 1);
  CHECK(bought[0].shop_slot == 2);
  CHECK(state.pending.empty());
}

TEST_CASE("Échoppe : un achat immédiat demandé depuis une ligne ouvre la boîte", "[ui][vending]") {
  ro::ui::UiContext ui;
  const ro::ui::SkinArt art;
  const std::array<ro::ui::VendingOfferView, 1> offers = {
      ro::ui::VendingOfferView{.item = {.item_id = 1201, .name = "+7 Knife [3]"},
                               .shop_slot = 2,
                               .stock = 1,
                               .price = 20'000'000}};
  ro::ui::VendingVisitView view;
  view.offers = offers;
  view.zeny = 50'000'000;
  ro::ui::VendingVisitWindowState state;
  ro::ui::OpenVendingVisitOn(state, 1);
  // Ce que le Ctrl + clic sur « Ajouter » laisse, depuis sa ligne de table.
  state.pending = {{.shop_slot = 2, .amount = 1}};
  state.pending_immediate = true;
  bool immediate = false;
  const auto frame = [&] {
    ui.BeginFrame(kViewport, kFrameSeconds);
    const ro::ui::VendingVisitAction drawn = ro::ui::DrawVendingVisitWindow(state, view, art);
    ui.EndFrame();
    immediate |= !drawn.buy.empty() && drawn.immediate;
  };
  frame();
  frame();
  REQUIRE(WindowNamed("vending_visit_confirm") != nullptr);
  REQUIRE(PointLowestLeftControl(frame, "vending_visit_confirm"));
  Click(frame);
  CHECK(immediate);
}

TEST_CASE("Échoppe : un prix élevé se confirme avant l'ouverture", "[ui][vending]") {
  ro::ui::UiContext ui;
  const ro::ui::SkinArt art;
  const std::array<ro::ui::VendingStockView, 1> stock = {ro::ui::VendingStockView{
      .item = {.item_id = 1201, .name = "+7 Knife [3]"}, .cart_slot = 2, .amount = 1}};
  const std::array<std::string_view, 1> faults = {""};
  ro::ui::VendingSetupView view;
  view.stock = stock;
  view.permitted_lines = 3;
  view.line_faults = faults;
  ro::ui::VendingSetupWindowState state;
  ro::ui::OpenVendingSetupOn(state, 1);
  std::memcpy(state.title.data(), "Armes", 5);
  ro::ui::PlaceOnVendingSetup(state, 2, 1, 1, view.permitted_lines);
  TypePrice(state.lines[0], "20,000,000");
  bool opened = false;
  const auto frame = [&] {
    ui.BeginFrame(kViewport, kFrameSeconds);
    opened |= ro::ui::DrawVendingSetupWindow(state, view, art).open_shop;
    ui.EndFrame();
  };
  frame();
  frame();
  REQUIRE(PointLowestLeftControl(frame, "vending_setup"));
  Click(frame);
  CHECK_FALSE(opened);
  REQUIRE(state.confirming);
  REQUIRE(PointLowestLeftControl(frame, "vending_setup_confirm"));
  Click(frame);
  CHECK(opened);
}
