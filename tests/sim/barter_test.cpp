// LE TROC, SANS LE FIL : l'ouverture, le panier, l'achat qui ferme, les
// fermetures dues — et chaque raison de refus du panier.
//
// Ce banc porte ce qu'une session montrerait le plus tard :
//
//   · l'achat FERME le troc et c'est l'attente du résultat, pas l'ouverture, qui
//     le revendique ;
//   · un changement de carte n'a RIEN à dire au serveur ;
//   · les devises se comptent cumulées sur tout le panier, sans ce que le joueur
//     porte ni ses favoris, à la première pile pour ce qui s'empile, au
//     raffinement exigé ou plus haut pour ce qui ne s'empile pas.

#include "ro/sim/barter.h"

#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/inventory.h"
#include "ro/sim/player_status.h"

using ro::sim::Barter;
using ro::sim::BarterCartRefusal;
using ro::sim::BarterCurrency;
using ro::sim::BarterGeneration;
using ro::sim::BarterItem;
using ro::sim::CheckBarterAddition;
using ro::sim::CheckBarterCart;
using ro::sim::CheckBarterCartShape;
using ro::sim::Inventory;
using ro::sim::InventoryItem;
using ro::sim::PlayerStatus;

namespace {

// Les familles du fil : ce qui s'empile, et ce qui ne s'empile pas.
constexpr std::uint16_t kEtcType = 3;
constexpr std::uint16_t kHealingType = 0;
constexpr std::uint16_t kWeaponType = 5;

// La boutique d'essai.
constexpr std::uint32_t kHerbIndex = 1;    // une potion, contre dix gelées
constexpr std::uint32_t kSwordIndex = 2;   // une épée, contre deux dagues +4 ou mieux
constexpr std::uint32_t kPotionIndex = 3;  // une autre potion, contre cinq gelées
constexpr std::uint32_t kHerbId = 501;
constexpr std::uint32_t kSwordId = 1101;
constexpr std::uint32_t kPotionId = 502;
constexpr std::uint32_t kJellopyId = 909;
constexpr std::uint32_t kKnifeId = 1201;

[[nodiscard]] std::vector<BarterItem> Shop() {
  BarterItem herb{.item_id = kHerbId,
                  .type = kHealingType,
                  .stock = ro::sim::kBarterUnlimitedStock,
                  .weight = 70,
                  .shop_index = kHerbIndex};
  herb.currencies.push_back(BarterCurrency{.item_id = kJellopyId, .amount = 10, .type = kEtcType});
  BarterItem sword{.item_id = kSwordId,
                   .type = kWeaponType,
                   .stock = 3,
                   .weight = 500,
                   .shop_index = kSwordIndex,
                   .zeny = 1000};
  sword.currencies.push_back(
      BarterCurrency{.item_id = kKnifeId, .refine_level = 4, .amount = 2, .type = kWeaponType});
  BarterItem potion{.item_id = kPotionId,
                    .type = kHealingType,
                    .stock = 40,
                    .weight = 100,
                    .shop_index = kPotionIndex};
  potion.currencies.push_back(BarterCurrency{.item_id = kJellopyId, .amount = 5, .type = kEtcType});
  return {herb, sword, potion};
}

[[nodiscard]] Barter OpenShop(BarterGeneration generation = BarterGeneration::kExpanded) {
  Barter barter;
  barter.TellOpening(generation, Shop());
  return barter;
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

}  // namespace

// ── LE MODÈLE ───────────────────────────────────────────────────────────────

TEST_CASE("Troc : la liste ouvre, retient la génération et vide le panier", "[sim][barter]") {
  Barter barter;
  CHECK_FALSE(barter.open());
  barter.TellOpening(BarterGeneration::kSimple, Shop());
  CHECK(barter.open());
  CHECK(barter.generation() == BarterGeneration::kSimple);
  CHECK(barter.opening_seq() == 1);
  REQUIRE(barter.items().size() == 3);
  REQUIRE(barter.FindItem(kSwordIndex) != nullptr);
  CHECK(barter.FindItem(kSwordIndex)->item_id == kSwordId);
  CHECK(barter.FindItem(99) == nullptr);

  REQUIRE(barter.PutInCart(kHerbIndex, 2));
  barter.TellOpening(BarterGeneration::kSimple, Shop());
  CHECK(barter.cart().empty());
  CHECK(barter.opening_seq() == 2);
}

TEST_CASE("Troc : le panier fusionne par article et se retire à zéro", "[sim][barter]") {
  Barter barter = OpenShop();
  const std::uint32_t before = barter.cart_seq();
  REQUIRE(barter.PutInCart(kPotionIndex, 2));
  REQUIRE(barter.PutInCart(kPotionIndex, 3));
  REQUIRE(barter.cart().size() == 1);
  CHECK(barter.InCart(kPotionIndex) == 5);
  CHECK(barter.cart_seq() > before);

  // Le stock montré est celui du PNJ moins le panier.
  CHECK(barter.RemainingStock(*barter.FindItem(kPotionIndex)) == 35);
  CHECK(barter.RemainingStock(*barter.FindItem(kHerbIndex)) == ro::sim::kBarterUnlimitedStock);

  CHECK_FALSE(barter.PutInCart(99, 1));
  CHECK_FALSE(barter.PutInCart(kHerbIndex, 0));
  CHECK_FALSE(barter.SetCartAmount(kHerbIndex, 4));

  REQUIRE(barter.SetCartAmount(kPotionIndex, 0));
  CHECK(barter.cart().empty());

  barter.Close();
  CHECK_FALSE(barter.PutInCart(kHerbIndex, 1));
}

TEST_CASE("Troc : l'achat ferme, dit au revoir, et attend son résultat", "[sim][barter]") {
  Barter barter = OpenShop(BarterGeneration::kSimple);
  REQUIRE(barter.PutInCart(kHerbIndex, 2));
  barter.MarkPurchaseSent();

  CHECK_FALSE(barter.open());
  CHECK(barter.awaiting_result());
  CHECK(barter.cart().empty());
  REQUIRE(barter.sent_cart().size() == 1);
  CHECK(barter.sent_cart()[0].amount == 2);
  CHECK(barter.TakeFarewell() == BarterGeneration::kSimple);
  CHECK_FALSE(barter.TakeFarewell().has_value());

  barter.TellResult(ro::sim::kBarterResultSucceeded);
  CHECK_FALSE(barter.awaiting_result());
  CHECK(barter.result_seq() == 1);
  CHECK(ro::sim::BarterResultSucceeded(barter.result()));
  // Les articles restent lisibles pour le message.
  CHECK(barter.FindItem(kHerbIndex) != nullptr);

  // Un second envoi, troc fermé, n'attend rien.
  barter.MarkPurchaseSent();
  CHECK_FALSE(barter.awaiting_result());
}

TEST_CASE("Troc : les codes de succès sont zéro et douze", "[sim][barter]") {
  CHECK(ro::sim::BarterResultSucceeded(0));
  CHECK(ro::sim::BarterResultSucceeded(12));
  CHECK_FALSE(ro::sim::BarterResultSucceeded(11));
  CHECK_FALSE(ro::sim::BarterResultSucceeded(13));
  CHECK_FALSE(ro::sim::BarterResultSucceeded(14));
  CHECK_FALSE(ro::sim::BarterResultSucceeded(1));
}

TEST_CASE("Troc : chaque fermeture du joueur se dit, une fois, dans sa génération",
          "[sim][barter]") {
  Barter barter = OpenShop(BarterGeneration::kExpanded);
  barter.Close();
  CHECK(barter.TakeFarewell() == BarterGeneration::kExpanded);
  CHECK_FALSE(barter.TakeFarewell().has_value());
  // Fermer un troc déjà fermé ne doit rien de plus.
  barter.Close();
  CHECK_FALSE(barter.TakeFarewell().has_value());
}

TEST_CASE("Troc : une liste de l'autre génération doit la fermeture de la première",
          "[sim][barter]") {
  Barter barter = OpenShop(BarterGeneration::kSimple);
  barter.TellOpening(BarterGeneration::kExpanded, Shop());
  CHECK(barter.open());
  CHECK(barter.generation() == BarterGeneration::kExpanded);
  CHECK(barter.TakeFarewell() == BarterGeneration::kSimple);
  CHECK_FALSE(barter.TakeFarewell().has_value());
}

TEST_CASE("Troc : les deux fermetures peuvent être dues ensemble", "[sim][barter]") {
  Barter barter = OpenShop(BarterGeneration::kSimple);
  barter.TellOpening(BarterGeneration::kExpanded, Shop());
  barter.Close();
  CHECK(barter.TakeFarewell() == BarterGeneration::kSimple);
  CHECK(barter.TakeFarewell() == BarterGeneration::kExpanded);
  CHECK_FALSE(barter.TakeFarewell().has_value());
}

TEST_CASE("Troc : un changement de carte ou de serveur n'a rien à dire", "[sim][barter]") {
  Barter barter = OpenShop(BarterGeneration::kSimple);
  REQUIRE(barter.PutInCart(kHerbIndex, 1));
  barter.MarkPurchaseSent();
  REQUIRE(barter.awaiting_result());

  barter.CloseForMapChange();
  CHECK_FALSE(barter.open());
  CHECK_FALSE(barter.awaiting_result());
  CHECK_FALSE(barter.TakeFarewell().has_value());

  barter.TellOpening(BarterGeneration::kExpanded, Shop());
  barter.CloseSilently();
  CHECK_FALSE(barter.open());
  CHECK_FALSE(barter.TakeFarewell().has_value());
}

// ── LA FORME : ce que le serveur ignore sans répondre ──────────────────────

TEST_CASE("Troc : la forme du panier", "[sim][barter]") {
  Barter closed;
  CHECK(CheckBarterCartShape(closed).refusal == BarterCartRefusal::kNotOpen);

  Barter barter = OpenShop();
  CHECK(CheckBarterCartShape(barter).refusal == BarterCartRefusal::kEmptyCart);
  REQUIRE(barter.PutInCart(kHerbIndex, 1));
  CHECK(CheckBarterCartShape(barter).passed());

  SECTION("le même objet sous deux index") {
    std::vector<BarterItem> items = Shop();
    items[2].item_id = kHerbId;
    barter.TellOpening(BarterGeneration::kExpanded, std::move(items));
    REQUIRE(barter.PutInCart(kHerbIndex, 1));
    REQUIRE(barter.PutInCart(kPotionIndex, 1));
    const auto check = CheckBarterCartShape(barter);
    CHECK(check.refusal == BarterCartRefusal::kSameItemTwice);
    CHECK(check.cart_line == 1);
    CHECK(check.item_id == kHerbId);
  }
}

TEST_CASE("Troc : le plafond de trame, en lignes, selon la génération", "[sim][barter]") {
  CHECK(ro::sim::kBarterMostSimpleLines == 146);
  CHECK(ro::sim::kBarterMostExpandedLines == 170);

  const auto fill = [](BarterGeneration generation, std::uint32_t lines) {
    std::vector<BarterItem> items;
    for (std::uint32_t index = 0; index < lines; ++index) {
      BarterItem item{.item_id = 10000 + index,
                      .type = kEtcType,
                      .stock = ro::sim::kBarterUnlimitedStock,
                      .shop_index = index};
      items.push_back(std::move(item));
    }
    Barter barter;
    barter.TellOpening(generation, std::move(items));
    for (std::uint32_t index = 0; index < lines; ++index) {
      REQUIRE(barter.PutInCart(index, 1));
    }
    return barter;
  };
  CHECK(CheckBarterCartShape(fill(BarterGeneration::kSimple, 146)).passed());
  CHECK(CheckBarterCartShape(fill(BarterGeneration::kSimple, 147)).refusal ==
        BarterCartRefusal::kTooManyLines);
  CHECK(CheckBarterCartShape(fill(BarterGeneration::kExpanded, 170)).passed());
  CHECK(CheckBarterCartShape(fill(BarterGeneration::kExpanded, 171)).refusal ==
        BarterCartRefusal::kTooManyLines);
}

// ── LES GARDES DU CLIENT D'ORIGINE ─────────────────────────────────────────

TEST_CASE("Troc : l'ajout d'un article inconnu ou nul", "[sim][barter]") {
  const Barter barter = OpenShop();
  Inventory bag;
  const PlayerStatus status = Carrying(0, 10000, 0);
  CHECK(CheckBarterAddition(barter, 99, 1, bag, status).refusal == BarterCartRefusal::kUnknownItem);
  CHECK(CheckBarterAddition(barter, kHerbIndex, 0, bag, status).refusal ==
        BarterCartRefusal::kZeroAmount);
}

TEST_CASE("Troc : un équipement se prend un par un, et une seule fois", "[sim][barter]") {
  Barter barter = OpenShop();
  Inventory bag;
  bag.Put(Knife(2, 4));
  bag.Put(Knife(3, 4));
  bag.Put(Knife(4, 4));
  bag.Put(Knife(5, 4));
  const PlayerStatus status = Carrying(0, 10000, 5000);

  CHECK(CheckBarterAddition(barter, kSwordIndex, 2, bag, status).refusal ==
        BarterCartRefusal::kGearOneAtATime);
  CHECK(CheckBarterAddition(barter, kSwordIndex, 1, bag, status).passed());
  REQUIRE(barter.PutInCart(kSwordIndex, 1));
  CHECK(CheckBarterAddition(barter, kSwordIndex, 1, bag, status).refusal ==
        BarterCartRefusal::kGearAlreadyInCart);
  CHECK(CheckBarterCart(barter, bag, status).passed());
}

TEST_CASE("Troc : une ligne ne dépasse pas trente mille", "[sim][barter]") {
  Barter barter = OpenShop();
  Inventory bag;
  bag.Put(Jellopies(2, 30000));
  const PlayerStatus status = Carrying(0, 100'000'000, 0);
  REQUIRE(barter.PutInCart(kHerbIndex, 29'999));
  CHECK(CheckBarterAddition(barter, kHerbIndex, 1, bag, status).refusal !=
        BarterCartRefusal::kOverLineLimit);
  const auto check = CheckBarterAddition(barter, kHerbIndex, 2, bag, status);
  CHECK(check.refusal == BarterCartRefusal::kOverLineLimit);
  CHECK(check.item_id == kHerbId);
}

TEST_CASE("Troc : le stock du PNJ borne le panier", "[sim][barter]") {
  Barter barter = OpenShop();
  Inventory bag;
  bag.Put(Jellopies(2, 1000));
  const PlayerStatus status = Carrying(0, 100'000, 0);
  REQUIRE(barter.PutInCart(kPotionIndex, 40));
  CHECK(CheckBarterCart(barter, bag, status).passed());
  CHECK(CheckBarterAddition(barter, kPotionIndex, 1, bag, status).refusal ==
        BarterCartRefusal::kOutOfStock);
}

TEST_CASE("Troc : le poids se compte charge plus panier, en dixièmes", "[sim][barter]") {
  Barter barter = OpenShop();
  Inventory bag;
  bag.Put(Jellopies(2, 1000));
  // 70 dixièmes par potion : dix potions pèsent 700, la charge en laisse 700.
  const PlayerStatus status = Carrying(9300, 10000, 0);
  CHECK(CheckBarterAddition(barter, kHerbIndex, 10, bag, status).passed());
  const auto check = CheckBarterAddition(barter, kHerbIndex, 11, bag, status);
  CHECK(check.refusal == BarterCartRefusal::kOverWeight);
  CHECK(check.item_id == kHerbId);

  SECTION("un poids inconnu ne refuse rien") {
    CHECK(CheckBarterAddition(barter, kHerbIndex, 11, bag, PlayerStatus{}).passed());
  }
}

TEST_CASE("Troc : les devises qui partent se retranchent du poids", "[sim][barter]") {
  Barter barter = OpenShop();
  Inventory bag;
  bag.Put(Jellopies(2, 1000));
  // Une gelée pèse un : dix dixièmes.
  static constexpr std::uint32_t kJellopyTenths = 10;
  const ro::sim::BarterUnitWeight weighed = [](std::uint32_t item_id) {
    return item_id == kJellopyId ? std::optional<std::uint32_t>(kJellopyTenths) : std::nullopt;
  };

  SECTION("un échange plus léger que ce qu'il coûte passe à la limite") {
    // Onze potions : +770, mais cent dix gelées partent, -1 100.
    const PlayerStatus status = Carrying(9300, 10000, 0);
    CHECK(CheckBarterAddition(barter, kHerbIndex, 11, bag, status).refusal ==
          BarterCartRefusal::kOverWeight);
    CHECK(CheckBarterAddition(barter, kHerbIndex, 11, bag, status, weighed).passed());
  }
  SECTION("à la quantité payée, et le refus dit ses chiffres") {
    // Une potion rare : +100, cinq gelées partent, -50. Net : +50 par unité.
    const PlayerStatus status = Carrying(9900, 10000, 0);
    CHECK(CheckBarterAddition(barter, kPotionIndex, 2, bag, status, weighed).passed());
    const auto check = CheckBarterAddition(barter, kPotionIndex, 3, bag, status, weighed);
    CHECK(check.refusal == BarterCartRefusal::kOverWeight);
    CHECK(check.item_id == kPotionId);
    CHECK(check.load_after_tenths == 10050);
    CHECK(check.max_load_tenths == 10000);
  }
  SECTION("le panier se juge d'un bloc, comme au serveur") {
    // La potion rare seule déborde (+50) ; la potion qui suit soulage (-30).
    const PlayerStatus status = Carrying(9960, 10000, 0);
    REQUIRE(barter.PutInCart(kPotionIndex, 1));
    CHECK_FALSE(CheckBarterCart(barter, bag, status, weighed).passed());
    REQUIRE(barter.PutInCart(kHerbIndex, 1));
    CHECK(CheckBarterCart(barter, bag, status, weighed).passed());
  }
  SECTION("une devise que la table ne pèse pas ne retranche rien") {
    const ro::sim::BarterUnitWeight unknown = [](std::uint32_t) {
      return std::optional<std::uint32_t>{};
    };
    const PlayerStatus status = Carrying(9300, 10000, 0);
    const auto check = CheckBarterAddition(barter, kHerbIndex, 11, bag, status, unknown);
    CHECK(check.refusal == BarterCartRefusal::kOverWeight);
    CHECK(check.load_after_tenths == 10070);
  }
}

// ── LES DEVISES ─────────────────────────────────────────────────────────────

TEST_CASE("Troc : les besoins de devise se cumulent sur tout le panier", "[sim][barter]") {
  Barter barter = OpenShop();
  Inventory bag;
  bag.Put(Jellopies(2, 50));
  const PlayerStatus status = Carrying(0, 100'000, 0);
  // Trois potions : trente gelées. Quatre autres : vingt. Cinquante en tout.
  REQUIRE(barter.PutInCart(kHerbIndex, 3));
  REQUIRE(barter.PutInCart(kPotionIndex, 4));
  CHECK(CheckBarterCart(barter, bag, status).passed());
  const auto check = CheckBarterAddition(barter, kPotionIndex, 1, bag, status);
  CHECK(check.refusal == BarterCartRefusal::kNotEnoughCurrency);
  CHECK(check.item_id == kJellopyId);
  CHECK(check.cart_line == 0);
}

TEST_CASE("Troc : une devise absente du sac manque", "[sim][barter]") {
  Barter barter = OpenShop();
  REQUIRE(barter.PutInCart(kHerbIndex, 1));
  const auto check = CheckBarterCart(barter, Inventory{}, Carrying(0, 10000, 0));
  CHECK(check.refusal == BarterCartRefusal::kNotEnoughCurrency);
  CHECK(check.item_id == kJellopyId);
}

TEST_CASE("Troc : une devise qui s'empile se prend dans la première pile seulement",
          "[sim][barter]") {
  Barter barter = OpenShop();
  Inventory bag;
  // Deux piles de six : douze gelées, mais la première seule compte.
  bag.Put(Jellopies(2, 6));
  bag.Put(Jellopies(3, 6));
  REQUIRE(barter.PutInCart(kPotionIndex, 1));
  CHECK(CheckBarterCart(barter, bag, Carrying(0, 10000, 0)).passed());
  REQUIRE(barter.SetCartAmount(kPotionIndex, 2));
  CHECK(CheckBarterCart(barter, bag, Carrying(0, 10000, 0)).refusal ==
        BarterCartRefusal::kNotEnoughCurrency);
}

TEST_CASE("Troc : une devise qui ne s'empile pas vaut au raffinement exigé ou plus haut",
          "[sim][barter]") {
  Barter barter = OpenShop();
  REQUIRE(barter.PutInCart(kSwordIndex, 1));
  const PlayerStatus status = Carrying(0, 10000, 5000);

  Inventory low;
  low.Put(Knife(2, 3));
  low.Put(Knife(3, 9));
  CHECK(CheckBarterCart(barter, low, status).refusal == BarterCartRefusal::kNotEnoughCurrency);

  Inventory high;
  high.Put(Knife(2, 4));
  high.Put(Knife(3, 9));
  CHECK(CheckBarterCart(barter, high, status).passed());
}

TEST_CASE("Troc : les seuils de raffinement s'emboîtent", "[sim][barter]") {
  // Deux articles qui prennent la même dague : l'un à +4 au moins, l'autre à
  // n'importe quel raffinement.
  std::vector<BarterItem> items = Shop();
  BarterItem any_knife{.item_id = 1102,
                       .type = kWeaponType,
                       .stock = ro::sim::kBarterUnlimitedStock,
                       .shop_index = 4};
  any_knife.currencies.push_back(
      BarterCurrency{.item_id = kKnifeId, .amount = 1, .type = kWeaponType});
  items.push_back(any_knife);
  Barter barter;
  barter.TellOpening(BarterGeneration::kExpanded, std::move(items));
  REQUIRE(barter.PutInCart(kSwordIndex, 1));
  REQUIRE(barter.PutInCart(4, 1));
  const PlayerStatus status = Carrying(0, 10000, 5000);

  // Trois dagues dont deux à +4 : l'épée prend les deux, l'autre la troisième.
  Inventory enough;
  enough.Put(Knife(2, 0));
  enough.Put(Knife(3, 4));
  enough.Put(Knife(4, 5));
  CHECK(CheckBarterCart(barter, enough, status).passed());

  // Trois dagues dont une seule à +4 : le total suffit, le seuil non.
  Inventory short_high;
  short_high.Put(Knife(2, 0));
  short_high.Put(Knife(3, 0));
  short_high.Put(Knife(4, 5));
  CHECK(CheckBarterCart(barter, short_high, status).refusal ==
        BarterCartRefusal::kNotEnoughCurrency);
}

TEST_CASE("Troc : une devise portée ou favorite ne compte pas, et on dit pourquoi",
          "[sim][barter]") {
  Barter barter = OpenShop();
  REQUIRE(barter.PutInCart(kSwordIndex, 1));
  const PlayerStatus status = Carrying(0, 10000, 5000);

  Inventory worn;
  worn.Put(Knife(2, 4));
  InventoryItem held = Knife(3, 4);
  held.worn_at = 2;
  worn.Put(held);
  CHECK(CheckBarterCart(barter, worn, status).refusal == BarterCartRefusal::kCurrencyWorn);

  Inventory favored;
  favored.Put(Knife(2, 4));
  InventoryItem kept = Knife(3, 4);
  kept.favorite = true;
  favored.Put(kept);
  CHECK(CheckBarterCart(barter, favored, status).refusal == BarterCartRefusal::kCurrencyFavorite);
}

TEST_CASE("Troc étendu : le zeny du panier se compte à part", "[sim][barter]") {
  Barter barter = OpenShop();
  Inventory bag;
  bag.Put(Knife(2, 4));
  bag.Put(Knife(3, 4));
  REQUIRE(barter.PutInCart(kSwordIndex, 1));
  CHECK(CheckBarterCart(barter, bag, Carrying(0, 10000, 1000)).passed());
  const auto check = CheckBarterCart(barter, bag, Carrying(0, 10000, 999));
  CHECK(check.refusal == BarterCartRefusal::kNotEnoughZeny);
  CHECK(check.item_id == kSwordId);
  // Un zeny inconnu ne refuse rien.
  PlayerStatus unknown = Carrying(0, 10000, 0);
  unknown.zeny = ro::sim::kUnknownStat;
  CHECK(CheckBarterCart(barter, bag, unknown).passed());
}

TEST_CASE("Troc : ce qu'on tient d'une devise se compte comme le panier la compte",
          "[sim][barter]") {
  Inventory bag;
  // Ce qui s'empile : la PREMIÈRE pile, hors favoris.
  InventoryItem favorite = Jellopies(2, 50);
  favorite.favorite = true;
  bag.Put(favorite);
  bag.Put(Jellopies(3, 12));
  bag.Put(Jellopies(4, 30));
  const ro::sim::BarterCurrencyHolding jellopies =
      ro::sim::HoldBarterCurrency(bag, kJellopyId, ro::sim::kBarterAnyRefine);
  CHECK(jellopies.usable == 12);
  CHECK_FALSE(jellopies.may_take_higher_refine);

  // Ce qui ne s'empile pas : au raffinement exigé ou plus haut, hors ce qu'on
  // porte.
  bag.Put(Knife(5, 4));
  bag.Put(Knife(6, 2));
  bag.Put(Knife(7, 9));
  InventoryItem worn = Knife(8, 10);
  worn.worn_at = 2;
  bag.Put(worn);
  const ro::sim::BarterCurrencyHolding exact = ro::sim::HoldBarterCurrency(bag, kKnifeId, 4);
  CHECK(exact.usable == 2);
  // 🔴 LA DAGUE +9 PEUT PARTIR pour une exigence de +4.
  CHECK(exact.may_take_higher_refine);
  CHECK(ro::sim::HoldBarterCurrency(bag, kKnifeId, ro::sim::kBarterAnyRefine).usable == 3);
  CHECK(ro::sim::HoldBarterCurrency(bag, kKnifeId, 10).usable == 0);
  CHECK(ro::sim::HoldBarterCurrency(bag, kSwordId, ro::sim::kBarterAnyRefine).usable == 0);
}

TEST_CASE("Troc : une devise non identifiée ne compte pas", "[sim][barter]") {
  Inventory bag;
  InventoryItem unknown = Jellopies(2, 50);
  unknown.identified = false;
  bag.Put(unknown);
  CHECK(ro::sim::HoldBarterCurrency(bag, kJellopyId, ro::sim::kBarterAnyRefine).usable == 0);

  Barter barter = OpenShop();
  REQUIRE(barter.PutInCart(kHerbIndex, 1));
  CHECK(CheckBarterCart(barter, bag, Carrying(0, 10000, 0)).refusal ==
        BarterCartRefusal::kNotEnoughCurrency);
}

TEST_CASE("Troc : les cases du sac se comptent avant que les devises partent", "[sim][barter]") {
  Inventory bag;
  bag.Put(Jellopies(2, 10));
  // Une seule case libre : les gelées qui partent n'en libèrent aucune.
  bag.SetCapacity(2);
  Barter barter = OpenShop();
  REQUIRE(barter.PutInCart(kHerbIndex, 1));
  CHECK(CheckBarterCart(barter, bag, Carrying(0, 10000, 0)).passed());

  // Une pile déjà au sac ne demande pas de case.
  InventoryItem herbs{
      .client_slot = 3, .item_id = kHerbId, .type = kHealingType, .amount = 5, .identified = true};
  bag.Put(herbs);
  CHECK(CheckBarterCart(barter, bag, Carrying(0, 10000, 0)).passed());

  // Le sac plein, un second article sans pile ne tient plus.
  bag.Put(Jellopies(4, 40));
  REQUIRE(barter.PutInCart(kPotionIndex, 1));
  const ro::sim::BarterCartCheck full = CheckBarterCart(barter, bag, Carrying(0, 10000, 0));
  CHECK(full.refusal == BarterCartRefusal::kBagFull);
  CHECK(full.item_id == kPotionId);

  // Une capacité que le serveur n'a pas dite ne refuse rien.
  bag.SetCapacity(0);
  CHECK_FALSE(CheckBarterCart(barter, bag, Carrying(0, 10000, 0)).refusal ==
              BarterCartRefusal::kBagFull);
}

TEST_CASE("Troc : la pile au sac reste sous le plafond du serveur", "[sim][barter]") {
  Inventory bag;
  bag.Put(Jellopies(2, 30000));
  InventoryItem herbs{.client_slot = 3,
                      .item_id = kHerbId,
                      .type = kHealingType,
                      .amount = 29999,
                      .identified = true};
  bag.Put(herbs);
  Barter barter = OpenShop();
  REQUIRE(barter.PutInCart(kHerbIndex, 1));
  CHECK(CheckBarterCart(barter, bag, Carrying(0, 10000, 0)).passed());
  CHECK(CheckBarterAddition(barter, kHerbIndex, 1, bag, Carrying(0, 10000, 0)).refusal ==
        BarterCartRefusal::kStackOverLimit);
}
