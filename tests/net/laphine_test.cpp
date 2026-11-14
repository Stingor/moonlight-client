// LE LAPHINE, DÉCODÉ — et ses verbes, sur le fil. Les trames suivent `packets_struct.hpp`
// du serveur, octet par octet.

#include "ro/net/laphine.h"

#include <cstddef>
#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/connection.h"
#include "ro/proto/item_option_packets.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/inventory.h"
#include "ro/sim/laphine.h"

#include "support/loopback_link.h"
#include "support/loopback_server.h"
#include "support/wire_bytes.h"

using ro::net::ApplyLaphineFrame;
using ro::net::Frame;
using ro::net::FrameOutcome;
using ro::sim::Inventory;
using ro::sim::InventoryItem;
using ro::sim::Laphine;
using ro::sim::LaphineKind;
using ro::sim::LaphineRecipe;
using ro::test::Connect;
using ro::test::LoopbackServer;
using ro::test_support::PutU16;
using ro::test_support::PutU32;
using ro::test_support::ReadU16;
using ro::test_support::ReadU32;

namespace {

constexpr std::uint32_t kStoneId = 100003;
constexpr std::uint32_t kSwordId = 28725;
constexpr std::uint32_t kHerbId = 501;
constexpr std::uint16_t kSwordSlot = 12;
constexpr std::uint16_t kHerbSlot = 13;

[[nodiscard]] Frame OpenFrame(std::uint16_t opcode, std::uint32_t item_id) {
  std::vector<std::uint8_t> bytes;
  PutU16(bytes, opcode);
  PutU32(bytes, item_id);
  return Frame{.opcode = opcode, .bytes = bytes};
}

[[nodiscard]] Frame ResultFrame(std::uint16_t opcode, std::uint16_t result) {
  std::vector<std::uint8_t> bytes;
  PutU16(bytes, opcode);
  PutU16(bytes, result);
  return Frame{.opcode = opcode, .bytes = bytes};
}

[[nodiscard]] Inventory BagWithSword() {
  Inventory bag;
  bag.Put(InventoryItem{
      .client_slot = kSwordSlot, .item_id = kSwordId, .amount = 1, .identified = true});
  return bag;
}

}  // namespace

TEST_CASE("Laphine : chaque espèce s'ouvre par son paquet", "[net][laphine]") {
  Laphine laphine;
  Inventory bag;
  REQUIRE(ApplyLaphineFrame(OpenFrame(0x0A4E, kStoneId), laphine, bag) == FrameOutcome::kApplied);
  CHECK(laphine.open());
  CHECK(laphine.kind() == LaphineKind::kSynthesis);
  CHECK(laphine.trigger_item_id() == kStoneId);

  REQUIRE(ApplyLaphineFrame(OpenFrame(0x0AB4, kStoneId), laphine, bag) == FrameOutcome::kApplied);
  CHECK(laphine.kind() == LaphineKind::kUpgrade);
}

TEST_CASE("Laphine : le résultat d'une synthèse", "[net][laphine]") {
  Laphine laphine;
  Inventory bag;
  REQUIRE(ApplyLaphineFrame(OpenFrame(0x0A4E, kStoneId), laphine, bag) == FrameOutcome::kApplied);
  REQUIRE(ApplyLaphineFrame(ResultFrame(0x0A50, 7), laphine, bag) == FrameOutcome::kApplied);
  CHECK(laphine.open());
  CHECK(laphine.result() == 7);
  REQUIRE(ApplyLaphineFrame(ResultFrame(0x0A50, 0), laphine, bag) == FrameOutcome::kApplied);
  CHECK_FALSE(laphine.open());
}

TEST_CASE("Laphine : le résultat d'une amélioration", "[net][laphine]") {
  Laphine laphine;
  Inventory bag;
  REQUIRE(ApplyLaphineFrame(OpenFrame(0x0AB4, kStoneId), laphine, bag) == FrameOutcome::kApplied);
  REQUIRE(ApplyLaphineFrame(ResultFrame(0x0AB7, 1), laphine, bag) == FrameOutcome::kApplied);
  CHECK(laphine.open());
  REQUIRE(ApplyLaphineFrame(ResultFrame(0x0AB7, 0), laphine, bag) == FrameOutcome::kApplied);
  CHECK_FALSE(laphine.open());
}

TEST_CASE("Laphine : la pièce changée s'écrit au sac et se désigne", "[net][laphine]") {
  Laphine laphine;
  Inventory bag = BagWithSword();
  REQUIRE(ApplyLaphineFrame(OpenFrame(0x0AB4, kStoneId), laphine, bag) == FrameOutcome::kApplied);

  ro::proto::ItemOptionChange change;
  change.client_slot = kSwordSlot;
  change.refine_level = 7;
  change.options[0] = {.index = 42, .value = 5, .param = 1};
  const Frame frame{.opcode = ro::proto::ItemOptionChange::kOpcode,
                    .bytes = ro::proto::EncodeItemOptionChange(change)};
  REQUIRE(ApplyLaphineFrame(frame, laphine, bag) == FrameOutcome::kApplied);

  const InventoryItem* sword = bag.Find(kSwordSlot);
  REQUIRE(sword != nullptr);
  CHECK(sword->refine_level == 7);
  CHECK(sword->options[0].id == 42);
  CHECK(sword->options[0].value == 5);
  CHECK(laphine.TakeUpgraded() == kSwordSlot);

  // Une case vide n'est pas une panne : la trame est en retard.
  change.client_slot = 99;
  const Frame late{.opcode = ro::proto::ItemOptionChange::kOpcode,
                   .bytes = ro::proto::EncodeItemOptionChange(change)};
  CHECK(ApplyLaphineFrame(late, laphine, bag) == FrameOutcome::kIgnored);
}

TEST_CASE("Laphine : un changement de carte ferme sans être pris ni dit", "[net][laphine]") {
  Laphine laphine;
  Inventory bag;
  REQUIRE(ApplyLaphineFrame(OpenFrame(0x0AB4, kStoneId), laphine, bag) == FrameOutcome::kApplied);
  const Frame map_move{.opcode = ro::proto::ZcNpcackMapmove::kOpcode, .bytes = {}};
  CHECK(ApplyLaphineFrame(map_move, laphine, bag) == FrameOutcome::kUnhandled);
  CHECK_FALSE(laphine.open());
  CHECK_FALSE(laphine.TakeFarewell().has_value());
}

TEST_CASE("Laphine : l'amélioration sur le fil", "[net][laphine]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  ro::net::Connection link = Connect(server);

  const LaphineRecipe recipe{
      .trigger_item_id = kStoneId, .most_refine = 10, .materials = {{.item_id = kSwordId}}};
  Laphine laphine;
  Inventory bag = BagWithSword();
  laphine.TellOpening(LaphineKind::kUpgrade, kStoneId);

  // Rien de déposé : rien ne part.
  CHECK_FALSE(ro::net::RequestLaphineMake(link, laphine, recipe, bag).has_value());
  REQUIRE(laphine.Deposit(kSwordSlot));
  REQUIRE(ro::net::RequestLaphineMake(link, laphine, recipe, bag).has_value());
  CHECK(laphine.waiting());

  const std::vector<std::uint8_t> request = server.Receive(8);
  REQUIRE(request.size() == 8);
  CHECK(ReadU16(request, 0) == 0x0AB6);
  CHECK(ReadU32(request, 2) == kStoneId);
  CHECK(ReadU16(request, 6) == kSwordSlot);

  // 🔴 LA RÉUSSITE AUSSI SE FERME CHEZ LE SERVEUR.
  laphine.TellResult(ro::sim::kLaphineUpgradeSucceeded);
  ro::net::SayLaphineFarewells(link, laphine);
  const std::vector<std::uint8_t> farewell = server.Receive(2);
  REQUIRE(farewell.size() == 2);
  CHECK(ReadU16(farewell, 0) == 0x0AB5);
}

TEST_CASE("Laphine : la fermeture précède l'usage d'un autre déclencheur", "[net][laphine]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  ro::net::Connection link = Connect(server);

  Laphine laphine;
  Inventory bag;
  REQUIRE(ApplyLaphineFrame(OpenFrame(0x0AB4, kStoneId), laphine, bag) == FrameOutcome::kApplied);
  REQUIRE(laphine.GiveWayToTrigger());
  ro::net::SayLaphineFarewells(link, laphine);
  const std::vector<std::uint8_t> farewell = server.Receive(2);
  REQUIRE(farewell.size() == 2);
  CHECK(ReadU16(farewell, 0) == 0x0AB5);
}

TEST_CASE("Laphine : la synthèse sur le fil dit la quantité de la recette", "[net][laphine]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  ro::net::Connection link = Connect(server);

  const LaphineRecipe recipe{
      .trigger_item_id = kStoneId,
      .needed_piles = 2,
      .most_refine = 10,
      .materials = {{.item_id = kSwordId}, {.item_id = kHerbId, .amount = 5}}};
  Laphine laphine;
  Inventory bag = BagWithSword();
  bag.Put(InventoryItem{
      .client_slot = kHerbSlot, .item_id = kHerbId, .amount = 20, .identified = true});
  laphine.TellOpening(LaphineKind::kSynthesis, kStoneId);
  REQUIRE(laphine.Deposit(kSwordSlot));
  REQUIRE(laphine.Deposit(kHerbSlot));
  REQUIRE(ro::net::RequestLaphineMake(link, laphine, recipe, bag).has_value());

  constexpr std::size_t kLength = 8 + 2 * 4;
  const std::vector<std::uint8_t> request = server.Receive(kLength);
  REQUIRE(request.size() == kLength);
  CHECK(ReadU16(request, 0) == 0x0A4F);
  CHECK(ReadU16(request, 2) == kLength);
  CHECK(ReadU32(request, 4) == kStoneId);
  CHECK(ReadU16(request, 8) == kSwordSlot);
  CHECK(ReadU16(request, 10) == 1);
  CHECK(ReadU16(request, 12) == kHerbSlot);
  CHECK(ReadU16(request, 14) == 5);

  laphine.Close();
  ro::net::SayLaphineFarewells(link, laphine);
  const std::vector<std::uint8_t> farewell = server.Receive(2);
  REQUIRE(farewell.size() == 2);
  CHECK(ReadU16(farewell, 0) == 0x0A70);
}
