// LA BOUTIQUE D'UN PNJ, décodée — et ses demandes, sur le fil.
//
// 🔴 CE BANC PORTE CE QU'AUCUNE SESSION NE MONTRERAIT VITE :
//
//   · un achat à une boutique DÉSARMÉE part précédé du choix qui la réarme,
//     dans la même rafale ;
//   · le résultat d'achat n'est lu que si une boutique l'attend — c'est aussi
//     celui du troc ;
//   · les changements de carte ferment sans être consommés, et le market doit
//     son congé à part.

#include "ro/net/npc_shop.h"

#include <array>
#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/connection.h"
#include "ro/proto/map_packets.h"
#include "ro/proto/npc_shop_packets.h"
#include "ro/sim/npc_shop.h"

#include "support/loopback_link.h"
#include "support/loopback_server.h"
#include "support/wire_bytes.h"

using ro::net::ApplyNpcShopFrame;
using ro::net::Frame;
using ro::net::FrameOutcome;
using ro::sim::NpcShop;
using ro::sim::NpcShopBuyLine;
using ro::sim::NpcShopFarewell;
using ro::sim::NpcShopKind;
using ro::sim::NpcShopSellLine;
using ro::test::Connect;
using ro::test::LoopbackServer;
using ro::test_support::PutU16;
using ro::test_support::PutU32;
using ro::test_support::ReadU16;
using ro::test_support::ReadU32;

namespace {

constexpr std::uint32_t kShopNpc = 110000123;

void PutU8(std::vector<std::uint8_t>& into, std::uint8_t value) { into.push_back(value); }

// Pose la longueur annoncée d'une trame variable, une fois qu'elle est écrite.
Frame Sealed(std::uint16_t opcode, std::vector<std::uint8_t> bytes) {
  bytes[2] = static_cast<std::uint8_t>(bytes.size() & 0xFF);
  bytes[3] = static_cast<std::uint8_t>(bytes.size() >> 8);
  return Frame{.opcode = opcode, .bytes = std::move(bytes)};
}

Frame DealChoiceFrame(std::uint32_t npc_id) {
  std::vector<std::uint8_t> bytes;
  PutU16(bytes, 0x00C4);
  PutU32(bytes, npc_id);
  return Frame{.opcode = 0x00C4, .bytes = bytes};
}

// La liste d'achat : en-tête de quatre octets, puis dix-neuf par ligne.
Frame BuyListFrame() {
  std::vector<std::uint8_t> bytes;
  PutU16(bytes, 0x0B77);
  PutU16(bytes, 0);
  PutU32(bytes, 501);
  PutU32(bytes, 50);
  PutU32(bytes, 38);
  PutU8(bytes, 0);
  PutU16(bytes, 0);
  PutU32(bytes, 0);
  PutU32(bytes, 2301);
  PutU32(bytes, 1000);
  PutU32(bytes, 760);
  PutU8(bytes, 5);
  PutU16(bytes, 12);
  PutU32(bytes, 0x10);
  return Sealed(0x0B77, bytes);
}

// La liste de vente : dix octets par ligne.
Frame SellListFrame() {
  std::vector<std::uint8_t> bytes;
  PutU16(bytes, 0x00C7);
  PutU16(bytes, 0);
  PutU16(bytes, 4);
  PutU32(bytes, 25);
  PutU32(bytes, 30);
  return Sealed(0x00C7, bytes);
}

Frame PurchaseResultFrame(std::uint8_t result) {
  std::vector<std::uint8_t> bytes;
  PutU16(bytes, 0x00CA);
  PutU8(bytes, result);
  return Frame{.opcode = 0x00CA, .bytes = bytes};
}

}  // namespace

TEST_CASE("Boutique PNJ : les deux listes se lisent, prix facturé compris", "[net][npc_shop]") {
  NpcShop shop;
  REQUIRE(ApplyNpcShopFrame(DealChoiceFrame(kShopNpc), shop) == FrameOutcome::kApplied);
  REQUIRE(ApplyNpcShopFrame(BuyListFrame(), shop) == FrameOutcome::kApplied);
  REQUIRE(shop.buy_offers().size() == 2);
  CHECK(shop.buy_offers()[0].item_id == 501);
  CHECK(shop.buy_offers()[0].base_price == 50);
  CHECK(shop.buy_offers()[0].price == 38);
  CHECK(shop.buy_offers()[1].item_type == 5);
  CHECK(shop.buy_offers()[1].view_sprite == 12);
  CHECK(shop.buy_offers()[1].equip_location == 0x10);

  REQUIRE(ApplyNpcShopFrame(SellListFrame(), shop) == FrameOutcome::kApplied);
  REQUIRE(shop.sell_offers().size() == 1);
  CHECK(shop.sell_offers()[0].client_slot == 4);
  CHECK(shop.sell_offers()[0].base_price == 25);
  CHECK(shop.sell_offers()[0].price == 30);
}

TEST_CASE("Boutique PNJ : le résultat d'achat n'est lu que si une boutique l'attend",
          "[net][npc_shop]") {
  NpcShop shop;
  CHECK(ApplyNpcShopFrame(PurchaseResultFrame(12), shop) == FrameOutcome::kUnhandled);
  REQUIRE(ApplyNpcShopFrame(DealChoiceFrame(kShopNpc), shop) == FrameOutcome::kApplied);
  CHECK(ApplyNpcShopFrame(PurchaseResultFrame(1), shop) == FrameOutcome::kApplied);
  CHECK(shop.outcome().result == 1);
}

TEST_CASE("Boutique PNJ : le market, son résultat et son stock", "[net][npc_shop]") {
  NpcShop shop;
  std::vector<std::uint8_t> open;
  PutU16(open, 0x0B7A);
  PutU16(open, 0);
  PutU32(open, 501);
  PutU8(open, 0);
  PutU32(open, 40);
  PutU32(open, 9);
  PutU16(open, 70);
  PutU32(open, 0);
  PutU32(open, 502);
  PutU8(open, 0);
  PutU32(open, 60);
  PutU32(open, 0xFFFFFFFF);
  PutU16(open, 100);
  PutU32(open, 0);
  REQUIRE(ApplyNpcShopFrame(Sealed(0x0B7A, open), shop) == FrameOutcome::kApplied);
  CHECK(shop.kind() == NpcShopKind::kMarket);
  REQUIRE(shop.buy_offers().size() == 2);
  CHECK(shop.buy_offers()[0].stock == 9);
  CHECK(shop.buy_offers()[0].unit_weight == 70);
  CHECK(shop.buy_offers()[1].stock == ro::sim::kNpcShopUnlimitedStock);

  shop.NoteDealSent();
  std::vector<std::uint8_t> result;
  PutU16(result, 0x0B4E);
  PutU16(result, 0);
  PutU16(result, 0);
  PutU32(result, 501);
  PutU16(result, 4);
  PutU32(result, 40);
  REQUIRE(ApplyNpcShopFrame(Sealed(0x0B4E, result), shop) == FrameOutcome::kApplied);
  CHECK(shop.buy_offers()[0].stock == 5);
}

TEST_CASE("Boutique PNJ : la boutique à points et son résultat", "[net][npc_shop]") {
  NpcShop shop;
  std::vector<std::uint8_t> list;
  PutU16(list, 0x0287);
  PutU16(list, 0);
  PutU32(list, 120);
  PutU32(list, 0);
  PutU32(list, 30);
  PutU32(list, 30);
  PutU8(list, 0);
  PutU32(list, 12345);
  REQUIRE(ApplyNpcShopFrame(Sealed(0x0287, list), shop) == FrameOutcome::kApplied);
  CHECK(shop.kind() == NpcShopKind::kPoints);
  CHECK(shop.points() == 120);
  REQUIRE(shop.buy_offers().size() == 1);
  CHECK(shop.buy_offers()[0].item_id == 12345);

  shop.NoteDealSent();
  const std::vector<std::uint8_t> update =
      ro::proto::EncodePointShopUpdate(ro::proto::PointShopUpdate{.points = 90, .result = 0});
  REQUIRE(update.size() == ro::proto::PointShopUpdate::kWireSize);
  REQUIRE(ApplyNpcShopFrame(Frame{.opcode = 0x0289, .bytes = update}, shop) ==
          FrameOutcome::kApplied);
  CHECK(shop.points() == 90);
  CHECK(shop.outcome().seq == 1);
}

TEST_CASE("Boutique PNJ : les changements de carte ferment sans être consommés",
          "[net][npc_shop]") {
  NpcShop shop;
  REQUIRE(ApplyNpcShopFrame(DealChoiceFrame(kShopNpc), shop) == FrameOutcome::kApplied);
  const Frame map_move{.opcode = ro::proto::ZcNpcackMapmove::kOpcode, .bytes = {}};
  CHECK(ApplyNpcShopFrame(map_move, shop) == FrameOutcome::kUnhandled);
  CHECK_FALSE(shop.open());
  CHECK(shop.TakeFarewell() == NpcShopFarewell::kTradeQuit);

  REQUIRE(ApplyNpcShopFrame(DealChoiceFrame(kShopNpc), shop) == FrameOutcome::kApplied);
  const Frame server_move{.opcode = ro::proto::ZcNpcackServermove::kOpcode, .bytes = {}};
  CHECK(ApplyNpcShopFrame(server_move, shop) == FrameOutcome::kUnhandled);
  CHECK(shop.TakeFarewell() == NpcShopFarewell::kNone);
}

TEST_CASE("Boutique PNJ : l'achat désarmé part derrière son choix, la vente aussi",
          "[net][npc_shop]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  ro::net::Connection link = Connect(server);

  NpcShop shop;
  REQUIRE(ApplyNpcShopFrame(DealChoiceFrame(kShopNpc), shop) == FrameOutcome::kApplied);
  REQUIRE(ro::net::ChooseNpcShopSide(link, shop, ro::sim::NpcShopSide::kBuy).has_value());
  const std::vector<std::uint8_t> choice = server.Receive(7);
  REQUIRE(choice.size() == 7);
  CHECK(ReadU16(choice, 0) == 0x00C5);
  CHECK(ReadU32(choice, 2) == kShopNpc);
  CHECK(choice[6] == 0);

  // La liste arme : l'achat part seul.
  REQUIRE(ApplyNpcShopFrame(BuyListFrame(), shop) == FrameOutcome::kApplied);
  const std::array<NpcShopBuyLine, 2> cart = {NpcShopBuyLine{.item_id = 501, .amount = 3},
                                              NpcShopBuyLine{.item_id = 2301, .amount = 1}};
  REQUIRE(ro::net::BuyFromNpcShop(link, shop, cart).has_value());
  const std::vector<std::uint8_t> buy = server.Receive(4 + 2 * 6);
  REQUIRE(buy.size() == 16);
  CHECK(ReadU16(buy, 0) == 0x00C8);
  CHECK(ReadU16(buy, 2) == 16);
  CHECK(ReadU16(buy, 4) == 3);
  CHECK(ReadU32(buy, 6) == 501);
  CHECK(ReadU16(buy, 10) == 1);
  CHECK(ReadU32(buy, 12) == 2301);

  // Une seconde avant le résultat est refusée.
  CHECK_FALSE(ro::net::BuyFromNpcShop(link, shop, cart).has_value());
  REQUIRE(ApplyNpcShopFrame(PurchaseResultFrame(0), shop) == FrameOutcome::kApplied);

  // Désarmée : la vente part derrière le choix de vente.
  REQUIRE(ApplyNpcShopFrame(SellListFrame(), shop) == FrameOutcome::kApplied);
  shop.NoteDealSent();
  REQUIRE(ApplyNpcShopFrame(PurchaseResultFrame(0), shop) == FrameOutcome::kApplied);
  CHECK_FALSE(shop.armed());
  const std::array<NpcShopSellLine, 1> sale = {NpcShopSellLine{.client_slot = 4, .amount = 2}};
  REQUIRE(ro::net::SellToNpcShop(link, shop, sale).has_value());
  const std::vector<std::uint8_t> rearm = server.Receive(7);
  REQUIRE(rearm.size() == 7);
  CHECK(ReadU16(rearm, 0) == 0x00C5);
  CHECK(rearm[6] == 1);
  const std::vector<std::uint8_t> sell = server.Receive(8);
  REQUIRE(sell.size() == 8);
  CHECK(ReadU16(sell, 0) == 0x00C9);
  CHECK(ReadU16(sell, 2) == 8);
  CHECK(ReadU16(sell, 4) == 4);
  CHECK(ReadU16(sell, 6) == 2);
}

TEST_CASE("Boutique PNJ : servie, une boutique de script refuse sans émettre", "[net][npc_shop]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  ro::net::Connection link = Connect(server);

  NpcShop shop;
  REQUIRE(ApplyNpcShopFrame(BuyListFrame(), shop) == FrameOutcome::kApplied);
  CHECK(shop.npc_id() == 0);
  const std::array<NpcShopBuyLine, 1> cart = {NpcShopBuyLine{.item_id = 501, .amount = 1}};
  REQUIRE(ro::net::BuyFromNpcShop(link, shop, cart).has_value());
  CHECK(server.Receive(10).size() == 10);
  REQUIRE(ApplyNpcShopFrame(PurchaseResultFrame(0), shop) == FrameOutcome::kApplied);
  CHECK_FALSE(shop.open());
  CHECK(shop.TakeFarewell() == NpcShopFarewell::kTradeQuit);
  CHECK_FALSE(ro::net::ChooseNpcShopSide(link, shop, ro::sim::NpcShopSide::kBuy).has_value());
}

TEST_CASE("Boutique PNJ : les deux congés, sur le fil", "[net][npc_shop]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  ro::net::Connection link = Connect(server);
  REQUIRE(ro::net::SayNpcShopFarewell(link, NpcShopFarewell::kTradeQuit).has_value());
  CHECK(server.Receive(2) == std::vector<std::uint8_t>{0xD4, 0x09});
  REQUIRE(ro::net::SayNpcShopFarewell(link, NpcShopFarewell::kMarketClose).has_value());
  CHECK(server.Receive(2) == std::vector<std::uint8_t>{0xD8, 0x09});
}
