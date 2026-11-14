// LE TROC VU PAR LA FENÊTRE : ce que dit chaque résultat et chaque refus, jusqu'où
// un ajout reste possible, et la vue composée d'un troc simple puis étendu.
//
// Ce banc porte ce que la fenêtre montre : le compte de chaque devise, en vert
// s'il suffit ; le bouton retenu avec sa raison ; les phrases du jeu au résultat.

#include "game/barter_view.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/barter.h"
#include "ro/sim/inventory.h"
#include "ro/sim/player_status.h"
#include "ro/vfs/item_words.h"
#include "ro/vfs/trade_words.h"

using moonclient::BarterLooks;
using moonclient::BarterMostAddable;
using moonclient::BarterRefusalText;
using moonclient::BarterResultText;
using moonclient::BarterScene;
using moonclient::BarterViewComposer;
using moonclient::SayingOfBarterResult;
using ro::sim::Barter;
using ro::sim::BarterCartRefusal;
using ro::sim::BarterCurrency;
using ro::sim::BarterGeneration;
using ro::sim::BarterItem;
using ro::sim::Inventory;
using ro::sim::InventoryItem;
using ro::sim::PlayerStatus;
using ro::vfs::ItemWords;
using ro::vfs::TradeWord;

namespace {

constexpr std::uint16_t kEtcType = 3;
constexpr std::uint16_t kHealingType = 0;
constexpr std::uint16_t kWeaponType = 5;

constexpr std::uint32_t kHerbIndex = 1;    // une potion, contre dix gelées
constexpr std::uint32_t kSwordIndex = 2;   // une épée +7, contre deux dagues +4 et 1 000z
constexpr std::uint32_t kPotionIndex = 3;  // une potion rare, deux en stock
constexpr std::uint32_t kHerbId = 501;
constexpr std::uint32_t kSwordId = 1101;
constexpr std::uint32_t kPotionId = 502;
constexpr std::uint32_t kJellopyId = 909;
constexpr std::uint32_t kKnifeId = 1201;
constexpr std::uint32_t kSwordRefine = 7;
constexpr std::uint16_t kKnifeRefine = 4;
constexpr std::uint32_t kSwordZeny = 1000;
constexpr std::uint32_t kPotionStock = 2;

constexpr std::int64_t kRoomyWeight = 100000;
constexpr std::int64_t kRich = 1000000;

[[nodiscard]] std::vector<BarterItem> SimpleShop() {
  BarterItem herb{.item_id = kHerbId,
                  .type = kHealingType,
                  .stock = ro::sim::kBarterUnlimitedStock,
                  .weight = 70,
                  .shop_index = kHerbIndex};
  herb.currencies.push_back(BarterCurrency{.item_id = kJellopyId, .amount = 10});
  BarterItem potion{.item_id = kPotionId,
                    .type = kHealingType,
                    .stock = kPotionStock,
                    .weight = 100,
                    .shop_index = kPotionIndex};
  potion.currencies.push_back(BarterCurrency{.item_id = kJellopyId, .amount = 1});
  return {herb, potion};
}

[[nodiscard]] std::vector<BarterItem> ExpandedShop() {
  std::vector<BarterItem> items = SimpleShop();
  BarterItem sword{.item_id = kSwordId,
                   .type = kWeaponType,
                   .stock = 3,
                   .weight = 500,
                   .shop_index = kSwordIndex,
                   .zeny = kSwordZeny,
                   .refine_level = kSwordRefine};
  sword.currencies.push_back(BarterCurrency{
      .item_id = kKnifeId, .refine_level = kKnifeRefine, .amount = 2, .type = kWeaponType});
  sword.currencies.push_back(BarterCurrency{.item_id = kJellopyId, .amount = 5, .type = kEtcType});
  items.push_back(sword);
  return items;
}

[[nodiscard]] PlayerStatus Carrying(std::int64_t weight, std::int64_t max_weight,
                                    std::int64_t zeny) {
  PlayerStatus status;
  status.weight = weight;
  status.max_weight = max_weight;
  status.zeny = zeny;
  return status;
}

[[nodiscard]] InventoryItem Jellopies(std::uint16_t slot, std::int16_t amount) {
  return InventoryItem{.client_slot = slot,
                       .item_id = kJellopyId,
                       .type = kEtcType,
                       .amount = amount,
                       .identified = true};
}

[[nodiscard]] InventoryItem Knife(std::uint16_t slot, std::uint8_t refine) {
  InventoryItem knife{.client_slot = slot, .item_id = kKnifeId, .type = kWeaponType, .amount = 1};
  knife.refine_level = refine;
  knife.identified = true;
  return knife;
}

// Les noms d'essai : l'identifiant, et le raffinement devant.
[[nodiscard]] BarterLooks TestLooks() {
  return BarterLooks{
      .name_of =
          [](std::uint32_t item_id, std::uint16_t refine_level) {
            std::string name = "#" + std::to_string(item_id);
            return refine_level == 0 ? name : "+" + std::to_string(refine_level) + " " + name;
          },
      .icon_of = [](std::uint32_t) { return ro::ui::SkinImage{}; }};
}

void Say(ItemWords& words, TradeWord word, const char* text) {
  words.trade.texts[static_cast<std::size_t>(word)] = text;
}

}  // namespace

// ── LES MOTS ────────────────────────────────────────────────────────────────

TEST_CASE("Troc : chaque code du résultat a sa phrase et sa couleur", "[app][barter]") {
  // 🔴 LE SERVEUR RÉPOND ZÉRO AU SUCCÈS D'UN TROC ; douze existe et vaut aussi.
  CHECK(SayingOfBarterResult(0).word == TradeWord::kDealSucceeded);
  CHECK(SayingOfBarterResult(0).succeeded);
  CHECK(SayingOfBarterResult(12).word == TradeWord::kExchangeSucceeded);
  CHECK(SayingOfBarterResult(12).succeeded);
  CHECK(SayingOfBarterResult(11).word == TradeWord::kExchangeFailed);
  CHECK(SayingOfBarterResult(13).word == TradeWord::kExchangeOutOfStock);
  CHECK(SayingOfBarterResult(14).word == TradeWord::kExchangeLacksGoods);
  CHECK(SayingOfBarterResult(1).word == TradeWord::kLacksZeny);
  CHECK(SayingOfBarterResult(2).word == TradeWord::kOverWeight);
  CHECK(SayingOfBarterResult(3).word == TradeWord::kOverCapacity);
  for (const std::uint8_t failed : std::array<std::uint8_t, 6>{1, 2, 3, 11, 13, 14}) {
    CHECK_FALSE(SayingOfBarterResult(failed).succeeded);
  }
  CHECK_FALSE(SayingOfBarterResult(42).word.has_value());
  CHECK_FALSE(SayingOfBarterResult(42).succeeded);
}

TEST_CASE("Troc : le résultat parle avec les mots du jeu, et jamais ne se tait", "[app][barter]") {
  ItemWords words;
  Say(words, TradeWord::kExchangeLacksGoods, "Not enough items to exchange.");
  CHECK(BarterResultText(14, words) == "Not enough items to exchange.");
  // Un code sans mot à lui : nos mots, pas le silence — des items
  // sont peut-être partis.
  CHECK_FALSE(BarterResultText(0, words).empty());
  CHECK(BarterResultText(11, words).find("11") != std::string::npos);
}

TEST_CASE("Troc : un refus dit par les mots du jeu, ou par les nôtres qui nomment l'item",
          "[app][barter]") {
  ItemWords words;
  // Les gardes du client d'origine, avec ses phrases.
  Say(words, TradeWord::kBarterEquipped, "Equipped items cannot be exchanged.");
  CHECK(BarterRefusalText(BarterCartRefusal::kCurrencyWorn, "Knife", words) ==
        "Equipped items cannot be exchanged.");
  Say(words, TradeWord::kLimitBuyItem, "No more than %d at a time.");
  CHECK(BarterRefusalText(BarterCartRefusal::kOverLineLimit, "", words) ==
        "No more than 30000 at a time.");
  // Ce que le client d'origine ne disait pas avant le clic : nos mots, l'item nommé.
  CHECK(
      BarterRefusalText(BarterCartRefusal::kNotEnoughCurrency, "Jellopy", words).find("Jellopy") !=
      std::string::npos);
  CHECK(BarterRefusalText(BarterCartRefusal::kCurrencyFavorite, "Jellopy", words).find("Jellopy") !=
        std::string::npos);
  // Sans la phrase du jeu, jamais de texte vide.
  const ItemWords bare;
  for (const BarterCartRefusal refusal :
       {BarterCartRefusal::kNotOpen, BarterCartRefusal::kEmptyCart,
        BarterCartRefusal::kTooManyLines, BarterCartRefusal::kUnknownItem,
        BarterCartRefusal::kZeroAmount, BarterCartRefusal::kSameItemTwice,
        BarterCartRefusal::kGearAlreadyInCart, BarterCartRefusal::kGearOneAtATime,
        BarterCartRefusal::kOverLineLimit, BarterCartRefusal::kOutOfStock,
        BarterCartRefusal::kOverWeight, BarterCartRefusal::kCurrencyWorn,
        BarterCartRefusal::kCurrencyFavorite, BarterCartRefusal::kNotEnoughCurrency,
        BarterCartRefusal::kNotEnoughZeny}) {
    CHECK_FALSE(BarterRefusalText(refusal, "X", bare).empty());
  }
  CHECK(BarterRefusalText(BarterCartRefusal::kNone, "X", bare).empty());
}

// ── JUSQU'OÙ UN AJOUT RESTE POSSIBLE ────────────────────────────────────────

TEST_CASE("Troc : l'ajout le plus grand s'arrête à ce que les devises paient", "[app][barter]") {
  Barter barter;
  barter.TellOpening(BarterGeneration::kSimple, SimpleShop());
  Inventory bag;
  bag.Put(Jellopies(2, 35));
  const PlayerStatus status = Carrying(0, kRoomyWeight, kRich);
  const BarterItem& herb = *barter.FindItem(kHerbIndex);

  CHECK(BarterMostAddable(barter, herb, bag, status) == 3);
  // Le panier en tient déjà un : dix gelées de moins.
  REQUIRE(barter.PutInCart(kHerbIndex, 1));
  CHECK(BarterMostAddable(barter, herb, bag, status) == 2);

  SECTION("le poids borne aussi") {
    // Soixante-dix dixièmes par potion, une déjà au panier.
    const PlayerStatus tight = Carrying(0, 150, kRich);
    CHECK(BarterMostAddable(barter, herb, bag, tight) == 1);
  }
  SECTION("et le stock du PNJ") {
    const BarterItem& potion = *barter.FindItem(kPotionIndex);
    CHECK(BarterMostAddable(barter, potion, bag, status) == kPotionStock);
  }
  SECTION("sans devise, rien") {
    Inventory empty;
    CHECK(BarterMostAddable(barter, herb, empty, status) == 0);
  }
}

TEST_CASE("Troc : un équipement se prend un à la fois, et une fois", "[app][barter]") {
  Barter barter;
  barter.TellOpening(BarterGeneration::kExpanded, ExpandedShop());
  Inventory bag;
  bag.Put(Jellopies(2, 35));
  bag.Put(Knife(3, 4));
  bag.Put(Knife(4, 5));
  const BarterItem& sword = *barter.FindItem(kSwordIndex);

  CHECK(BarterMostAddable(barter, sword, bag, Carrying(0, kRoomyWeight, kRich)) == 1);
  // Le zeny de l'étendu borne aussi.
  CHECK(BarterMostAddable(barter, sword, bag, Carrying(0, kRoomyWeight, kSwordZeny - 1)) == 0);
  REQUIRE(barter.PutInCart(kSwordIndex, 1));
  CHECK(BarterMostAddable(barter, sword, bag, Carrying(0, kRoomyWeight, kRich)) == 0);
}

// ── LA VUE ──────────────────────────────────────────────────────────────────

TEST_CASE("Troc simple : chaque devise dit ce que le joueur en tient", "[app][barter]") {
  Barter barter;
  barter.TellOpening(BarterGeneration::kSimple, SimpleShop());
  Inventory bag;
  bag.Put(Jellopies(2, 35));
  // ⚠ LES FAVORIS NE COMPTENT PAS : l'échange ne les prend pas.
  InventoryItem favorite = Jellopies(3, 50);
  favorite.favorite = true;
  bag.Put(favorite);
  const PlayerStatus status = Carrying(0, kRoomyWeight, kRich);
  ItemWords words;
  Say(words, TradeWord::kBarterList, "Items to trade");
  const BarterLooks looks = TestLooks();
  BarterViewComposer composer;

  const ro::ui::BarterView& view = composer.Compose(BarterScene{
      .barter = barter, .inventory = bag, .status = status, .words = words, .looks = looks});
  CHECK_FALSE(view.expanded);
  CHECK(view.list_title == "Items to trade");
  CHECK(view.title.empty());
  // Un troc simple ne dépense pas de zeny : il ne le montre pas.
  CHECK(view.purse_line.empty());
  REQUIRE(view.offers.size() == 2);
  const ro::ui::BarterOfferView& herb = view.offers[0];
  CHECK(herb.item.name == "#501");
  CHECK(herb.stock == ro::ui::kBarterNoStockLimit);
  CHECK(herb.weight_tenths == 70);
  CHECK(herb.most == 3);
  CHECK(herb.refusal.empty());
  REQUIRE(herb.prices.size() == 1);
  CHECK(herb.prices[0].item.name == "#909");
  CHECK(herb.prices[0].amount == 10);
  CHECK(herb.prices[0].held == 35);
  CHECK(view.offers[1].stock == kPotionStock);
  CHECK(view.cart.empty());
  CHECK(view.blockers.empty());
}

TEST_CASE("Troc étendu : raffinements, zeny, et le refus dit avant le clic", "[app][barter]") {
  Barter barter;
  barter.TellOpening(BarterGeneration::kExpanded, ExpandedShop());
  Inventory bag;
  bag.Put(Jellopies(2, 35));
  // Une seule dague au raffinement exigé : il en faut deux.
  bag.Put(Knife(3, kKnifeRefine));
  bag.Put(Knife(4, 2));
  const PlayerStatus status = Carrying(0, kRoomyWeight, 2500);
  const ItemWords words;
  const BarterLooks looks = TestLooks();
  BarterViewComposer composer;

  const ro::ui::BarterView* view = &composer.Compose(BarterScene{
      .barter = barter, .inventory = bag, .status = status, .words = words, .looks = looks});
  CHECK(view->expanded);
  CHECK(view->purse_line.find("2,500z") != std::string_view::npos);
  REQUIRE(view->offers.size() == 3);
  const ro::ui::BarterOfferView* sword = &view->offers[2];
  // L'article porte son raffinement ; la devise, le sien.
  CHECK(sword->item.name == "+7 #1101");
  CHECK(sword->zeny == kSwordZeny);
  REQUIRE(sword->prices.size() == 2);
  CHECK(sword->prices[0].item.name == "+4 #1201");
  CHECK(sword->prices[0].refine_level == kKnifeRefine);
  // La dague +2 ne compte pas : exigée +4 ou plus.
  CHECK(sword->prices[0].held == 1);
  CHECK(sword->prices[1].held == 35);
  // 🔴 RIEN À AJOUTER, ET LA RAISON NOMME LA DEVISE.
  CHECK(sword->most == 0);
  CHECK(sword->refusal.find("#1201") != std::string_view::npos);

  SECTION("une dague de plus, raffinée au-delà : l'ajout s'ouvre, et le risque se dit") {
    bag.Put(Knife(5, 9));
    view = &composer.Compose(BarterScene{
        .barter = barter, .inventory = bag, .status = status, .words = words, .looks = looks});
    sword = &view->offers[2];
    CHECK(sword->most == 1);
    CHECK(sword->refusal.empty());
    CHECK(sword->prices[0].held == 2);
    CHECK(sword->prices[0].may_take_higher_refine);
  }
}

TEST_CASE("Troc : le panier montre ses bornes, son poids, et le verdict qui le retient",
          "[app][barter]") {
  Barter barter;
  barter.TellOpening(BarterGeneration::kSimple, SimpleShop());
  Inventory bag;
  bag.Put(Jellopies(2, 35));
  const PlayerStatus status = Carrying(0, kRoomyWeight, kRich);
  const ItemWords words;
  const BarterLooks looks = TestLooks();
  BarterViewComposer composer;
  REQUIRE(barter.PutInCart(kHerbIndex, 2));

  const ro::ui::BarterView* view = &composer.Compose(BarterScene{
      .barter = barter, .inventory = bag, .status = status, .words = words, .looks = looks});
  REQUIRE(view->cart.size() == 1);
  CHECK(view->cart[0].amount == 2);
  // Deux au panier, et de quoi en payer une de plus.
  CHECK(view->cart[0].most == 3);
  CHECK(view->cart_seq == barter.cart_seq());
  CHECK(view->cart_weight.find("14") != std::string_view::npos);
  CHECK(view->blockers.empty());

  // Des gelées sont parties entre-temps : le panier ne passerait plus.
  REQUIRE(bag.Take(2, 20));
  view = &composer.Compose(BarterScene{
      .barter = barter, .inventory = bag, .status = status, .words = words, .looks = looks});
  REQUIRE(view->blockers.size() == 1);
  CHECK(view->blockers[0].find("#909") != std::string_view::npos);
  CHECK(view->offers[0].most == 0);
}

TEST_CASE("Troc : trop lourd se dit avec la charge, devises retranchées", "[app][barter]") {
  Barter barter;
  barter.TellOpening(BarterGeneration::kSimple, SimpleShop());
  Inventory bag;
  bag.Put(Jellopies(2, 35));
  const ItemWords words;
  const BarterLooks looks = TestLooks();
  BarterViewComposer composer;
  // Une gelée pèse un. La potion rare (+10) ne coûte qu'une gelée (-1) : net +9.
  static constexpr std::uint32_t kJellopyTenths = 10;
  const ro::sim::BarterUnitWeight weighed = [](std::uint32_t item_id) {
    return item_id == kJellopyId ? std::optional<std::uint32_t>(kJellopyTenths) : std::nullopt;
  };
  REQUIRE(barter.PutInCart(kPotionIndex, 2));

  // 🔴 LE PANIER RETENU DIT DE COMBIEN : 995 + 2 × 90 dixièmes = 117.5 pour 100.
  const PlayerStatus heavy = Carrying(995, 1000, kRich);
  const ro::ui::BarterView* view = &composer.Compose(BarterScene{.barter = barter,
                                                                 .inventory = bag,
                                                                 .status = heavy,
                                                                 .words = words,
                                                                 .looks = looks,
                                                                 .unit_weight = weighed});
  REQUIRE(view->blockers.size() == 1);
  const std::string_view said = view->blockers[0];
  CHECK(said.starts_with("Trop lourd : "));
  CHECK(said.find("117.5") != std::string_view::npos);
  CHECK(said.find("100") != std::string_view::npos);

  // 🔴 LES GELÉES QUI PARTENT FONT PASSER CE QUE LEUR POIDS BRUT RETENAIT : 20
  // de plus brut, 18 net, sur 19 de marge.
  const PlayerStatus tight = Carrying(810, 1000, kRich);
  view = &composer.Compose(BarterScene{
      .barter = barter, .inventory = bag, .status = tight, .words = words, .looks = looks});
  CHECK(view->blockers.size() == 1);
  view = &composer.Compose(BarterScene{.barter = barter,
                                       .inventory = bag,
                                       .status = tight,
                                       .words = words,
                                       .looks = looks,
                                       .unit_weight = weighed});
  CHECK(view->blockers.empty());
}
