// LE TROC SUR LE FIL : les deux listes et les deux achats, octet par octet
// d'après `packets.hpp` / `packets_struct.hpp` du serveur.
//
// 🔴 LE BANC PORTE LA PREUVE QUE LE DÉCODEUR ENGENDRÉ DE LA LISTE ÉTENDUE MENT
// EN SILENCE : sur trois articles portant zéro, deux et une devise, la trame
// fait exactement trois fois quarante-huit octets après l'en-tête — le pas qu'il
// suppose. Il l'accepte donc, et rend un deuxième article fait des devises du
// premier.

#include "ro/proto/barter_packets.h"

#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/proto/map_packets.h"

#include "support/wire_bytes.h"

using ro::proto::DecodeExpandedBarterItemList;
using ro::proto::EncodeExpandedBarterItemList;
using ro::proto::ExpandedBarterCurrency;
using ro::proto::ExpandedBarterItem;
using ro::proto::ExpandedBarterItemList;
using ro::test_support::PutU16;
using ro::test_support::PutU32;
using ro::test_support::ReadU16;
using ro::test_support::ReadU32;

namespace {

void PatchLength(std::vector<std::uint8_t>& frame) {
  frame[2] = static_cast<std::uint8_t>(frame.size() & 0xFF);
  frame[3] = static_cast<std::uint8_t>(frame.size() >> 8);
}

// La partie fixe d'un article étendu, dans l'ordre de
// `PACKET_ZC_NPC_EXPANDED_BARTER_MARKET_ITEMINFO_sub` (branche 20250402).
void PutExpandedItem(std::vector<std::uint8_t>& frame, std::uint32_t item_id,
                     std::uint32_t currency_count) {
  PutU32(frame, item_id);         // nameid        @0
  PutU16(frame, 5);               // type          @4
  PutU32(frame, 0xFFFFFFFF);      // amount        @6
  PutU32(frame, 1500);            // weight        @10
  PutU32(frame, item_id % 100);   // index         @14
  PutU32(frame, 25000);           // zeny          @18
  PutU16(frame, 12);              // viewSprite    @22
  PutU32(frame, 2);               // location      @24
  PutU32(frame, currency_count);  // currency_count @28
  PutU32(frame, 7);               // refine_level  @32
}

void PutCurrency(std::vector<std::uint8_t>& frame, std::uint32_t item_id, std::uint16_t refine,
                 std::uint32_t amount) {
  PutU32(frame, item_id);  // nameid        @0
  PutU16(frame, refine);   // refine_level  @4
  PutU32(frame, amount);   // amount        @6
  PutU16(frame, 3);        // type          @10
}

// Trois articles : sans devise, avec deux, avec une.
[[nodiscard]] std::vector<std::uint8_t> ThreeItemFrame() {
  std::vector<std::uint8_t> frame;
  PutU16(frame, 0x0B79);
  PutU16(frame, 0);
  PutU32(frame, 3);
  PutExpandedItem(frame, 1201, 0);
  PutExpandedItem(frame, 1302, 2);
  PutCurrency(frame, 909, 0, 10);
  PutCurrency(frame, 1101, 4, 1);
  PutExpandedItem(frame, 2303, 1);
  PutCurrency(frame, 7539, 0, 3);
  PatchLength(frame);
  return frame;
}

}  // namespace

TEST_CASE("Troc étendu : chaque article porte son propre nombre de devises", "[proto][barter]") {
  const std::vector<std::uint8_t> frame = ThreeItemFrame();
  REQUIRE(frame.size() == 8 + 3 * 36 + 3 * 12);

  const auto list = DecodeExpandedBarterItemList(frame);
  REQUIRE(list.has_value());
  CHECK(list->announced_count == 3);
  REQUIRE(list->items.size() == 3);

  const ExpandedBarterItem& bare = list->items[0];
  CHECK(bare.item_id == 1201);
  CHECK(bare.type == 5);
  CHECK(bare.stock == 0xFFFFFFFF);
  CHECK(bare.weight == 1500);
  CHECK(bare.shop_index == 1);
  CHECK(bare.zeny == 25000);
  CHECK(bare.view_sprite == 12);
  CHECK(bare.location == 2);
  CHECK(bare.refine_level == 7);
  CHECK(bare.currencies.empty());

  const ExpandedBarterItem& two = list->items[1];
  CHECK(two.item_id == 1302);
  REQUIRE(two.currencies.size() == 2);
  CHECK(two.currencies[0].item_id == 909);
  CHECK(two.currencies[0].refine_level == 0);
  CHECK(two.currencies[0].amount == 10);
  CHECK(two.currencies[0].type == 3);
  CHECK(two.currencies[1].item_id == 1101);
  CHECK(two.currencies[1].refine_level == 4);
  CHECK(two.currencies[1].amount == 1);

  const ExpandedBarterItem& one = list->items[2];
  CHECK(one.item_id == 2303);
  REQUIRE(one.currencies.size() == 1);
  CHECK(one.currencies[0].item_id == 7539);
  CHECK(one.currencies[0].amount == 3);
}

TEST_CASE("Troc étendu : une trame qui ne tombe pas juste est refusée", "[proto][barter]") {
  SECTION("longueur annoncée fausse") {
    std::vector<std::uint8_t> frame = ThreeItemFrame();
    frame[2] = static_cast<std::uint8_t>(frame[2] + 1);
    CHECK_FALSE(DecodeExpandedBarterItemList(frame).has_value());
  }
  SECTION("devises coupées") {
    std::vector<std::uint8_t> frame = ThreeItemFrame();
    frame.resize(frame.size() - 4);
    PatchLength(frame);
    CHECK_FALSE(DecodeExpandedBarterItemList(frame).has_value());
  }
  SECTION("reste plus court qu'un article") {
    std::vector<std::uint8_t> frame = ThreeItemFrame();
    PutU32(frame, 0);
    PatchLength(frame);
    CHECK_FALSE(DecodeExpandedBarterItemList(frame).has_value());
  }
  SECTION("compte de devises démesuré") {
    std::vector<std::uint8_t> frame;
    PutU16(frame, 0x0B79);
    PutU16(frame, 0);
    PutU32(frame, 1);
    PutExpandedItem(frame, 1201, 0xFFFFFFFF);
    PatchLength(frame);
    CHECK_FALSE(DecodeExpandedBarterItemList(frame).has_value());
  }
  SECTION("autre opcode") {
    std::vector<std::uint8_t> frame = ThreeItemFrame();
    frame[0] = 0x78;
    CHECK_FALSE(DecodeExpandedBarterItemList(frame).has_value());
  }
}

TEST_CASE("Troc étendu : une liste vide est une liste", "[proto][barter]") {
  std::vector<std::uint8_t> frame;
  PutU16(frame, 0x0B79);
  PutU16(frame, 8);
  PutU32(frame, 0);
  const auto list = DecodeExpandedBarterItemList(frame);
  REQUIRE(list.has_value());
  CHECK(list->items.empty());
}

TEST_CASE("Troc étendu : l'encodeur d'essai est l'envers du décodeur", "[proto][barter]") {
  ExpandedBarterItemList list;
  list.announced_count = 1;
  ExpandedBarterItem item{.item_id = 1201,
                          .type = 5,
                          .stock = 4,
                          .weight = 1500,
                          .shop_index = 3,
                          .zeny = 25000,
                          .view_sprite = 12,
                          .location = 2,
                          .refine_level = 7};
  item.currencies.push_back(
      ExpandedBarterCurrency{.item_id = 909, .refine_level = 0, .amount = 10, .type = 3});
  list.items.push_back(item);
  const std::vector<std::uint8_t> frame = EncodeExpandedBarterItemList(list);
  CHECK(frame.size() == 8 + 36 + 12);
  CHECK(ReadU16(frame, 2) == frame.size());
  const auto back = DecodeExpandedBarterItemList(frame);
  REQUIRE(back.has_value());
  REQUIRE(back->items.size() == 1);
  CHECK(back->items[0].stock == 4);
  CHECK(back->items[0].currencies[0].item_id == 909);
}

TEST_CASE("Troc simple : la liste engendrée suit le pas de trente et un octets",
          "[proto][barter]") {
  std::vector<std::uint8_t> frame;
  PutU16(frame, 0x0B78);
  PutU16(frame, 0);
  PutU32(frame, 1201);  // nameid          @0
  frame.push_back(5);   // type            @4
  PutU32(frame, 7);     // amount          @5
  PutU32(frame, 909);   // currencyNameid  @9
  PutU32(frame, 10);    // currencyAmount  @13
  PutU32(frame, 1500);  // weight          @17
  PutU32(frame, 3);     // index           @21
  PutU16(frame, 12);    // viewSprite      @25
  PutU32(frame, 2);     // location        @27
  PatchLength(frame);
  REQUIRE(frame.size() == 4 + 31);

  ro::proto::ZcNpcBarterMarketIteminfo list;
  REQUIRE(ro::proto::Decode(frame, list));
  REQUIRE(list.list.size() == 1);
  CHECK(list.list[0].nameid == 1201);
  CHECK(list.list[0].type == 5);
  CHECK(list.list[0].amount == 7);
  CHECK(list.list[0].currencyNameid == 909);
  CHECK(list.list[0].currencyAmount == 10);
  CHECK(list.list[0].weight == 1500);
  CHECK(list.list[0].index == 3);
  CHECK(list.list[0].viewSprite == 12);
  CHECK(list.list[0].location == 2);
}

TEST_CASE("Troc : les deux achats engendrés, sur le fil", "[proto][barter]") {
  SECTION("simple : objet, quantité, index d'inventaire, index de boutique") {
    ro::proto::CzNpcBarterMarketPurchase request;
    request.list.push_back({.itemId = 1201, .amount = 2, .invIndex = 0, .shopIndex = 3});
    const std::vector<std::uint8_t> frame = ro::proto::Encode(request);
    REQUIRE(frame.size() == 4 + 14);
    CHECK(ReadU16(frame, 0) == 0x0B0F);
    CHECK(ReadU16(frame, 2) == 18);
    CHECK(ReadU32(frame, 4) == 1201);
    CHECK(ReadU32(frame, 8) == 2);
    CHECK(ReadU16(frame, 12) == 0);
    CHECK(ReadU32(frame, 14) == 3);
  }
  SECTION("étendu : objet, index de boutique, quantité") {
    ro::proto::CzNpcExpandedBarterMarketPurchase request;
    request.list.push_back({.itemId = 1201, .shopIndex = 3, .amount = 2});
    const std::vector<std::uint8_t> frame = ro::proto::Encode(request);
    REQUIRE(frame.size() == 4 + 12);
    CHECK(ReadU16(frame, 0) == 0x0B57);
    CHECK(ReadU16(frame, 2) == 16);
    CHECK(ReadU32(frame, 4) == 1201);
    CHECK(ReadU32(frame, 8) == 3);
    CHECK(ReadU32(frame, 12) == 2);
  }
}
