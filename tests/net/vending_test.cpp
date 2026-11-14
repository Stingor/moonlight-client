// L'ÉCHOPPE DE VENTE, décodée — et ses demandes, sur le fil.
//
// 🔴 CE BANC PORTE CE QU'AUCUNE SESSION NE MONTRERAIT VITE :
//
//   · les listes se lisent jusqu'au bout de leur longueur, objet complet compris ;
//   · la disparition d'une enseigne et les changements de carte se lisent SANS
//     être consommés ;
//   · un achat part suivi de la demande de liste qui en montrera l'effet ;
//   · un verbe refuse sans rien envoyer ce que le serveur fausserait.

#include "ro/net/vending.h"

#include <array>
#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/connection.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/inventory.h"
#include "ro/sim/vending.h"

#include "support/loopback_link.h"
#include "support/loopback_server.h"
#include "support/wire_bytes.h"

using ro::net::ApplyVendingFrame;
using ro::net::Frame;
using ro::net::FrameOutcome;
using ro::sim::InventoryItem;
using ro::sim::Vending;
using ro::sim::VendingBasketLine;
using ro::sim::VendingSetupLine;
using ro::sim::VendingVisitEnd;
using ro::test::Connect;
using ro::test::LoopbackServer;
using ro::test_support::PutU16;
using ro::test_support::PutU32;
using ro::test_support::ReadU16;
using ro::test_support::ReadU32;

namespace {

constexpr std::uint32_t kVendorAid = 2000005;
constexpr std::uint32_t kVenderId = 77;
constexpr std::uint32_t kMyAid = 2000001;

void PutU8(std::vector<std::uint8_t>& into, std::uint8_t value) { into.push_back(value); }

// Les champs d'objet communs aux deux listes, à partir du type : identifié,
// cassé, quatre cartes, cinq options.
void PutItemBody(std::vector<std::uint8_t>& into, std::uint32_t item_id) {
  PutU8(into, 4);  // type
  PutU32(into, item_id);
  PutU8(into, 1);  // identifié
  PutU8(into, 0);  // intact
  for (std::uint32_t card = 0; card < 4; ++card) {
    PutU32(into, card == 0 ? 4001 : 0);
  }
  for (int option = 0; option < 5; ++option) {
    PutU16(into, option == 0 ? 25 : 0);
    PutU16(into, option == 0 ? 3 : 0);
    PutU8(into, 0);
  }
}

Frame Framed(std::uint16_t opcode, std::vector<std::uint8_t> bytes) {
  return Frame{.opcode = opcode, .bytes = std::move(bytes)};
}

Frame PreparationFrame(std::uint16_t lines) {
  std::vector<std::uint8_t> bytes;
  PutU16(bytes, 0x012D);
  PutU16(bytes, lines);
  return Framed(0x012D, bytes);
}

// Ma propre échoppe : une ligne de cinquante-huit octets, prix en tête.
Frame MyListFrame(std::uint16_t slot, std::uint16_t amount, std::uint32_t price) {
  std::vector<std::uint8_t> bytes;
  PutU16(bytes, 0x0B40);
  PutU16(bytes, 8 + 58);
  PutU32(bytes, kMyAid);
  PutU32(bytes, price);
  PutU16(bytes, slot);
  PutU16(bytes, amount);
  PutItemBody(bytes, 1201);
  PutU8(bytes, 7);  // raffinement
  PutU8(bytes, 2);  // grade
  return Framed(0x0B40, bytes);
}

Frame SaleFrame(std::uint16_t slot, std::uint16_t amount, std::int32_t zeny) {
  std::vector<std::uint8_t> bytes;
  PutU16(bytes, 0x09E5);
  PutU16(bytes, slot);
  PutU16(bytes, amount);
  PutU32(bytes, 150001);
  PutU32(bytes, 1'700'000'000);
  PutU32(bytes, static_cast<std::uint32_t>(zeny));
  return Framed(0x09E5, bytes);
}

// L'échoppe d'un autre : soixante-quatre octets par offre, quantité avant la
// case, emplacement et apparence après les options.
Frame VisitFrame(int offers) {
  std::vector<std::uint8_t> bytes;
  PutU16(bytes, 0x0B3D);
  PutU16(bytes, static_cast<std::uint16_t>(12 + 64 * offers));
  PutU32(bytes, kVendorAid);
  PutU32(bytes, kVenderId);
  for (int rank = 0; rank < offers; ++rank) {
    PutU32(bytes, 900);
    PutU16(bytes, 5);
    PutU16(bytes, static_cast<std::uint16_t>(2 + rank));
    PutItemBody(bytes, 2301);
    PutU32(bytes, 0x10);  // emplacement
    PutU16(bytes, 3);     // apparence
    PutU8(bytes, 4);
    PutU8(bytes, 1);
  }
  return Framed(0x0B3D, bytes);
}

}  // namespace

TEST_CASE("Échoppe : préparation, liste, résultat et ventes se décodent", "[net][vending]") {
  Vending vending;
  REQUIRE(ApplyVendingFrame(PreparationFrame(7), vending) == FrameOutcome::kApplied);
  CHECK(vending.preparing());
  CHECK(vending.offered_lines() == 7);

  REQUIRE(ApplyVendingFrame(MyListFrame(4, 10, 50), vending) == FrameOutcome::kApplied);
  REQUIRE(vending.shop_open());
  CHECK(vending.shop_owner_aid() == kMyAid);
  REQUIRE(vending.shop_lines().size() == 1);
  const ro::sim::VendingLine& line = vending.shop_lines()[0];
  CHECK(line.price_zeny == 50);
  CHECK(line.item.client_slot == 4);
  CHECK(line.item.amount == 10);
  CHECK(line.item.type == 4);
  CHECK(line.item.item_id == 1201);
  CHECK(line.item.identified);
  CHECK_FALSE(line.item.damaged);
  CHECK(line.item.cards[0] == 4001);
  CHECK(line.item.options[0].id == 25);
  CHECK(line.item.options[0].value == 3);
  CHECK(line.item.refine_level == 7);
  CHECK(line.item.grade == 2);

  std::vector<std::uint8_t> ack;
  PutU16(ack, 0x0A28);
  PutU8(ack, 0);
  REQUIRE(ApplyVendingFrame(Framed(0x0A28, ack), vending) == FrameOutcome::kApplied);
  CHECK(vending.opening_result_seq() == 1);

  REQUIRE(ApplyVendingFrame(SaleFrame(4, 3, 150), vending) == FrameOutcome::kApplied);
  CHECK(vending.shop_lines()[0].item.amount == 7);
  REQUIRE(vending.journal().size() == 1);
  CHECK(vending.journal()[0].buyer_char_id == 150001);
  CHECK(vending.journal()[0].sold_at_unix_s == 1'700'000'000);
  CHECK(vending.earned_zeny() == 150);

  SECTION("une liste coupée est indéchiffrable") {
    Frame cut = MyListFrame(4, 1, 1);
    cut.bytes.pop_back();
    CHECK(ApplyVendingFrame(cut, vending) == FrameOutcome::kUndecodable);
  }
}

TEST_CASE("Échoppe visitée : liste, refus, enseigne et carte", "[net][vending]") {
  Vending vending;
  REQUIRE(ApplyVendingFrame(VisitFrame(2), vending) == FrameOutcome::kApplied);
  REQUIRE(vending.visiting());
  CHECK(vending.vendor_aid() == kVendorAid);
  CHECK(vending.vender_id() == kVenderId);
  REQUIRE(vending.offers().size() == 2);
  const ro::sim::VendingLine& offer = vending.offers()[1];
  CHECK(offer.price_zeny == 900);
  CHECK(offer.item.amount == 5);
  CHECK(offer.item.client_slot == 3);
  CHECK(offer.item.item_id == 2301);
  CHECK(offer.item.wearable_at == 0x10);
  CHECK(offer.item.view_id == 3);
  CHECK(offer.item.refine_level == 4);
  CHECK(offer.item.grade == 1);

  std::vector<std::uint8_t> refused;
  PutU16(refused, 0x0135);
  PutU16(refused, 3);
  PutU16(refused, 9);
  PutU8(refused, ro::sim::kVendingBuyOutOfStock);
  REQUIRE(ApplyVendingFrame(Framed(0x0135, refused), vending) == FrameOutcome::kApplied);
  CHECK(vending.purchase_result() == ro::sim::kVendingBuyOutOfStock);
  CHECK(vending.purchase_result_slot() == 3);
  CHECK(vending.purchase_result_amount() == 9);

  SECTION("l'enseigne qui disparaît ferme la visite sans être consommée") {
    std::vector<std::uint8_t> gone;
    PutU16(gone, 0x0132);
    PutU32(gone, kVendorAid);
    CHECK(ApplyVendingFrame(Framed(0x0132, gone), vending) == FrameOutcome::kUnhandled);
    CHECK_FALSE(vending.visiting());
    CHECK(vending.visit_end() == VendingVisitEnd::kVendorGone);
  }
  SECTION("le changement de carte aussi") {
    const Frame map_move{.opcode = ro::proto::ZcNpcackMapmove::kOpcode, .bytes = {}};
    CHECK(ApplyVendingFrame(map_move, vending) == FrameOutcome::kUnhandled);
    CHECK_FALSE(vending.visiting());
  }
}

TEST_CASE("Échoppe : l'ouverture, l'abandon et la fermeture, sur le fil", "[net][vending]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  ro::net::Connection link = Connect(server);

  Vending vending;
  const std::array<InventoryItem, 1> cart = {[] {
    InventoryItem item;
    item.client_slot = 4;
    item.item_id = 512;
    item.amount = 10;
    item.identified = true;
    return item;
  }()};
  const std::array<VendingSetupLine, 1> lines = {VendingSetupLine{4, 10, 50}};

  // Sans préparation, rien ne part.
  CHECK_FALSE(ro::net::RequestVendingOpen(link, vending, "Pommes", lines, cart, 0).has_value());
  CHECK_FALSE(ro::net::RequestVendingShopClose(link, vending).has_value());

  REQUIRE(ApplyVendingFrame(PreparationFrame(3), vending) == FrameOutcome::kApplied);
  // Un prix au-delà du plafond serait rabattu : refusé.
  const std::array<VendingSetupLine, 1> too_dear = {
      VendingSetupLine{4, 1, ro::sim::kVendingMostPriceZeny + 1}};
  CHECK_FALSE(ro::net::RequestVendingOpen(link, vending, "Pommes", too_dear, cart, 0).has_value());
  CHECK_FALSE(vending.opening_pending());

  REQUIRE(ro::net::RequestVendingOpen(link, vending, "Pommes", lines, cart, 0).has_value());
  const std::vector<std::uint8_t> open = server.Receive(85 + 8);
  REQUIRE(open.size() == 85 + 8);
  CHECK(ReadU16(open, 0) == 0x01B2);
  CHECK(ReadU16(open, 2) == 85 + 8);
  CHECK(open[4] == 'P');
  CHECK(open[9] == 's');
  CHECK(open[10] == 0);
  CHECK(open[84] == 1);
  CHECK(ReadU16(open, 85) == 4);
  CHECK(ReadU16(open, 87) == 10);
  CHECK(ReadU32(open, 89) == 50);
  CHECK(vending.opening_pending());
  // Déjà en route : ne repart pas.
  CHECK_FALSE(ro::net::RequestVendingOpen(link, vending, "Pommes", lines, cart, 0).has_value());

  REQUIRE(ro::net::SayVendingPreparationCancelled(link).has_value());
  const std::vector<std::uint8_t> cancel = server.Receive(85);
  REQUIRE(cancel.size() == 85);
  CHECK(ReadU16(cancel, 0) == 0x01B2);
  CHECK(ReadU16(cancel, 2) == 85);
  CHECK(cancel[4] == 0);
  CHECK(cancel[84] == 0);

  REQUIRE(ApplyVendingFrame(MyListFrame(4, 10, 50), vending) == FrameOutcome::kApplied);
  CHECK(vending.shop_title() == "Pommes");
  REQUIRE(ro::net::RequestVendingShopClose(link, vending).has_value());
  CHECK(server.Receive(2) == std::vector<std::uint8_t>{0x2E, 0x01});
  CHECK_FALSE(vending.shop_open());
}

TEST_CASE("Échoppe visitée : la liste et l'achat suivi de sa liste, sur le fil", "[net][vending]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  ro::net::Connection link = Connect(server);

  REQUIRE(ro::net::RequestVendingList(link, kVendorAid).has_value());
  const std::vector<std::uint8_t> asked = server.Receive(6);
  REQUIRE(asked.size() == 6);
  CHECK(ReadU16(asked, 0) == 0x0130);
  CHECK(ReadU32(asked, 2) == kVendorAid);

  Vending vending;
  const std::array<VendingBasketLine, 2> basket = {VendingBasketLine{2, 2},
                                                   VendingBasketLine{2, 1}};
  // Sans visite, rien ne part.
  CHECK_FALSE(ro::net::RequestVendingPurchase(link, vending, basket, 1'000'000).has_value());

  REQUIRE(ApplyVendingFrame(VisitFrame(2), vending) == FrameOutcome::kApplied);
  // Zeny insuffisants : refusé.
  CHECK_FALSE(ro::net::RequestVendingPurchase(link, vending, basket, 2'699).has_value());

  REQUIRE(ro::net::RequestVendingPurchase(link, vending, basket, 2'700).has_value());
  const std::vector<std::uint8_t> buy = server.Receive(16);
  REQUIRE(buy.size() == 16);
  CHECK(ReadU16(buy, 0) == 0x0801);
  CHECK(ReadU16(buy, 2) == 16);
  CHECK(ReadU32(buy, 4) == kVendorAid);
  CHECK(ReadU32(buy, 8) == kVenderId);
  // Les deux lignes de la même offre, fusionnées : quantité, puis case.
  CHECK(ReadU16(buy, 12) == 3);
  CHECK(ReadU16(buy, 14) == 2);
  const std::vector<std::uint8_t> refresh = server.Receive(6);
  REQUIRE(refresh.size() == 6);
  CHECK(ReadU16(refresh, 0) == 0x0130);
  CHECK(ReadU32(refresh, 2) == kVendorAid);
  CHECK(vending.purchase_pending());
  CHECK_FALSE(ro::net::RequestVendingPurchase(link, vending, basket, 2'700).has_value());

  // La liste revient vide : tout est acheté.
  REQUIRE(ApplyVendingFrame(VisitFrame(0), vending) == FrameOutcome::kApplied);
  CHECK_FALSE(vending.visiting());
  CHECK(vending.visit_end() == VendingVisitEnd::kEmptied);
}
