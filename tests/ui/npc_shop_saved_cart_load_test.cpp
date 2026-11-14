// CHARGER UN PANIER ENREGISTRÉ dans la boutique ouverte : ce que la boutique
// vend s'ajoute au panier courant, avec les bornes de sa ligne ; le reste se
// dit.

#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/npc_shop_saved_carts.h"
#include "ro/ui/npc_shop_window.h"

using ro::ui::LoadNpcShopSavedCart;
using ro::ui::NpcShopCartLine;
using ro::ui::NpcShopCartLoad;
using ro::ui::NpcShopOfferView;
using ro::ui::NpcShopSavedCartLineView;
using ro::ui::NpcShopSavedCartView;
using ro::ui::NpcShopView;

namespace {

constexpr std::uint32_t kRedPotionId = 501;
constexpr std::uint32_t kFlyWingId = 601;
constexpr std::uint32_t kKnifeId = 1201;
constexpr std::uint32_t kGemId = 717;

NpcShopOfferView Offer(std::uint32_t item_id, std::string_view name, std::int64_t price, int most) {
  NpcShopOfferView offer;
  offer.item.item_id = item_id;
  offer.item.name = name;
  offer.base_price = price;
  offer.price = price;
  offer.most = most;
  return offer;
}

const std::vector<NpcShopSavedCartLineView> kSavedLines = {
    {.item_id = kRedPotionId, .amount = 100, .item_name = "Red Potion"},
    {.item_id = kFlyWingId, .amount = 20, .item_name = "Fly Wing"},
    {.item_id = kKnifeId, .amount = 3, .item_name = "Knife"},
    {.item_id = kGemId, .amount = 5, .item_name = "Blue Gemstone"}};

const NpcShopSavedCartView kSaved{.name = "Courses", .lines = kSavedLines};

}  // namespace

TEST_CASE("une boutique qui vend tout prend tout le panier, chaque ligne bornée",
          "[npc_shop_saved_cart_load]") {
  const std::vector<NpcShopOfferView> offers = {
      Offer(kRedPotionId, "Red Potion", 50, 30000), Offer(kFlyWingId, "Fly Wing", 60, 30000),
      Offer(kKnifeId, "Knife", 50, 1), Offer(kGemId, "Blue Gemstone", 600, 30000)};
  NpcShopView view;
  view.offers = offers;
  std::vector<NpcShopCartLine> cart;
  const NpcShopCartLoad load = LoadNpcShopSavedCart(cart, view, kSaved);
  CHECK(load.absent.empty());
  CHECK(load.full.empty());
  REQUIRE(cart.size() == 4);
  CHECK(cart[0].amount == 100);
  CHECK(cart[0].unit_price == 50);
  CHECK(cart[1].amount == 20);
  // Un équipement ne s'achète qu'à l'unité : la ligne le borne comme un clic.
  CHECK(cart[2].amount == 1);
  CHECK(cart[2].most == 1);
  CHECK(cart[3].amount == 5);
  CHECK(ro::ui::NpcShopCartLoadNote(load).empty());
}

TEST_CASE("une boutique qui en vend une partie prend sa part et dit le reste",
          "[npc_shop_saved_cart_load]") {
  const std::vector<NpcShopOfferView> offers = {Offer(kRedPotionId, "Red Potion", 50, 30000),
                                                Offer(kFlyWingId, "Fly Wing", 60, 30000)};
  NpcShopView view;
  view.offers = offers;
  std::vector<NpcShopCartLine> cart;
  const NpcShopCartLoad load = LoadNpcShopSavedCart(cart, view, kSaved);
  REQUIRE(cart.size() == 2);
  CHECK(load.absent == std::vector<std::string>{"Knife", "Blue Gemstone"});
  CHECK(ro::ui::NpcShopCartLoadNote(load) ==
        "2 item(s) absent(s) de cette boutique : Knife, Blue Gemstone");
}

TEST_CASE("une boutique qui ne vend rien du panier n'y ajoute rien", "[npc_shop_saved_cart_load]") {
  const std::vector<NpcShopOfferView> offers = {Offer(909, "Jellopy", 3, 30000)};
  NpcShopView view;
  view.offers = offers;
  std::vector<NpcShopCartLine> cart;
  const NpcShopCartLoad load = LoadNpcShopSavedCart(cart, view, kSaved);
  CHECK(cart.empty());
  CHECK(load.absent.size() == kSavedLines.size());
}

TEST_CASE("le chargement se cumule au panier courant, borné à la ligne",
          "[npc_shop_saved_cart_load]") {
  // Il reste 120 places dans la pile de potions du sac.
  const std::vector<NpcShopOfferView> offers = {
      Offer(kRedPotionId, "Red Potion", 50, 120), Offer(kFlyWingId, "Fly Wing", 60, 30000),
      Offer(kKnifeId, "Knife", 50, 1), Offer(kGemId, "Blue Gemstone", 600, 0)};
  NpcShopView view;
  view.offers = offers;
  std::vector<NpcShopCartLine> cart = {
      {.item_id = kRedPotionId, .amount = 40, .unit_price = 50, .most = 120},
      {.item_id = kKnifeId, .amount = 1, .unit_price = 50, .most = 1}};
  const NpcShopCartLoad load = LoadNpcShopSavedCart(cart, view, kSaved);
  REQUIRE(cart.size() == 3);
  CHECK(cart[0].amount == 120);
  CHECK(cart[1].amount == 1);
  CHECK(cart[2].item_id == kFlyWingId);
  CHECK(cart[2].amount == 20);
  // La pile de gemmes est pleine : la ligne n'offre aucun bouton, le
  // chargement n'y met rien non plus.
  CHECK(load.full == std::vector<std::string>{"Blue Gemstone"});
  CHECK(load.absent.empty());
  // Charger deux fois ne dépasse toujours pas les bornes.
  (void)LoadNpcShopSavedCart(cart, view, kSaved);
  CHECK(cart[0].amount == 120);
  CHECK(cart[2].amount == 40);
}

TEST_CASE("un objet absent sans nom enregistré se dit par son identifiant",
          "[npc_shop_saved_cart_load]") {
  const std::vector<NpcShopSavedCartLineView> lines = {{.item_id = 12345, .amount = 1}};
  NpcShopView view;
  std::vector<NpcShopCartLine> cart;
  const NpcShopCartLoad load =
      LoadNpcShopSavedCart(cart, view, NpcShopSavedCartView{.name = "x", .lines = lines});
  CHECK(load.absent == std::vector<std::string>{"#12345"});
}
