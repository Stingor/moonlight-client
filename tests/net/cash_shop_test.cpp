// LA BOUTIQUE DE VOTE, décodée — et ses quatre demandes, sur le fil.
//
// 🔴 CE BANC PORTE TROIS CHOSES QU'AUCUNE SESSION NE MONTRERAIT VITE :
//
//   · les deux changements de carte ferment la boutique SANS être consommés —
//     le monde doit encore les appliquer ;
//   · le changement de carte se dit au serveur, le changement de serveur non ;
//   · l'achat porte les POINTS D'ÉVÉNEMENT DÉPENSÉS, pas le solde.

#include "ro/net/cash_shop.h"

#include <array>
#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/connection.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/cash_shop.h"
#include "ro/sim/hat_effect_items.h"

#include "support/loopback_link.h"
#include "support/loopback_server.h"
#include "support/wire_bytes.h"

using ro::net::ApplyCashShopFrame;
using ro::net::ApplyHatEffectItemsFrame;
using ro::net::CashShopPurchaseLine;
using ro::net::Frame;
using ro::net::FrameOutcome;
using ro::sim::CashShop;
using ro::test::Connect;
using ro::test::LoopbackServer;
using ro::test_support::PutU16;
using ro::test_support::PutU32;
using ro::test_support::ReadU16;
using ro::test_support::ReadU32;

namespace {

[[nodiscard]] Frame OpenFrame(std::uint32_t vote, std::uint32_t event, std::uint32_t tab) {
  std::vector<std::uint8_t> bytes;
  PutU16(bytes, ro::proto::ZcSeCashshopOpen::kOpcode);
  PutU32(bytes, vote);
  PutU32(bytes, event);
  PutU32(bytes, tab);
  return Frame{.opcode = ro::proto::ZcSeCashshopOpen::kOpcode, .bytes = bytes};
}

// Un onglet du catalogue : en-tête de huit octets, puis quatorze par item.
[[nodiscard]] Frame CatalogueFrame(std::uint16_t tab, std::uint32_t first_item, int count) {
  std::vector<std::uint8_t> bytes;
  PutU16(bytes, ro::proto::ZcAckSchedulerCashitem::kOpcode);
  PutU16(bytes, static_cast<std::uint16_t>(ro::proto::ZcAckSchedulerCashitem::kWireSize +
                                           static_cast<std::size_t>(count) *
                                               ro::proto::ZcAckSchedulerCashitemSub::kWireSize));
  PutU16(bytes, static_cast<std::uint16_t>(count));
  PutU16(bytes, tab);
  for (int rank = 0; rank < count; ++rank) {
    PutU32(bytes, first_item + static_cast<std::uint32_t>(rank));
    PutU32(bytes, 100);
    PutU16(bytes, 7);
    PutU32(bytes, 0x0400);
  }
  return Frame{.opcode = ro::proto::ZcAckSchedulerCashitem::kOpcode, .bytes = bytes};
}

[[nodiscard]] Frame ResultFrame(std::uint16_t result, std::uint32_t vote, std::uint32_t event) {
  std::vector<std::uint8_t> bytes;
  PutU16(bytes, ro::proto::ZcSePcBuyCashitemResult::kOpcode);
  PutU32(bytes, 0);
  PutU16(bytes, result);
  PutU32(bytes, vote);
  PutU32(bytes, event);
  return Frame{.opcode = ro::proto::ZcSePcBuyCashitemResult::kOpcode, .bytes = bytes};
}

}  // namespace

TEST_CASE("Boutique : l'ouverture pose les points et l'onglet", "[net][cash_shop]") {
  CashShop shop;
  REQUIRE(ApplyCashShopFrame(OpenFrame(250, 40, 7), shop) == FrameOutcome::kApplied);
  CHECK(shop.open());
  CHECK(shop.vote_points() == 250);
  CHECK(shop.event_points() == 40);
  CHECK(shop.opening_tab() == 7);
  // Et la même réponse referme.
  REQUIRE(ApplyCashShopFrame(OpenFrame(250, 40, 7), shop) == FrameOutcome::kApplied);
  CHECK_FALSE(shop.open());
}

TEST_CASE("Boutique : le catalogue se lit par onglet, apparence et emplacement compris",
          "[net][cash_shop]") {
  CashShop shop;
  REQUIRE(ApplyCashShopFrame(CatalogueFrame(1, 5000, 3), shop) == FrameOutcome::kApplied);
  REQUIRE(shop.tab(1).size() == 3);
  CHECK(shop.tab(1)[2].item_id == 5002);
  CHECK(shop.tab(1)[0].price == 100);
  CHECK(shop.tab(1)[0].view_sprite == 7);
  CHECK(shop.tab(1)[0].equip_location == 0x0400);

  SECTION("une trame coupée est indéchiffrable") {
    Frame cut = CatalogueFrame(1, 5000, 2);
    cut.bytes.pop_back();
    CHECK(ApplyCashShopFrame(cut, shop) == FrameOutcome::kUndecodable);
  }
}

TEST_CASE("Boutique : le résultat d'achat porte les soldes", "[net][cash_shop]") {
  CashShop shop;
  REQUIRE(ApplyCashShopFrame(ResultFrame(4, 80, 2), shop) == FrameOutcome::kApplied);
  CHECK(shop.purchase_result() == 4);
  CHECK(shop.vote_points() == 80);
  CHECK(shop.event_points() == 2);
}

TEST_CASE("Boutique : les changements de carte ferment sans être consommés", "[net][cash_shop]") {
  CashShop shop;
  REQUIRE(ApplyCashShopFrame(OpenFrame(1, 1, 0), shop) == FrameOutcome::kApplied);
  const Frame map_move{.opcode = ro::proto::ZcNpcackMapmove::kOpcode, .bytes = {}};
  CHECK(ApplyCashShopFrame(map_move, shop) == FrameOutcome::kUnhandled);
  CHECK_FALSE(shop.open());
  CHECK(shop.TakeFarewell());

  REQUIRE(ApplyCashShopFrame(OpenFrame(1, 1, 0), shop) == FrameOutcome::kApplied);
  const Frame server_move{.opcode = ro::proto::ZcNpcackServermove::kOpcode, .bytes = {}};
  CHECK(ApplyCashShopFrame(server_move, shop) == FrameOutcome::kUnhandled);
  CHECK_FALSE(shop.open());
  CHECK_FALSE(shop.TakeFarewell());
}

TEST_CASE("Effets de chapeau : la carte du serveur remplace la précédente", "[net][cash_shop]") {
  ro::sim::HatEffectItems items;
  items.Tell(1, 1);
  std::vector<std::uint8_t> bytes;
  PutU16(bytes, 0x0F17);
  PutU16(bytes, 12);
  PutU16(bytes, 1);
  PutU32(bytes, 20500);
  PutU16(bytes, 33);
  REQUIRE(ApplyHatEffectItemsFrame(Frame{.opcode = 0x0F17, .bytes = bytes}, items) ==
          FrameOutcome::kApplied);
  CHECK(items.OrdinalOf(20500) == 33);
  CHECK(items.OrdinalOf(1) == 0);
}

TEST_CASE("Boutique : les quatre demandes, sur le fil", "[net][cash_shop]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  ro::net::Connection link = Connect(server);

  REQUIRE(ro::net::RequestCashShopOpen(link, 6).has_value());
  const std::vector<std::uint8_t> open = server.Receive(ro::proto::CzSeCashshopOpen2::kWireSize);
  REQUIRE(open.size() == ro::proto::CzSeCashshopOpen2::kWireSize);
  CHECK(ReadU16(open, 0) == ro::proto::CzSeCashshopOpen2::kOpcode);
  CHECK(ReadU32(open, 2) == 6);

  REQUIRE(ro::net::RequestCashShopCatalogue(link).has_value());
  CHECK(server.Receive(2) == std::vector<std::uint8_t>{0xC9, 0x08});

  REQUIRE(ro::net::SayCashShopClosed(link).has_value());
  CHECK(server.Receive(2) == std::vector<std::uint8_t>{0x4A, 0x08});

  const std::array<CashShopPurchaseLine, 2> lines = {
      CashShopPurchaseLine{.item_id = 5000, .amount = 2, .tab = 6},
      CashShopPurchaseLine{.item_id = 5001, .amount = 1, .tab = 7}};
  REQUIRE(ro::net::RequestCashShopPurchase(link, 15, lines).has_value());
  const std::size_t length = ro::proto::CzSePcBuyCashitemList::kWireSize +
                             2 * ro::proto::CzSePcBuyCashitemListSub::kWireSize;
  const std::vector<std::uint8_t> buy = server.Receive(length);
  REQUIRE(buy.size() == length);
  CHECK(ReadU16(buy, 0) == ro::proto::CzSePcBuyCashitemList::kOpcode);
  CHECK(ReadU16(buy, 2) == length);
  CHECK(ReadU16(buy, 4) == 2);
  CHECK(ReadU32(buy, 6) == 15);
  CHECK(ReadU32(buy, 10) == 5000);
  CHECK(ReadU32(buy, 14) == 2);
  CHECK(ReadU16(buy, 18) == 6);
  CHECK(ReadU32(buy, 20) == 5001);
  CHECK(ReadU16(buy, 28) == 7);
}
