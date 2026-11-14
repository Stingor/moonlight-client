// LES FENÊTRES DE L'ÉCHOPPE D'ACHAT : une préparation qui se remplit, une vente
// qui ne dépasse jamais ce que l'acheteur veut, des boutons que le verdict
// retient, et des gestes coûteux qui se confirment.
//
// 🔴 LE SERVEUR REFUSE LA VENTE ENTIÈRE POUR UNE LIGNE : une même case sur deux
// lignes, ou plus que ce qui est voulu, et tout échoue. Le panier de vente ne
// doit donc jamais pouvoir le fabriquer.

#include <array>
#include <cstdio>
#include <functional>
#include <string_view>
#include <vector>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/buying_store_log_window.h"
#include "ro/ui/buying_store_own_window.h"
#include "ro/ui/buying_store_sell_window.h"
#include "ro/ui/buying_store_setup_window.h"
#include "ro/ui/ui_context.h"

using ro::ui::BuyingStoreOfferView;
using ro::ui::BuyingStoreSellState;
using ro::ui::BuyingStoreSetupState;
using ro::ui::BuyingStoreWantedView;

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;

constexpr std::uint32_t kJellopy = 909;
constexpr std::uint32_t kRedPotion = 501;

// Le balayage qui cherche un bouton : le pas, la marge au bord, et les
// abscisses essayées, en fraction de la largeur.
constexpr float kSweepStepPixels = 3.0F;
constexpr float kEdgeMarginPixels = 14.0F;
// ⚠ LA DERNIÈRE VISE LE « OUI » D'UNE BOÎTE : ses deux boutons sont centrés, et
// un libellé court laisse le bord gauche vide.
constexpr std::array<float, 4> kSweepColumns = {0.0F, 0.1F, 0.25F, 0.35F};

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
  // ⚠ PAS CONTRE LE BORD : le bord d'une fenêtre qu'on redimensionne est
  // interactif lui aussi, et le cliquer ne fait que tirer dessus.
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

TEST_CASE("Échoppe d'achat : la préparation se remplit, sans doublon ni dépassement",
          "[ui][buying_store]") {
  BuyingStoreSetupState state;
  state.lines.push_back({.item_id = 42});
  state.zeny_limit = 7;
  std::snprintf(state.title.data(), state.title.size(), "Achète jellopy");
  ro::ui::OpenBuyingStoreSetupOn(state, 3);
  CHECK(state.open);
  CHECK(state.preparation_seen == 3);
  CHECK(state.lines.empty());
  CHECK(state.zeny_limit == 0);
  // LE NOM RESTE d'une échoppe à l'autre.
  CHECK(std::string_view(state.title.data()) == "Achète jellopy");

  CHECK(ro::ui::AddBuyingStoreSetupLine(state, kJellopy, 2));
  CHECK_FALSE(ro::ui::AddBuyingStoreSetupLine(state, kJellopy, 2));
  CHECK(ro::ui::AddBuyingStoreSetupLine(state, kRedPotion, 2));
  CHECK_FALSE(ro::ui::AddBuyingStoreSetupLine(state, 502, 2));
  REQUIRE(state.lines.size() == 2);
  CHECK(state.lines[0].amount == 1);
  CHECK(state.lines[0].price == 0);

  CHECK_FALSE(ro::ui::BuyingStoreSetupHasDearPrice(state));
  state.lines[1].price = static_cast<int>(ro::ui::kBuyingStoreDearPrice);
  CHECK(ro::ui::BuyingStoreSetupHasDearPrice(state));
}

TEST_CASE("Échoppe d'achat : la préparation se dessine, et le verdict retient l'ouverture",
          "[ui][buying_store]") {
  ro::ui::UiContext ui;
  const ro::ui::SkinArt art;
  const std::array<ro::ui::BuyingStoreCandidateView, 1> candidates = {
      ro::ui::BuyingStoreCandidateView{.item = {.item_id = kJellopy, .name = "Jellopy"},
                                       .owned = 12}};
  const std::array<std::string_view, 1> blockers = {"Un item est à prix 0."};
  const std::array<std::string_view, 1> notes = {"Un item est à prix 0."};
  ro::ui::BuyingStoreSetupView view;
  view.slots = 2;
  view.most_amount = 9999;
  view.candidates = candidates;
  view.title_on_wire = "Achète";
  view.title_bytes_left = 73;
  view.zeny = 1000;
  view.blockers = blockers;
  view.line_notes = notes;

  BuyingStoreSetupState state;
  ro::ui::OpenBuyingStoreSetupOn(state, 1);
  ro::ui::AddBuyingStoreSetupLine(state, kJellopy, view.slots);
  for (int frame = 0; frame < 2; ++frame) {
    ui.BeginFrame(kViewport, kFrameSeconds);
    const ro::ui::BuyingStoreSetupAction drawn =
        ro::ui::DrawBuyingStoreSetupWindow(state, view, art);
    ui.EndFrame();
    CHECK_FALSE(drawn.cancel);
    CHECK_FALSE(drawn.open_store);
  }
  CHECK(state.open);
  // ⚠ L'IDENTITÉ « ### » EST CELLE QUE LA PILE D'ÉCHAP CHERCHE.
  CHECK(ImGui::FindWindowByName(ro::ui::kBuyingStoreSetupWindowName) != nullptr);

  // Baissée — la croix, Échap —, elle rend l'abandon.
  state.open = false;
  ui.BeginFrame(kViewport, kFrameSeconds);
  const ro::ui::BuyingStoreSetupAction closed =
      ro::ui::DrawBuyingStoreSetupWindow(state, view, art);
  ui.EndFrame();
  CHECK_FALSE(closed.open_store);
}

TEST_CASE("Échoppe d'achat : la vente ne dépasse ni le voulu, ni la pile, ni une case",
          "[ui][buying_store]") {
  const std::array<BuyingStoreWantedView, 1> wanted = {
      BuyingStoreWantedView{.item = {.item_id = kJellopy}, .wanted = 10, .price = 5}};
  const BuyingStoreOfferView first{.item = {.item_id = kJellopy}, .client_slot = 2, .owned = 6};
  const BuyingStoreOfferView second{.item = {.item_id = kJellopy}, .client_slot = 7, .owned = 30};

  BuyingStoreSellState state;
  CHECK(ro::ui::OpenBuyingStoreSellOn(state, 150000, 9));
  CHECK(ro::ui::BuyingStoreAddable(wanted, state.cart, first) == 6);
  CHECK(ro::ui::AddToBuyingStoreSale(state, wanted, first, 4) == 4);
  // LA MÊME CASE GROSSIT SA LIGNE, bornée à ce que la pile tient encore.
  CHECK(ro::ui::AddToBuyingStoreSale(state, wanted, first, 50) == 2);
  REQUIRE(state.cart.size() == 1);
  CHECK(state.cart[0].amount == 6);
  // L'autre pile ne prend que ce que l'acheteur veut encore.
  CHECK(ro::ui::BuyingStoreAddable(wanted, state.cart, second) == 4);
  CHECK(ro::ui::AddToBuyingStoreSale(state, wanted, second, 30) == 4);
  CHECK(ro::ui::BuyingStoreAddable(wanted, state.cart, second) == 0);
  CHECK(state.cart.size() == 2);

  SECTION("une pile refusée n'ajoute rien") {
    BuyingStoreOfferView carded = second;
    carded.client_slot = 11;
    carded.barred = "Item serti";
    CHECK(ro::ui::AddToBuyingStoreSale(state, wanted, carded, 1) == 0);
  }
  SECTION("une autre ouverture du même acheteur vide la vente") {
    CHECK_FALSE(ro::ui::OpenBuyingStoreSellOn(state, 150000, 9));
    CHECK(state.cart.size() == 2);
    CHECK(ro::ui::OpenBuyingStoreSellOn(state, 150000, 10));
    CHECK(state.cart.empty());
  }
}

TEST_CASE("Échoppe d'achat : la vente se dessine, retenue par son verdict", "[ui][buying_store]") {
  ro::ui::UiContext ui;
  const ro::ui::SkinArt art;
  const std::array<BuyingStoreWantedView, 1> wanted = {BuyingStoreWantedView{
      .item = {.item_id = kJellopy, .name = "Jellopy"}, .wanted = 10, .price = 5}};
  const std::array<BuyingStoreOfferView, 2> offers = {
      BuyingStoreOfferView{.item = {.item_id = kJellopy, .name = "Jellopy"},
                           .client_slot = 2,
                           .owned = 6,
                           .price = 5},
      BuyingStoreOfferView{.item = {.item_id = kJellopy, .name = "Jellopy"},
                           .client_slot = 5,
                           .owned = 1,
                           .price = 5,
                           .barred = "Item serti"}};
  const std::array<std::string_view, 1> blockers = {"L'acheteur n'a pas de quoi tout payer."};
  ro::ui::BuyingStoreSellView view;
  view.buyer = "Acheteur";
  view.store_title = "J'achète";
  view.wanted = wanted;
  view.offers = offers;
  view.buyer_funds = 10;
  view.cart_total = 30;
  view.blockers = blockers;

  BuyingStoreSellState state;
  ro::ui::OpenBuyingStoreSellOn(state, 150000, 9);
  ro::ui::AddToBuyingStoreSale(state, wanted, offers[0], 6);
  for (int frame = 0; frame < 2; ++frame) {
    ui.BeginFrame(kViewport, kFrameSeconds);
    const ro::ui::BuyingStoreSellAction drawn = ro::ui::DrawBuyingStoreSellWindow(state, view, art);
    ui.EndFrame();
    CHECK_FALSE(drawn.close);
    // Rien ne part sans le geste ET sa confirmation.
    CHECK(drawn.sell.empty());
  }
  CHECK(state.pending.empty());
  CHECK(ImGui::FindWindowByName(ro::ui::kBuyingStoreSellWindowName) != nullptr);
}

TEST_CASE("Échoppe d'achat : mon échoppe ne se ferme pas sans confirmation", "[ui][buying_store]") {
  ro::ui::UiContext ui;
  const ro::ui::SkinArt art;
  const std::array<ro::ui::BuyingStoreOwnLineView, 1> lines = {ro::ui::BuyingStoreOwnLineView{
      .item = {.item_id = kJellopy, .name = "Jellopy"}, .wanted = 4, .price = 5}};
  ro::ui::BuyingStoreOwnView view;
  view.store_title = "J'achète";
  view.lines = lines;
  view.zeny_left = 20;
  view.zeny_at_opening = 50;
  view.spent = 30;
  view.purchases = 2;

  ro::ui::BuyingStoreOwnState state;
  ro::ui::OpenBuyingStoreOwnOn(state, 1);
  ui.BeginFrame(kViewport, kFrameSeconds);
  CHECK_FALSE(ro::ui::DrawBuyingStoreOwnWindow(state, view, art).close);
  ui.EndFrame();

  // 🔴 L'INTERRUPTEUR BAISSÉ — la pile d'Échap — NE FERME RIEN : la fenêtre se
  // relève et pose la question.
  state.open = false;
  for (int frame = 0; frame < 2; ++frame) {
    ui.BeginFrame(kViewport, kFrameSeconds);
    const ro::ui::BuyingStoreOwnAction drawn = ro::ui::DrawBuyingStoreOwnWindow(state, view, art);
    ui.EndFrame();
    CHECK_FALSE(drawn.close);
  }
  CHECK(state.open);
  CHECK_FALSE(state.ask_close);
}

TEST_CASE("Échoppe d'achat : l'historique se dessine et se referme", "[ui][buying_store]") {
  ro::ui::UiContext ui;
  const ro::ui::SkinArt art;
  const std::array<ro::ui::BuyingStoreLogLineView, 1> lines = {
      ro::ui::BuyingStoreLogLineView{.item = {.item_id = kJellopy, .name = "Jellopy"},
                                     .clock = "01/10 - 12:30:00",
                                     .seller_char_id = 150001,
                                     .amount = 3,
                                     .zeny = 15}};
  const ro::ui::BuyingStoreLogView view{.lines = lines, .total = 15};
  ro::ui::BuyingStoreLogState state{.open = true};
  ui.BeginFrame(kViewport, kFrameSeconds);
  CHECK_FALSE(ro::ui::DrawBuyingStoreLogWindow(state, view, art).close);
  ui.EndFrame();
  // Baissée par Échap, elle ne dessine plus rien : c'est l'étage qui lit
  // l'interrupteur baissé comme une fermeture.
  state.open = false;
  ui.BeginFrame(kViewport, kFrameSeconds);
  const ro::ui::BuyingStoreLogAction closed = ro::ui::DrawBuyingStoreLogWindow(state, view, art);
  ui.EndFrame();
  CHECK_FALSE(closed.close);
  CHECK(ImGui::FindWindowByName(ro::ui::kBuyingStoreLogWindowName) != nullptr);
}

TEST_CASE("Échoppe d'achat : « Vendre » ouvre le récapitulatif, et seul « oui » vend",
          "[ui][buying_store]") {
  ro::ui::UiContext ui;
  const ro::ui::SkinArt art;
  const std::array<BuyingStoreWantedView, 1> wanted = {BuyingStoreWantedView{
      .item = {.item_id = kJellopy, .name = "Jellopy"}, .wanted = 10, .price = 5}};
  const std::array<BuyingStoreOfferView, 1> offers = {BuyingStoreOfferView{
      .item = {.item_id = kJellopy, .name = "Jellopy"}, .client_slot = 2, .owned = 6, .price = 5}};
  ro::ui::BuyingStoreSellView view;
  view.buyer = "Acheteur";
  view.wanted = wanted;
  view.offers = offers;
  view.buyer_funds = 1000;
  view.cart_total = 30;

  BuyingStoreSellState state;
  ro::ui::OpenBuyingStoreSellOn(state, 150000, 9);
  ro::ui::AddToBuyingStoreSale(state, wanted, offers[0], 6);
  std::vector<ro::ui::BuyingStoreSaleCartLine> sold;
  const std::function<void()> frame = [&] {
    ui.BeginFrame(kViewport, kFrameSeconds);
    ro::ui::BuyingStoreSellAction action = ro::ui::DrawBuyingStoreSellWindow(state, view, art);
    ui.EndFrame();
    if (!action.sell.empty()) {
      sold = std::move(action.sell);
    }
  };
  frame();
  frame();

  REQUIRE(PointLowestLeftControl(frame, ro::ui::kBuyingStoreSellWindowName));
  Click(frame);
  // 🔴 LE CLIC NE VEND PAS : il ouvre la question, qui doit s'être OUVERTE —
  // une boîte demandée ailleurs que là où elle se dessine ne paraît jamais.
  CHECK(sold.empty());
  CHECK(state.pending.size() == 1);
  REQUIRE(WindowNamed("buying_store_sale_confirm") != nullptr);

  REQUIRE(PointLowestLeftControl(frame, "buying_store_sale_confirm"));
  Click(frame);
  REQUIRE(sold.size() == 1);
  CHECK(sold[0].client_slot == 2);
  CHECK(sold[0].amount == 6);
  CHECK(state.pending.empty());
}

TEST_CASE("Échoppe d'achat : fermer mon échoppe passe par la question", "[ui][buying_store]") {
  ro::ui::UiContext ui;
  const ro::ui::SkinArt art;
  ro::ui::BuyingStoreOwnView view;
  view.zeny_left = 20;
  ro::ui::BuyingStoreOwnState state;
  ro::ui::OpenBuyingStoreOwnOn(state, 1);
  bool closed = false;
  const std::function<void()> frame = [&] {
    ui.BeginFrame(kViewport, kFrameSeconds);
    closed |= ro::ui::DrawBuyingStoreOwnWindow(state, view, art).close;
    ui.EndFrame();
  };
  frame();
  frame();

  REQUIRE(PointLowestLeftControl(frame, ro::ui::kBuyingStoreOwnWindowName));
  Click(frame);
  CHECK_FALSE(closed);
  REQUIRE(WindowNamed("buying_store_own_close") != nullptr);

  REQUIRE(PointLowestLeftControl(frame, "buying_store_own_close"));
  Click(frame);
  CHECK(closed);
}

TEST_CASE("Échoppe d'achat : un prix élevé se confirme avant d'ouvrir", "[ui][buying_store]") {
  ro::ui::UiContext ui;
  const ro::ui::SkinArt art;
  ro::ui::BuyingStoreSetupView view;
  view.slots = 2;
  view.most_amount = 9999;
  view.title_bytes_left = 70;
  BuyingStoreSetupState state;
  ro::ui::OpenBuyingStoreSetupOn(state, 1);
  ro::ui::AddBuyingStoreSetupLine(state, kJellopy, view.slots);
  state.lines[0].price = static_cast<int>(ro::ui::kBuyingStoreDearPrice);
  bool opened = false;
  const std::function<void()> frame = [&] {
    ui.BeginFrame(kViewport, kFrameSeconds);
    opened |= ro::ui::DrawBuyingStoreSetupWindow(state, view, art).open_store;
    ui.EndFrame();
  };
  frame();
  frame();

  REQUIRE(PointLowestLeftControl(frame, ro::ui::kBuyingStoreSetupWindowName));
  Click(frame);
  CHECK_FALSE(opened);
  REQUIRE(WindowNamed("buying_store_dear_confirm") != nullptr);

  REQUIRE(PointLowestLeftControl(frame, "buying_store_dear_confirm"));
  Click(frame);
  CHECK(opened);
}
