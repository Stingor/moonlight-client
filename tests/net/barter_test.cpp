// LE TROC, DÉCODÉ — et ses verbes, sur le fil.
//
// Ces trames sont fabriquées octet par octet d'après `packets.hpp` /
// `packets_struct.hpp`. Le banc porte ce qu'aucune session ne montrerait :
//
//   · le résultat d'achat, que la boutique à zeny reçoit par le même paquet,
//     n'est pris QUE lorsque le troc l'attend ;
//   · l'achat part AVANT la fermeture, et chacun dans sa génération ;
//   · les changements de carte ferment sans être consommés, et sans rien dire.

#include "ro/net/barter.h"

#include <cstddef>
#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/connection.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/barter.h"

#include "support/loopback_link.h"
#include "support/loopback_server.h"
#include "support/wire_bytes.h"

using ro::net::ApplyBarterFrame;
using ro::net::Frame;
using ro::net::FrameOutcome;
using ro::sim::Barter;
using ro::sim::BarterGeneration;
using ro::test::Connect;
using ro::test::LoopbackServer;
using ro::test_support::PutU16;
using ro::test_support::PutU32;
using ro::test_support::ReadU16;
using ro::test_support::ReadU32;

namespace {

void PatchLength(std::vector<std::uint8_t>& frame) {
  frame[2] = static_cast<std::uint8_t>(frame.size() & 0xFF);
  frame[3] = static_cast<std::uint8_t>(frame.size() >> 8);
}

// `ZC_NPC_BARTER_MARKET_ITEMINFO` : deux articles de trente et un octets.
[[nodiscard]] Frame SimpleListFrame() {
  std::vector<std::uint8_t> bytes;
  PutU16(bytes, 0x0B78);
  PutU16(bytes, 0);
  // Une potion, stock illimité, contre dix gelées.
  PutU32(bytes, 501);
  bytes.push_back(0);
  PutU32(bytes, 0xFFFFFFFF);
  PutU32(bytes, 909);
  PutU32(bytes, 10);
  PutU32(bytes, 70);
  PutU32(bytes, 1);
  PutU16(bytes, 0);
  PutU32(bytes, 0);
  // Une épée, trois en stock, contre deux dagues.
  PutU32(bytes, 1101);
  bytes.push_back(5);
  PutU32(bytes, 3);
  PutU32(bytes, 1201);
  PutU32(bytes, 2);
  PutU32(bytes, 500);
  PutU32(bytes, 2);
  PutU16(bytes, 12);
  PutU32(bytes, 2);
  PatchLength(bytes);
  return Frame{.opcode = 0x0B78, .bytes = bytes};
}

// `ZC_NPC_EXPANDED_BARTER_MARKET_ITEMINFO` : un article sans devise, un à deux.
[[nodiscard]] Frame ExpandedListFrame() {
  std::vector<std::uint8_t> bytes;
  PutU16(bytes, 0x0B79);
  PutU16(bytes, 0);
  PutU32(bytes, 2);
  const auto item = [&](std::uint32_t item_id, std::uint32_t shop_index, std::uint32_t zeny,
                        std::uint32_t currencies) {
    PutU32(bytes, item_id);
    PutU16(bytes, 5);
    PutU32(bytes, 4);
    PutU32(bytes, 500);
    PutU32(bytes, shop_index);
    PutU32(bytes, zeny);
    PutU16(bytes, 12);
    PutU32(bytes, 2);
    PutU32(bytes, currencies);
    PutU32(bytes, 7);
  };
  item(1101, 0, 250000, 0);
  item(1102, 1, 0, 2);
  PutU32(bytes, 909);
  PutU16(bytes, 0);
  PutU32(bytes, 10);
  PutU16(bytes, 3);
  PutU32(bytes, 1201);
  PutU16(bytes, 4);
  PutU32(bytes, 1);
  PutU16(bytes, 5);
  PatchLength(bytes);
  return Frame{.opcode = 0x0B79, .bytes = bytes};
}

[[nodiscard]] Frame ResultFrame(std::uint8_t code) {
  return Frame{.opcode = 0x00CA, .bytes = {0xCA, 0x00, code}};
}

}  // namespace

TEST_CASE("Troc simple : la liste ouvre, une devise par article", "[net][barter]") {
  Barter barter;
  REQUIRE(ApplyBarterFrame(SimpleListFrame(), barter) == FrameOutcome::kApplied);
  CHECK(barter.open());
  CHECK(barter.generation() == BarterGeneration::kSimple);
  REQUIRE(barter.items().size() == 2);
  const ro::sim::BarterItem& herb = barter.items()[0];
  CHECK(herb.item_id == 501);
  CHECK(herb.unlimited());
  CHECK(herb.weight == 70);
  CHECK(herb.shop_index == 1);
  REQUIRE(herb.currencies.size() == 1);
  CHECK(herb.currencies[0].item_id == 909);
  CHECK(herb.currencies[0].amount == 10);
  CHECK(herb.currencies[0].refine_level == ro::sim::kBarterAnyRefine);
  const ro::sim::BarterItem& sword = barter.items()[1];
  CHECK(sword.type == 5);
  CHECK(sword.stock == 3);
  CHECK(sword.view_sprite == 12);
  CHECK(sword.equip_location == 2);
  CHECK(sword.zeny == 0);

  SECTION("une trame coupée est indéchiffrable") {
    Frame cut = SimpleListFrame();
    cut.bytes.pop_back();
    PatchLength(cut.bytes);
    CHECK(ApplyBarterFrame(cut, barter) == FrameOutcome::kUndecodable);
  }
}

TEST_CASE("Troc étendu : la liste ouvre, zeny, raffinement et devises compris", "[net][barter]") {
  Barter barter;
  REQUIRE(ApplyBarterFrame(ExpandedListFrame(), barter) == FrameOutcome::kApplied);
  CHECK(barter.generation() == BarterGeneration::kExpanded);
  REQUIRE(barter.items().size() == 2);
  CHECK(barter.items()[0].zeny == 250000);
  CHECK(barter.items()[0].refine_level == 7);
  CHECK(barter.items()[0].currencies.empty());
  const ro::sim::BarterItem& second = barter.items()[1];
  CHECK(second.item_id == 1102);
  REQUIRE(second.currencies.size() == 2);
  CHECK(second.currencies[1].item_id == 1201);
  CHECK(second.currencies[1].refine_level == 4);
  CHECK(second.currencies[1].type == 5);

  SECTION("une trame qui ne tombe pas juste est indéchiffrable") {
    Frame cut = ExpandedListFrame();
    cut.bytes.resize(cut.bytes.size() - 2);
    PatchLength(cut.bytes);
    CHECK(ApplyBarterFrame(cut, barter) == FrameOutcome::kUndecodable);
  }
}

TEST_CASE("Troc : le résultat n'est pris que s'il est attendu", "[net][barter]") {
  Barter barter;
  // Aucun troc : c'est le résultat de la boutique à zeny.
  CHECK(ApplyBarterFrame(ResultFrame(0), barter) == FrameOutcome::kUnhandled);

  // Troc ouvert sans achat : toujours pas le sien.
  REQUIRE(ApplyBarterFrame(SimpleListFrame(), barter) == FrameOutcome::kApplied);
  CHECK(ApplyBarterFrame(ResultFrame(0), barter) == FrameOutcome::kUnhandled);
  CHECK(barter.result_seq() == 0);

  REQUIRE(barter.PutInCart(1, 2));
  barter.MarkPurchaseSent();
  REQUIRE(ApplyBarterFrame(ResultFrame(ro::sim::kBarterResultNotEnoughGoods), barter) ==
          FrameOutcome::kApplied);
  CHECK(barter.result() == ro::sim::kBarterResultNotEnoughGoods);
  CHECK(barter.result_seq() == 1);
  // Une fois reçu, le suivant revient à la boutique à zeny.
  CHECK(ApplyBarterFrame(ResultFrame(0), barter) == FrameOutcome::kUnhandled);
}

TEST_CASE("Troc : les changements de carte ferment sans être consommés ni dits", "[net][barter]") {
  Barter barter;
  REQUIRE(ApplyBarterFrame(ExpandedListFrame(), barter) == FrameOutcome::kApplied);
  const Frame map_move{.opcode = ro::proto::ZcNpcackMapmove::kOpcode, .bytes = {}};
  CHECK(ApplyBarterFrame(map_move, barter) == FrameOutcome::kUnhandled);
  CHECK_FALSE(barter.open());
  CHECK_FALSE(barter.TakeFarewell().has_value());

  REQUIRE(ApplyBarterFrame(SimpleListFrame(), barter) == FrameOutcome::kApplied);
  const Frame server_move{.opcode = ro::proto::ZcNpcackServermove::kOpcode, .bytes = {}};
  CHECK(ApplyBarterFrame(server_move, barter) == FrameOutcome::kUnhandled);
  CHECK_FALSE(barter.open());
  CHECK_FALSE(barter.TakeFarewell().has_value());
}

TEST_CASE("Troc simple : l'achat puis la fermeture, sur le fil", "[net][barter]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  ro::net::Connection link = Connect(server);

  Barter barter;
  REQUIRE(ApplyBarterFrame(SimpleListFrame(), barter) == FrameOutcome::kApplied);
  REQUIRE(barter.PutInCart(1, 20));
  REQUIRE(barter.PutInCart(2, 1));
  REQUIRE(ro::net::RequestBarterPurchase(link, barter).has_value());
  CHECK_FALSE(barter.open());
  CHECK(barter.awaiting_result());

  const std::size_t length = 4 + 2 * 14;
  const std::vector<std::uint8_t> buy = server.Receive(length);
  REQUIRE(buy.size() == length);
  CHECK(ReadU16(buy, 0) == 0x0B0F);
  CHECK(ReadU16(buy, 2) == length);
  CHECK(ReadU32(buy, 4) == 501);  // itemId
  CHECK(ReadU32(buy, 8) == 20);   // amount
  CHECK(ReadU16(buy, 12) == 0);   // invIndex : le serveur ne le lit pas
  CHECK(ReadU32(buy, 14) == 1);   // shopIndex
  CHECK(ReadU32(buy, 18) == 1101);
  CHECK(ReadU32(buy, 22) == 1);
  CHECK(ReadU32(buy, 28) == 2);

  // Un second achat, troc fermé, ne part pas.
  CHECK_FALSE(ro::net::RequestBarterPurchase(link, barter).has_value());

  REQUIRE(ro::net::SayBarterFarewells(link, barter).has_value());
  CHECK(server.Receive(2) == std::vector<std::uint8_t>{0x12, 0x0B});
  CHECK_FALSE(barter.TakeFarewell().has_value());
}

TEST_CASE("Troc étendu : l'achat puis la fermeture, sur le fil", "[net][barter]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  ro::net::Connection link = Connect(server);

  Barter barter;
  REQUIRE(ApplyBarterFrame(ExpandedListFrame(), barter) == FrameOutcome::kApplied);
  REQUIRE(barter.PutInCart(1, 3));
  REQUIRE(ro::net::RequestBarterPurchase(link, barter).has_value());

  const std::size_t length = 4 + 12;
  const std::vector<std::uint8_t> buy = server.Receive(length);
  REQUIRE(buy.size() == length);
  CHECK(ReadU16(buy, 0) == 0x0B57);
  CHECK(ReadU16(buy, 2) == length);
  CHECK(ReadU32(buy, 4) == 1102);  // itemId
  CHECK(ReadU32(buy, 8) == 1);     // shopIndex
  CHECK(ReadU32(buy, 12) == 3);    // amount

  REQUIRE(ro::net::SayBarterFarewells(link, barter).has_value());
  CHECK(server.Receive(2) == std::vector<std::uint8_t>{0x58, 0x0B});
}

TEST_CASE("Troc : l'achat refuse ce que le serveur ignorerait", "[net][barter]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  ro::net::Connection link = Connect(server);

  Barter barter;
  CHECK_FALSE(ro::net::RequestBarterPurchase(link, barter).has_value());
  REQUIRE(ApplyBarterFrame(SimpleListFrame(), barter) == FrameOutcome::kApplied);
  // Panier vide.
  CHECK_FALSE(ro::net::RequestBarterPurchase(link, barter).has_value());
  CHECK(barter.open());
  CHECK_FALSE(barter.awaiting_result());
}

TEST_CASE("Troc : les deux fermetures, chacune son opcode", "[net][barter]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  ro::net::Connection link = Connect(server);

  Barter barter;
  REQUIRE(ApplyBarterFrame(SimpleListFrame(), barter) == FrameOutcome::kApplied);
  REQUIRE(ApplyBarterFrame(ExpandedListFrame(), barter) == FrameOutcome::kApplied);
  barter.Close();
  REQUIRE(ro::net::SayBarterFarewells(link, barter).has_value());
  CHECK(server.Receive(4) == std::vector<std::uint8_t>{0x12, 0x0B, 0x58, 0x0B});
}
