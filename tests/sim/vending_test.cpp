// L'ÉCHOPPE DE VENTE, côté modèle : ses transitions et ses contrôles.
//
// 🔴 CE BANC PORTE CE QUE LE SERVEUR NE DIT PAS :
//
//   · le journal survit à la fermeture, et ne s'efface qu'à l'ouverture suivante ;
//   · une liste revenue vide après un achat clôt la visite ;
//   · un changement de carte abandonne la préparation EN LE DISANT, et garde
//     l'échoppe ouverte ;
//   · les contrôles nomment ce que le serveur rabattrait ou ignorerait en silence.

#include "ro/sim/vending.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using ro::sim::CheckVendingBasket;
using ro::sim::CheckVendingSetup;
using ro::sim::FitVendingTitle;
using ro::sim::InventoryItem;
using ro::sim::MergeVendingBasket;
using ro::sim::Vending;
using ro::sim::VendingBasketFault;
using ro::sim::VendingBasketLine;
using ro::sim::VendingLine;
using ro::sim::VendingSetupFault;
using ro::sim::VendingSetupLine;
using ro::sim::VendingSetupRefusal;
using ro::sim::VendingVisitEnd;

namespace {

VendingLine Line(std::uint16_t slot, std::uint32_t item_id, std::int16_t amount,
                 std::uint32_t price) {
  VendingLine line;
  line.item.client_slot = slot;
  line.item.item_id = item_id;
  line.item.amount = amount;
  line.price_zeny = price;
  return line;
}

InventoryItem Carted(std::uint16_t slot, std::int16_t amount) {
  InventoryItem item;
  item.client_slot = slot;
  item.item_id = 512;
  item.amount = amount;
  item.identified = true;
  return item;
}

bool Has(const std::vector<VendingSetupRefusal>& refusals, VendingSetupFault fault,
         std::size_t line) {
  for (const VendingSetupRefusal& refusal : refusals) {
    if (refusal.fault == fault && refusal.line == line) {
      return true;
    }
  }
  return false;
}

}  // namespace

TEST_CASE("Échoppe : la préparation, la demande, puis l'échoppe ouverte", "[sim][vending]") {
  Vending vending;
  vending.TellPreparation(14);
  CHECK(vending.preparing());
  CHECK(vending.offered_lines() == 14);
  CHECK(vending.permitted_lines() == ro::sim::kVendingMostLines);
  CHECK(vending.CanSendOpening());

  vending.NoteOpeningSent("Pommes");
  CHECK(vending.opening_pending());
  CHECK_FALSE(vending.CanSendOpening());

  vending.TellShopOpened(2000001, {Line(4, 512, 10, 50)});
  CHECK_FALSE(vending.preparing());
  CHECK_FALSE(vending.opening_pending());
  CHECK(vending.shop_open());
  CHECK(vending.shop_title() == "Pommes");
  CHECK(vending.shop_owner_aid() == 2000001);
  REQUIRE(vending.shop_lines().size() == 1);

  vending.TellOpeningResult(ro::sim::kVendingOpenSucceeded);
  CHECK(vending.shop_open());
  CHECK(vending.opening_result_seq() == 1);
}

TEST_CASE("Échoppe : un échec d'ouverture clôt la préparation", "[sim][vending]") {
  Vending vending;
  vending.TellPreparation(5);
  vending.NoteOpeningSent("x");
  vending.TellOpeningResult(ro::sim::kVendingOpenFailed);
  CHECK_FALSE(vending.preparing());
  CHECK_FALSE(vending.opening_pending());
  CHECK_FALSE(vending.shop_open());
  CHECK(vending.opening_result() == ro::sim::kVendingOpenFailed);
  CHECK_FALSE(vending.TakeFarewell());
}

TEST_CASE("Échoppe : les ventes décroissent les lignes et s'inscrivent au journal",
          "[sim][vending]") {
  Vending vending;
  vending.TellPreparation(3);
  vending.NoteOpeningSent("t");
  vending.TellShopOpened(1, {Line(4, 512, 10, 50), Line(6, 607, 1, 2'000'000'000)});

  vending.TellSale(4, 3, 150001, 1'700'000'000, 150);
  REQUIRE(vending.shop_lines().size() == 2);
  CHECK(vending.shop_lines()[0].item.amount == 7);
  REQUIRE(vending.journal().size() == 1);
  CHECK(vending.journal()[0].item.item_id == 512);
  CHECK(vending.journal()[0].item.amount == 3);
  CHECK(vending.journal()[0].buyer_char_id == 150001);
  CHECK(vending.journal()[0].sold_at_unix_s == 1'700'000'000);

  vending.TellSale(6, 1, 150002, 1'700'000'100, 2'000'000'000);
  vending.TellSale(4, 7, 150002, 1'700'000'200, 350);
  CHECK(vending.sold_out());
  // Au-delà de 32 bits sans déborder.
  CHECK(vending.earned_zeny() == 2'000'000'500);

  SECTION("le journal survit à la fermeture") {
    vending.CloseShop();
    CHECK_FALSE(vending.shop_open());
    CHECK(vending.journal().size() == 3);
  }
  SECTION("et s'efface à l'ouverture suivante") {
    vending.CloseShop();
    vending.TellPreparation(3);
    vending.NoteOpeningSent("u");
    vending.TellShopOpened(1, {Line(4, 512, 1, 1)});
    CHECK(vending.journal().empty());
    CHECK(vending.earned_zeny() == 0);
  }
  SECTION("une vente d'une case inconnue reste comptée") {
    vending.TellSale(99, 2, 7, 8, 40);
    CHECK(vending.journal().back().item.item_id == 0);
    CHECK(vending.journal().back().item.client_slot == 99);
    CHECK(vending.earned_zeny() == 2'000'000'540);
  }
}

TEST_CASE("Échoppe : l'abandon se dit, une fois", "[sim][vending]") {
  Vending vending;
  vending.TellPreparation(3);
  vending.CancelPreparation();
  CHECK_FALSE(vending.preparing());
  CHECK(vending.TakeFarewell());
  CHECK_FALSE(vending.TakeFarewell());
}

TEST_CASE("Échoppe : le changement de carte abandonne la préparation et garde l'échoppe",
          "[sim][vending]") {
  Vending vending;
  vending.TellShopOpened(1, {Line(4, 512, 1, 1)});
  vending.CloseForMapChange();
  CHECK(vending.shop_open());
  CHECK_FALSE(vending.TakeFarewell());

  vending.CloseShop();
  vending.TellPreparation(3);
  vending.CloseForMapChange();
  CHECK_FALSE(vending.preparing());
  CHECK(vending.TakeFarewell());

  SECTION("le changement de serveur ferme tout sans rien dire") {
    vending.TellPreparation(3);
    vending.TellShopOpened(1, {Line(4, 512, 1, 1)});
    vending.TellSale(4, 1, 2, 3, 1);
    vending.CloseSilently();
    CHECK_FALSE(vending.shop_open());
    CHECK_FALSE(vending.TakeFarewell());
    CHECK(vending.journal().size() == 1);
  }
}

TEST_CASE("Échoppe visitée : ouverture, rafraîchissement, fin", "[sim][vending]") {
  Vending vending;
  vending.TellVisit(2000005, 77, {Line(2, 512, 5, 10), Line(3, 607, 1, 900)});
  CHECK(vending.visiting());
  CHECK(vending.vendor_aid() == 2000005);
  CHECK(vending.vender_id() == 77);
  CHECK(vending.visit_opening_seq() == 1);

  SECTION("un achat puis la liste qui revient : même visite") {
    vending.NotePurchaseSent();
    CHECK(vending.purchase_pending());
    vending.TellVisit(2000005, 77, {Line(2, 512, 3, 10)});
    CHECK_FALSE(vending.purchase_pending());
    CHECK(vending.visit_opening_seq() == 1);
    CHECK(vending.offers().size() == 1);
  }
  SECTION("la liste revenue vide après un achat clôt la visite") {
    vending.NotePurchaseSent();
    vending.TellVisit(2000005, 77, {});
    CHECK_FALSE(vending.visiting());
    CHECK(vending.visit_end() == VendingVisitEnd::kEmptied);
  }
  SECTION("fermée pendant qu'une liste est en route, elle ne se rouvre pas") {
    vending.NotePurchaseSent();
    vending.CloseVisit();
    vending.TellVisit(2000005, 77, {Line(2, 512, 3, 10)});
    CHECK_FALSE(vending.visiting());
    CHECK(vending.visit_end() == VendingVisitEnd::kClosed);
    // La suivante, demandée de nouveau, rouvre.
    vending.TellVisit(2000005, 77, {Line(2, 512, 3, 10)});
    CHECK(vending.visiting());
    CHECK(vending.visit_opening_seq() == 2);
  }
  SECTION("l'enseigne du vendeur disparaît") {
    vending.TellVendorGone(123);
    CHECK(vending.visiting());
    vending.TellVendorGone(2000005);
    CHECK_FALSE(vending.visiting());
    CHECK(vending.visit_end() == VendingVisitEnd::kVendorGone);
  }
  SECTION("parti pendant un achat, il rouvre plus tard au premier clic") {
    // 🔴 LE DERNIER LOT D'UNE ÉCHOPPE EN AUTOTRADE LA FERME : le serveur ne
    // répondra pas à la liste demandée après l'achat. La garder pour « en
    // route » avalait la première liste de sa prochaine échoppe.
    vending.NotePurchaseSent();
    vending.TellVendorGone(2000005);
    vending.TellVisit(2000005, 79, {Line(2, 512, 1, 10)});
    CHECK(vending.visiting());
    CHECK(vending.visit_opening_seq() == 2);
  }
  SECTION("un autre vendeur : visite neuve") {
    vending.TellVisit(2000009, 78, {Line(2, 512, 1, 10)});
    CHECK(vending.visit_opening_seq() == 2);
  }
  SECTION("un refus porte son rang") {
    vending.TellPurchaseRefused(2, 5, ro::sim::kVendingBuyNoZeny);
    CHECK(vending.purchase_result() == ro::sim::kVendingBuyNoZeny);
    CHECK(vending.purchase_result_slot() == 2);
    CHECK(vending.purchase_result_amount() == 5);
    CHECK(vending.purchase_result_seq() == 1);
  }
  SECTION("le changement de carte clôt la visite") {
    vending.CloseForMapChange();
    CHECK_FALSE(vending.visiting());
    CHECK(vending.visit_end() == VendingVisitEnd::kLeftMap);
  }
}

TEST_CASE("Échoppe : le titre se coupe entre deux caractères", "[sim][vending]") {
  CHECK(FitVendingTitle("court") == "court");
  const std::string long_ascii(100, 'a');
  CHECK(FitVendingTitle(long_ascii).size() == ro::sim::kVendingTitleMostTextBytes);
  // Un « é » tient en un octet sur le fil (1252) : soixante-dix-neuf passent.
  std::string accents;
  for (int count = 0; count < 90; ++count) {
    accents += "\xC3\xA9";
  }
  CHECK(FitVendingTitle(accents).size() == 2 * ro::sim::kVendingTitleMostTextBytes);
  // Un emoji fait partir le titre en UTF-8, quatre octets chacun : dix-neuf
  // tiennent, et la coupe ne tombe jamais au milieu.
  std::string emoji;
  for (int count = 0; count < 30; ++count) {
    emoji += "\xF0\x9F\x8D\x8E";
  }
  CHECK(FitVendingTitle(emoji).size() == 4 * 19);
}

TEST_CASE("Échoppe : la préparation contrôlée avant l'envoi", "[sim][vending]") {
  const std::array<InventoryItem, 2> cart = {Carted(2, 10), Carted(3, 1)};

  SECTION("une échoppe correcte passe") {
    const std::array<VendingSetupLine, 1> lines = {VendingSetupLine{2, 10, 100}};
    CHECK(CheckVendingSetup("t", lines, 3, cart, 1000).empty());
  }
  SECTION("titre vide, aucune ligne") {
    const auto refusals = CheckVendingSetup("", {}, 3, cart, 0);
    CHECK(Has(refusals, VendingSetupFault::kNoTitle, ro::sim::kVendingWholeRequest));
    CHECK(Has(refusals, VendingSetupFault::kNoLines, ro::sim::kVendingWholeRequest));
  }
  SECTION("chaque défaut de ligne est nommé à sa ligne") {
    InventoryItem broken = Carted(5, 1);
    broken.damaged = true;
    InventoryItem unknown = Carted(6, 1);
    unknown.identified = false;
    InventoryItem rented = Carted(7, 1);
    rented.rental_until_unix_s = 1;
    const std::array<InventoryItem, 5> full_cart = {Carted(2, 10), Carted(3, 1), broken, unknown,
                                                    rented};
    const std::array<VendingSetupLine, 8> lines = {
        VendingSetupLine{2, 11, 0},
        VendingSetupLine{2, 1, 1},
        VendingSetupLine{3, 0, ro::sim::kVendingMostPriceZeny + 1},
        VendingSetupLine{4, 1, 1},
        VendingSetupLine{5, 1, 1},
        VendingSetupLine{6, 1, 1},
        VendingSetupLine{7, 1, 1},
        VendingSetupLine{8, 1, 1}};
    const auto refusals = CheckVendingSetup("t", lines, 7, full_cart, 0);
    CHECK(Has(refusals, VendingSetupFault::kTooManyLines, ro::sim::kVendingWholeRequest));
    CHECK(Has(refusals, VendingSetupFault::kAmountAboveStack, 0));
    CHECK(Has(refusals, VendingSetupFault::kZeroPrice, 0));
    CHECK(Has(refusals, VendingSetupFault::kDuplicateSlot, 1));
    CHECK(Has(refusals, VendingSetupFault::kZeroAmount, 2));
    CHECK(Has(refusals, VendingSetupFault::kPriceAboveMost, 2));
    CHECK(Has(refusals, VendingSetupFault::kNotInCart, 3));
    CHECK(Has(refusals, VendingSetupFault::kDamaged, 4));
    CHECK(Has(refusals, VendingSetupFault::kUnidentified, 5));
    CHECK(Has(refusals, VendingSetupFault::kRented, 6));
  }
  SECTION("zeny plus recette au-delà du plafond") {
    const std::array<VendingSetupLine, 1> lines = {VendingSetupLine{2, 3, 1'000'000'000}};
    CHECK(CheckVendingSetup("t", lines, 3, cart, 0).size() == 1);
    CHECK(Has(CheckVendingSetup("t", lines, 3, cart, 0), VendingSetupFault::kTakingsOverflow,
              ro::sim::kVendingWholeRequest));
    const std::array<VendingSetupLine, 1> fits = {VendingSetupLine{2, 2, 1'000'000'000}};
    CHECK(CheckVendingSetup("t", fits, 3, cart, 147'483'647).empty());
    CHECK_FALSE(CheckVendingSetup("t", fits, 3, cart, 147'483'648).empty());
  }
}

TEST_CASE("Échoppe visitée : le panier fusionné et contrôlé", "[sim][vending]") {
  const std::array<VendingLine, 2> offers = {Line(2, 512, 5, 10), Line(3, 607, 1, 900)};

  const std::array<VendingBasketLine, 3> basket = {VendingBasketLine{2, 2}, VendingBasketLine{3, 1},
                                                   VendingBasketLine{2, 3}};
  const auto merged = MergeVendingBasket(basket);
  REQUIRE(merged.size() == 2);
  CHECK(merged[0].shop_slot == 2);
  CHECK(merged[0].amount == 5);
  CHECK(ro::sim::VendingBasketCost(merged, offers) == 950);
  CHECK(CheckVendingBasket(basket, offers, 950).empty());

  SECTION("zeny insuffisants") {
    const auto refusals = CheckVendingBasket(basket, offers, 949);
    REQUIRE(refusals.size() == 1);
    CHECK(refusals[0].fault == VendingBasketFault::kTotalAboveZeny);
  }
  SECTION("les lignes identiques cumulées dépassent le stock") {
    const std::array<VendingBasketLine, 2> greedy = {VendingBasketLine{2, 3},
                                                     VendingBasketLine{2, 3}};
    const auto refusals = CheckVendingBasket(greedy, offers, 1'000'000);
    REQUIRE(refusals.size() == 1);
    CHECK(refusals[0].fault == VendingBasketFault::kAmountAboveStock);
    CHECK(refusals[0].line == 0);
  }
  SECTION("panier vide, quantité nulle, offre inconnue") {
    CHECK(CheckVendingBasket({}, offers, 0)[0].fault == VendingBasketFault::kEmpty);
    const std::array<VendingBasketLine, 2> odd = {VendingBasketLine{2, 0}, VendingBasketLine{9, 1}};
    const auto refusals = CheckVendingBasket(odd, offers, 1'000'000);
    REQUIRE(refusals.size() == 2);
    CHECK(refusals[0].fault == VendingBasketFault::kZeroAmount);
    CHECK(refusals[1].fault == VendingBasketFault::kUnknownOffer);
    CHECK(refusals[1].line == 1);
  }
  SECTION("plus de douze lignes") {
    std::vector<VendingLine> many;
    std::vector<VendingBasketLine> wide;
    for (std::uint16_t slot = 2; slot < 2 + 13; ++slot) {
      many.push_back(Line(slot, 512, 1, 1));
      wide.push_back(VendingBasketLine{slot, 1});
    }
    CHECK(CheckVendingBasket(wide, many, 1'000)[0].fault == VendingBasketFault::kTooManyLines);
  }
}
