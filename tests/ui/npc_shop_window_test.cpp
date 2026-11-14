// LA FENÊTRE DE LA BOUTIQUE D'UN PNJ : ce que le panier accepte, les paliers
// qu'une ligne propose, et une fenêtre qui se dessine avec son verdict.
//
// 🔴 UN ÉQUIPEMENT NE S'ACHÈTE QU'À L'UNITÉ : le serveur ramène à un, en
// silence, une ligne qui en demande plus, et facture ce qu'il a ramené. Le
// panier ne doit donc jamais porter plus que ce que la ligne permet.

#include "ro/ui/npc_shop_window.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <string_view>
#include <vector>

#include <imgui.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/trade_widgets.h"
#include "ro/ui/ui_context.h"

using ro::ui::AddToNpcShopCart;
using ro::ui::NpcShopCartLine;
using ro::ui::NpcShopOfferView;
using ro::ui::NpcShopSellView;
using ro::ui::NpcShopTab;
using ro::ui::NpcShopView;
using ro::ui::NpcShopWindowAction;
using ro::ui::NpcShopWindowState;
using ro::ui::TradeStepsFor;

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;

}  // namespace

TEST_CASE("Boutique PNJ : le panier s'arrête à ce que la ligne permet", "[ui][npc_shop]") {
  std::vector<NpcShopCartLine> cart;
  const NpcShopCartLine knife{.item_id = 1201, .amount = 1, .unit_price = 38, .most = 1};
  CHECK(AddToNpcShopCart(cart, knife));
  // Un second couteau n'entre pas : la ligne est pleine.
  CHECK_FALSE(AddToNpcShopCart(cart, knife));
  REQUIRE(cart.size() == 1);
  CHECK(cart[0].amount == 1);

  const NpcShopCartLine potions{.item_id = 501, .amount = 100, .unit_price = 38, .most = 150};
  CHECK(AddToNpcShopCart(cart, potions));
  CHECK(AddToNpcShopCart(cart, potions));
  REQUIRE(cart.size() == 2);
  CHECK(cart[1].amount == 150);
}

TEST_CASE("Boutique PNJ : à la vente, deux piles du même item sont deux lignes", "[ui][npc_shop]") {
  std::vector<NpcShopCartLine> cart;
  AddToNpcShopCart(cart, {.item_id = 909, .client_slot = 2, .amount = 3, .most = 5});
  AddToNpcShopCart(cart, {.item_id = 909, .client_slot = 7, .amount = 3, .most = 5});
  CHECK(cart.size() == 2);
}

TEST_CASE("Boutique PNJ : les paliers d'une ligne", "[ui][npc_shop]") {
  const auto buy = TradeStepsFor(30000, false);
  REQUIRE(buy.count == 4);
  CHECK(buy.values[0] == 1);
  CHECK(buy.values[3] == 1000);

  const auto nine = TradeStepsFor(9, true);
  REQUIRE(nine.count == 2);
  CHECK(nine.values[1] == 9);

  const auto pile = TradeStepsFor(650, true);
  REQUIRE(pile.count == 4);
  CHECK(pile.values[2] == 100);
  CHECK(pile.values[3] == 650);

  const auto one = TradeStepsFor(1, false);
  REQUIRE(one.count == 1);
  CHECK(one.values[0] == 1);
}

TEST_CASE("Boutique PNJ : une ouverture neuve vide le panier et choisit l'onglet",
          "[ui][npc_shop]") {
  NpcShopWindowState state;
  state.cart.push_back({.item_id = 501});
  state.close_after_sale = true;
  ro::ui::OpenNpcShopOn(state, 7, /*buy_first=*/false);
  CHECK(state.open);
  CHECK(state.opening_seen == 7);
  CHECK(state.tab == NpcShopTab::kSell);
  CHECK(state.force_tab);
  CHECK(state.cart.empty());
  CHECK_FALSE(state.close_after_sale);
}

TEST_CASE(
    "Boutique PNJ : vendre tout le sac se confirme si le joueur le veut, un objet précieux "
    "toujours",
    "[ui][npc_shop]") {
  const std::array<NpcShopSellView, 2> sells = {
      NpcShopSellView{.item = {.item_id = 909}, .client_slot = 2, .amount = 40, .price = 1},
      NpcShopSellView{.item = {.item_id = 1201}, .client_slot = 4, .amount = 1, .precious = true}};
  NpcShopView view;
  view.sells = sells;
  const std::array<NpcShopCartLine, 1> jellopies = {
      NpcShopCartLine{.item_id = 909, .client_slot = 2, .amount = 40, .most = 40}};
  const std::array<NpcShopCartLine, 2> everything = {
      jellopies[0], NpcShopCartLine{.item_id = 1201, .client_slot = 4, .amount = 1, .most = 1}};

  NpcShopWindowState state;
  CHECK_FALSE(ro::ui::NpcShopSaleNeedsConfirmation(state, view, jellopies));
  CHECK(ro::ui::NpcShopSaleNeedsConfirmation(state, view, everything));

  state.close_after_sale = true;
  CHECK(ro::ui::NpcShopSaleNeedsConfirmation(state, view, jellopies));

  state.confirm_sell_all = false;
  CHECK_FALSE(ro::ui::NpcShopSaleNeedsConfirmation(state, view, jellopies));
  CHECK(ro::ui::NpcShopSaleNeedsConfirmation(state, view, everything));
}

namespace {

// Une liste de vente comme `app` la compose : deux types, une pièce portée.
// Les favoris verrouillés n'y sont déjà plus.
constexpr std::string_view kEtc = "Divers";
constexpr std::string_view kHealing = "Soins";
constexpr std::string_view kWeapon = "Armes";

const std::array<NpcShopSellView, 5> kMixedSells = {
    NpcShopSellView{
        .item = {.item_id = 909}, .client_slot = 2, .amount = 40, .price = 3, .kind = kEtc},
    NpcShopSellView{
        .item = {.item_id = 501}, .client_slot = 3, .amount = 12, .price = 25, .kind = kHealing},
    NpcShopSellView{.item = {.item_id = 1201},
                    .client_slot = 4,
                    .amount = 1,
                    .price = 25,
                    .kind = kWeapon,
                    .worn = true},
    NpcShopSellView{
        .item = {.item_id = 914}, .client_slot = 5, .amount = 7, .price = 10, .kind = kEtc},
    NpcShopSellView{.item = {.item_id = 1202},
                    .client_slot = 6,
                    .amount = 1,
                    .price = 500,
                    .precious = true,
                    .kind = kWeapon}};

std::vector<NpcShopCartLine> OfKind(const NpcShopView& view, std::string_view kind) {
  std::vector<NpcShopCartLine> kept;
  for (const NpcShopCartLine& line : ro::ui::NpcShopSellableLines(view)) {
    for (const NpcShopSellView& sold : view.sells) {
      if (sold.client_slot == line.client_slot && sold.kind == kind) {
        kept.push_back(line);
      }
    }
  }
  return kept;
}

bool SameLines(const std::vector<NpcShopCartLine>& left,
               const std::vector<NpcShopCartLine>& right) {
  if (left.size() != right.size()) {
    return false;
  }
  for (std::size_t index = 0; index < left.size(); ++index) {
    const NpcShopCartLine& a = left[index];
    const NpcShopCartLine& b = right[index];
    if (a.item_id != b.item_id || a.client_slot != b.client_slot || a.amount != b.amount ||
        a.unit_price != b.unit_price || a.most != b.most) {
      return false;
    }
  }
  return true;
}

}  // namespace

TEST_CASE("Boutique PNJ : les types vendables, leurs lignes et leur recette", "[ui][npc_shop]") {
  NpcShopView view;
  view.sells = kMixedSells;

  // La pièce portée ne compte ni dans « Tout ajouter » ni dans un type.
  const std::vector<NpcShopCartLine> sellable = ro::ui::NpcShopSellableLines(view);
  REQUIRE(sellable.size() == 4);
  CHECK(std::ranges::none_of(sellable,
                             [](const NpcShopCartLine& line) { return line.client_slot == 4; }));

  const std::vector<ro::ui::NpcShopSellKind> kinds = ro::ui::NpcShopSellKinds(view);
  REQUIRE(kinds.size() == 3);
  // Par ordre alphabétique de libellé.
  CHECK(kinds[0].kind == kWeapon);
  CHECK(kinds[0].lines == 1);
  CHECK(kinds[0].total == 500);
  CHECK(kinds[1].kind == kEtc);
  CHECK(kinds[1].lines == 2);
  CHECK(kinds[1].total == 40 * 3 + 7 * 10);
  CHECK(kinds[2].kind == kHealing);
  CHECK(kinds[2].lines == 1);
  CHECK(kinds[2].total == 12 * 25);
}

TEST_CASE("Boutique PNJ : un type dont toutes les lignes sont portées ne paraît pas",
          "[ui][npc_shop]") {
  const std::array<NpcShopSellView, 1> worn_only = {NpcShopSellView{.item = {.item_id = 1201},
                                                                    .client_slot = 4,
                                                                    .amount = 1,
                                                                    .price = 25,
                                                                    .kind = kWeapon,
                                                                    .worn = true}};
  NpcShopView view;
  view.sells = worn_only;
  CHECK(ro::ui::NpcShopSellKinds(view).empty());
  CHECK(ro::ui::NpcShopSellableLines(view).empty());

  std::vector<NpcShopCartLine> cart;
  ro::ui::AddNpcShopSellKind(cart, view, kWeapon);
  CHECK(cart.empty());
}

TEST_CASE("Boutique PNJ : ajouter un type, c'est « Tout ajouter » restreint à ce type",
          "[ui][npc_shop]") {
  NpcShopView view;
  view.sells = kMixedSells;

  for (const std::string_view kind : {kEtc, kHealing, kWeapon}) {
    std::vector<NpcShopCartLine> cart;
    ro::ui::AddNpcShopSellKind(cart, view, kind);
    CHECK(SameLines(cart, OfKind(view, kind)));
  }

  // Deux types l'un après l'autre s'ajoutent ; un second clic ne dépasse pas la
  // pile, et une ligne déjà entamée se complète.
  std::vector<NpcShopCartLine> cart;
  AddToNpcShopCart(cart,
                   {.item_id = 909, .client_slot = 2, .amount = 5, .unit_price = 3, .most = 40});
  ro::ui::AddNpcShopSellKind(cart, view, kEtc);
  ro::ui::AddNpcShopSellKind(cart, view, kEtc);
  ro::ui::AddNpcShopSellKind(cart, view, kHealing);
  REQUIRE(cart.size() == 3);
  CHECK(cart[0].amount == 40);
  CHECK(cart[1].client_slot == 5);
  CHECK(cart[1].amount == 7);
  CHECK(cart[2].client_slot == 3);
  CHECK(cart[2].amount == 12);

  // La vente d'un type qui porte un objet précieux se confirme toujours, et le
  // type seul ne ferme pas la boutique après la vente.
  NpcShopWindowState state;
  ro::ui::AddNpcShopSellKind(state.cart, view, kWeapon);
  CHECK_FALSE(state.close_after_sale);
  CHECK(ro::ui::NpcShopSaleNeedsConfirmation(state, view, state.cart));
}

TEST_CASE("Boutique PNJ : la fenêtre se dessine avec sa liste, son panier et son verdict",
          "[ui][npc_shop]") {
  ro::ui::UiContext ui;
  const ro::ui::SkinArt art;
  const std::array<NpcShopOfferView, 2> offers = {
      NpcShopOfferView{.item = {.item_id = 501, .name = "Red Potion"},
                       .base_price = 50,
                       .price = 38,
                       .most = 30000},
      NpcShopOfferView{.item = {.item_id = 1201, .name = "Knife [3]"},
                       .base_price = 50,
                       .price = 38,
                       .most = 1}};
  const std::array<std::string_view, 1> blockers = {"Zeny insuffisant."};
  NpcShopView view;
  view.title = "Tool Dealer";
  view.purse_line = "Zeny : 10z";
  view.purse = 10;
  view.unit = "z";
  view.offers = offers;
  view.offers_listed = true;
  view.blockers = blockers;
  view.cart_total = 38;

  NpcShopWindowState state;
  ro::ui::OpenNpcShopOn(state, 1, true);
  AddToNpcShopCart(state.cart, {.item_id = 501, .amount = 1, .unit_price = 38, .most = 30000});

  for (int frame = 0; frame < 2; ++frame) {
    ui.BeginFrame(kViewport, kFrameSeconds);
    const NpcShopWindowAction drawn = ro::ui::DrawNpcShopWindow(state, view, art);
    ui.EndFrame();
    CHECK_FALSE(drawn.close);
    // Rien ne part sans geste, et surtout pas un panier que le verdict retient.
    CHECK(drawn.buy.empty());
    CHECK(drawn.sell.empty());
  }
  CHECK(state.open);
  CHECK_FALSE(state.force_tab);

  // Fermée, elle ne dessine rien et ne rend rien.
  state.open = false;
  ui.BeginFrame(kViewport, kFrameSeconds);
  const NpcShopWindowAction closed = ro::ui::DrawNpcShopWindow(state, view, art);
  ui.EndFrame();
  CHECK_FALSE(closed.close);
}

TEST_CASE("Boutique PNJ : l'onglet de vente se dessine, favoris masqués comptés",
          "[ui][npc_shop]") {
  ro::ui::UiContext ui;
  const ro::ui::SkinArt art;
  const std::array<NpcShopSellView, 1> sells = {
      NpcShopSellView{.item = {.item_id = 1201, .name = "+7 Knife [3]", .damaged = true},
                      .client_slot = 4,
                      .amount = 1,
                      .base_price = 25,
                      .price = 30,
                      .precious = true,
                      .kind = "Armes"}};
  NpcShopView view;
  view.can_buy = false;
  view.sells = sells;
  view.sells_listed = true;
  view.hidden_favorites = 2;
  view.unit = "z";
  NpcShopWindowState state;
  ro::ui::OpenNpcShopOn(state, 1, false);
  ui.BeginFrame(kViewport, kFrameSeconds);
  const NpcShopWindowAction drawn = ro::ui::DrawNpcShopWindow(state, view, art);
  ui.EndFrame();
  CHECK_FALSE(drawn.close);
  CHECK(state.tab == NpcShopTab::kSell);
}
