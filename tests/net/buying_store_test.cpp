// L'ÉCHOPPE D'ACHAT, décodée — et ses cinq demandes, sur le fil.
//
// 🔴 CE BANC PORTE TROIS CHOSES QU'AUCUNE SESSION NE MONTRERAIT VITE :
//
//   · un achat conclu fait 24 octets, pas les 22 que la table du serveur déclare ;
//   · la vente à un acheteur retire l'objet du sac par son seul paquet ;
//   · les changements de carte se lisent sans être consommés.

#include "ro/net/buying_store.h"

#include <array>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/connection.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/buying_store.h"
#include "ro/sim/inventory.h"

#include "support/loopback_link.h"
#include "support/loopback_server.h"
#include "support/wire_bytes.h"

using ro::net::ApplyBuyingStoreFrame;
using ro::net::Frame;
using ro::net::FrameOutcome;
using ro::sim::BuyingStore;
using ro::sim::BuyingStorePhase;
using ro::test::Connect;
using ro::test::LoopbackServer;
using ro::test_support::PutU16;
using ro::test_support::PutU32;
using ro::test_support::ReadU16;
using ro::test_support::ReadU32;

namespace {

void PutU8(std::vector<std::uint8_t>& into, std::uint8_t value) { into.push_back(value); }

[[nodiscard]] Frame FrameOf(std::vector<std::uint8_t> bytes) {
  const auto opcode = static_cast<std::uint16_t>(bytes[0] | (bytes[1] << 8));
  return Frame{.opcode = opcode, .bytes = std::move(bytes)};
}

[[nodiscard]] Frame PreparationFrame(std::uint8_t slots) {
  std::vector<std::uint8_t> bytes;
  PutU16(bytes, 0x0810);
  PutU8(bytes, slots);
  return FrameOf(bytes);
}

// Une ligne de liste : prix, quantité, famille, objet — onze octets.
void PutListed(std::vector<std::uint8_t>& into, std::uint32_t price, std::uint16_t amount,
               std::uint8_t type, std::uint32_t item_id) {
  PutU32(into, price);
  PutU16(into, amount);
  PutU8(into, type);
  PutU32(into, item_id);
}

[[nodiscard]] Frame MyListFrame() {
  std::vector<std::uint8_t> bytes;
  PutU16(bytes, 0x0813);
  PutU16(bytes, 12 + 2 * 11);
  PutU32(bytes, 2'000'001);
  PutU32(bytes, 10'000);
  PutListed(bytes, 50, 100, 3, 909);
  PutListed(bytes, 400, 5, 0, 501);
  return FrameOf(bytes);
}

}  // namespace

TEST_CASE("Échoppe d'achat : préparation, liste, achats et fin, décodés", "[buying_store]") {
  BuyingStore store;
  REQUIRE(ApplyBuyingStoreFrame(PreparationFrame(2), store) == FrameOutcome::kApplied);
  CHECK(store.phase() == BuyingStorePhase::kPreparing);
  CHECK(store.slots() == 2);
  REQUIRE(store.TellRequested("Achat", 10'000));

  REQUIRE(ApplyBuyingStoreFrame(MyListFrame(), store) == FrameOutcome::kApplied);
  REQUIRE(store.open());
  CHECK(store.owner_account_id() == 2'000'001);
  CHECK(store.zeny_limit() == 10'000);
  REQUIRE(store.lines().size() == 2);
  CHECK(store.lines()[0].item_id == 909);
  CHECK(store.lines()[0].item_type == 3);
  CHECK(store.lines()[0].wanted == 100);
  CHECK(store.lines()[0].price == 50);
  CHECK(store.lines()[1].item_id == 501);

  // 🔴 VINGT-QUATRE OCTETS.
  std::vector<std::uint8_t> bought;
  PutU16(bought, 0x09E6);
  PutU32(bought, 909);
  PutU16(bought, 30);
  PutU32(bought, 1'500);
  PutU32(bought, 8'500);
  PutU32(bought, 150'000);
  PutU32(bought, 1'790'000'000);
  REQUIRE(bought.size() == 24);
  REQUIRE(ApplyBuyingStoreFrame(FrameOf(bought), store) == FrameOutcome::kApplied);
  CHECK(store.lines()[0].wanted == 70);
  CHECK(store.zeny_limit() == 8'500);
  REQUIRE(store.journal().size() == 1);
  CHECK(store.journal()[0].zeny == 1'500);
  CHECK(store.journal()[0].seller_char_id == 150'000);
  CHECK(store.journal()[0].unix_s == 1'790'000'000);

  std::vector<std::uint8_t> ended;
  PutU16(ended, 0x081A);
  PutU16(ended, 3);
  REQUIRE(ApplyBuyingStoreFrame(FrameOf(ended), store) == FrameOutcome::kApplied);
  CHECK_FALSE(store.open());
  CHECK(store.end_result() == ro::sim::kBuyingStoreEndLimitReached);
  CHECK(store.journal().size() == 1);

  SECTION("une liste coupée est indéchiffrable") {
    Frame cut = MyListFrame();
    cut.bytes.pop_back();
    CHECK(ApplyBuyingStoreFrame(cut, store) == FrameOutcome::kUndecodable);
  }
}

TEST_CASE("Échoppe d'achat : le refus d'ouverture, décodé", "[buying_store]") {
  BuyingStore store;
  REQUIRE(ApplyBuyingStoreFrame(PreparationFrame(5), store) == FrameOutcome::kApplied);
  REQUIRE(store.TellRequested("x", 1));
  std::vector<std::uint8_t> refused;
  PutU16(refused, 0x0812);
  PutU16(refused, 2);
  PutU32(refused, 24'000);
  REQUIRE(ApplyBuyingStoreFrame(FrameOf(refused), store) == FrameOutcome::kApplied);
  CHECK(store.phase() == BuyingStorePhase::kClosed);
  CHECK(store.open_refusal() == ro::sim::kBuyingStoreOpenOverweight);
  CHECK(store.open_refusal_weight() == 24'000);
}

TEST_CASE("Échoppe d'achat : visite, vente confirmée et refus, décodés", "[buying_store]") {
  BuyingStore store;
  std::vector<std::uint8_t> listed;
  PutU16(listed, 0x0818);
  PutU16(listed, 16 + 2 * 11);
  PutU32(listed, 2'000'001);
  PutU32(listed, 42);
  PutU32(listed, 10'000);
  PutListed(listed, 50, 100, 3, 909);
  // Une ligne pleine reste listée.
  PutListed(listed, 400, 0, 0, 501);
  REQUIRE(ApplyBuyingStoreFrame(FrameOf(listed), store) == FrameOutcome::kApplied);
  REQUIRE(store.visiting());
  CHECK(store.visited().buyer_account_id == 2'000'001);
  CHECK(store.visited().store_id == 42);
  CHECK(store.visited().zeny_limit == 10'000);
  REQUIRE(store.visited().lines.size() == 2);
  CHECK(store.visited().lines[1].wanted == 0);

  const std::array<ro::sim::BuyingStoreSaleLine, 1> sale = {
      ro::sim::BuyingStoreSaleLine{.client_slot = 7, .item_id = 909, .amount = 3}};
  REQUIRE(store.TellSaleRequested(sale));

  std::vector<std::uint8_t> sold;
  PutU16(sold, 0x081C);
  PutU16(sold, 7);
  PutU16(sold, 3);
  PutU32(sold, 50);
  REQUIRE(ApplyBuyingStoreFrame(FrameOf(sold), store) == FrameOutcome::kApplied);
  REQUIRE(store.sales().size() == 1);
  CHECK(store.sales()[0].item_id == 909);
  CHECK(store.sales()[0].zeny == 150);
  CHECK(store.visited().lines[0].wanted == 97);
  CHECK(store.visited().zeny_limit == 9'850);

  // 🔴 LE MÊME PAQUET RETIRE L'OBJET DU SAC.
  ro::sim::Inventory bag;
  ro::sim::InventoryItem jellopy;
  jellopy.client_slot = 7;
  jellopy.item_id = 909;
  jellopy.amount = 10;
  bag.Put(jellopy);
  REQUIRE(ro::net::ApplyBuyingStoreSaleToBag(FrameOf(sold), bag) == FrameOutcome::kApplied);
  REQUIRE(bag.Find(7) != nullptr);
  CHECK(bag.Find(7)->amount == 7);
  CHECK(ro::net::ApplyBuyingStoreSaleToBag(PreparationFrame(1), bag) == FrameOutcome::kUnhandled);

  std::vector<std::uint8_t> refused;
  PutU16(refused, 0x0824);
  PutU16(refused, 6);
  PutU32(refused, 909);
  REQUIRE(store.TellSaleRequested(sale));
  REQUIRE(ApplyBuyingStoreFrame(FrameOf(refused), store) == FrameOutcome::kApplied);
  CHECK_FALSE(store.sale_pending());
  CHECK(store.sale_refusal() == ro::sim::kBuyingStoreSaleOverCount);
  CHECK(store.sale_refusal_item() == 909);
}

TEST_CASE("Échoppe d'achat : les changements de carte ferment sans être consommés",
          "[buying_store]") {
  BuyingStore store;
  REQUIRE(ApplyBuyingStoreFrame(PreparationFrame(2), store) == FrameOutcome::kApplied);
  const Frame map_move{.opcode = ro::proto::ZcNpcackMapmove::kOpcode, .bytes = {}};
  CHECK(ApplyBuyingStoreFrame(map_move, store) == FrameOutcome::kUnhandled);
  CHECK(store.phase() == BuyingStorePhase::kClosed);

  REQUIRE(ApplyBuyingStoreFrame(PreparationFrame(2), store) == FrameOutcome::kApplied);
  REQUIRE(store.TellRequested("x", 1));
  REQUIRE(ApplyBuyingStoreFrame(MyListFrame(), store) == FrameOutcome::kApplied);
  const Frame server_move{.opcode = ro::proto::ZcNpcackServermove::kOpcode, .bytes = {}};
  CHECK(ApplyBuyingStoreFrame(server_move, store) == FrameOutcome::kUnhandled);
  CHECK_FALSE(store.open());
  CHECK_FALSE(store.TakeFarewell());
}

TEST_CASE("Échoppe d'achat : le titre se coupe à 79 octets, sur un caractère", "[buying_store]") {
  CHECK(ro::net::BuyingStoreTitleOnWire("Achat") == "Achat");
  // 1252 : un octet par lettre, coupé net.
  CHECK(ro::net::BuyingStoreTitleOnWire(std::string(100, 'a')).size() == 79);
  // UTF-8 : un emoji de quatre octets ne se coupe pas en deux.
  std::string emojis;
  for (int count = 0; count < 20; ++count) {
    emojis += "\xF0\x9F\x98\x80";
  }
  const std::string cut = ro::net::BuyingStoreTitleOnWire(emojis);
  CHECK(cut.size() == 76);
}

TEST_CASE("Échoppe d'achat : les demandes, sur le fil", "[buying_store]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  ro::net::Connection link = Connect(server);
  BuyingStore store;

  const std::array<ro::sim::BuyingStoreWish, 1> wishes = {
      ro::sim::BuyingStoreWish{.item_id = 909, .amount = 100, .price = 50}};

  // Hors préparation, rien ne part.
  CHECK_FALSE(ro::net::RequestBuyingStoreOpen(link, store, "Achat", 5'000, wishes).has_value());

  store.TellPreparation(2);
  // Une limite que le fil ne porte pas, ou aucune ligne : rien ne part.
  CHECK_FALSE(ro::net::RequestBuyingStoreOpen(link, store, "Achat", 0, wishes).has_value());
  CHECK_FALSE(ro::net::RequestBuyingStoreOpen(link, store, "Achat", 5'000, {}).has_value());
  const std::array<ro::sim::BuyingStoreWish, 1> huge = {
      ro::sim::BuyingStoreWish{.item_id = 909, .amount = 70'000, .price = 50}};
  CHECK_FALSE(ro::net::RequestBuyingStoreOpen(link, store, "Achat", 5'000, huge).has_value());

  REQUIRE(ro::net::RequestBuyingStoreOpen(link, store, "Achat", 5'000, wishes).has_value());
  CHECK(store.phase() == BuyingStorePhase::kRequested);
  const std::vector<std::uint8_t> open = server.Receive(99);
  REQUIRE(open.size() == 99);
  CHECK(ReadU16(open, 0) == 0x0811);
  CHECK(ReadU16(open, 2) == 99);
  CHECK(ReadU32(open, 4) == 5'000);
  CHECK(open[8] == 1);
  CHECK(ReadU32(open, 89) == 909);
  // Déjà partie : la seconde ne part pas.
  CHECK_FALSE(ro::net::RequestBuyingStoreOpen(link, store, "Achat", 5'000, wishes).has_value());

  REQUIRE(ro::net::SayBuyingStoreClosed(link).has_value());
  CHECK(server.Receive(2) == std::vector<std::uint8_t>{0x15, 0x08});

  store.TellPreparation(2);
  REQUIRE(ro::net::RequestBuyingStoreCancel(link, store).has_value());
  CHECK(store.phase() == BuyingStorePhase::kClosed);
  const std::vector<std::uint8_t> cancel = server.Receive(89);
  REQUIRE(cancel.size() == 89);
  CHECK(cancel[8] == 0);

  REQUIRE(ro::net::RequestBuyingStoreList(link, 2'000'001).has_value());
  const std::vector<std::uint8_t> visit = server.Receive(6);
  REQUIRE(visit.size() == 6);
  CHECK(ReadU16(visit, 0) == 0x0817);
  CHECK(ReadU32(visit, 2) == 2'000'001);

  const std::array<ro::sim::BuyingStoreSaleLine, 1> sale = {
      ro::sim::BuyingStoreSaleLine{.client_slot = 7, .item_id = 909, .amount = 3}};
  // Hors visite, rien ne part.
  CHECK_FALSE(ro::net::RequestBuyingStoreSale(link, store, sale).has_value());
  store.TellVisit(ro::sim::VisitedBuyingStore{
      .buyer_account_id = 2'000'001, .store_id = 42, .zeny_limit = 10'000, .lines = {}});
  REQUIRE(ro::net::RequestBuyingStoreSale(link, store, sale).has_value());
  CHECK(store.sale_pending());
  const std::vector<std::uint8_t> trade = server.Receive(20);
  REQUIRE(trade.size() == 20);
  CHECK(ReadU16(trade, 0) == 0x0819);
  CHECK(ReadU32(trade, 4) == 2'000'001);
  CHECK(ReadU32(trade, 8) == 42);
  CHECK(ReadU16(trade, 12) == 7);
  CHECK(ReadU32(trade, 14) == 909);
  CHECK(ReadU16(trade, 18) == 3);
  // Une vente attend : la seconde ne part pas.
  CHECK_FALSE(ro::net::RequestBuyingStoreSale(link, store, sale).has_value());
}
