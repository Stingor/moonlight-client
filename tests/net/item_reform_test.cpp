// LA RÉFORME, DÉCODÉE — et ses verbes, sur le fil. Les trames suivent `packets_struct.hpp`
// du serveur, octet par octet.

#include "ro/net/item_reform.h"

#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/connection.h"
#include "ro/sim/inventory.h"
#include "ro/sim/item_reform.h"

#include "support/loopback_link.h"
#include "support/loopback_server.h"
#include "support/wire_bytes.h"

using ro::net::ApplyItemReformFrame;
using ro::net::Frame;
using ro::net::FrameOutcome;
using ro::sim::Inventory;
using ro::sim::InventoryItem;
using ro::sim::ItemReform;
using ro::sim::ItemReformRecipe;
using ro::sim::ItemReformTrigger;
using ro::test::Connect;
using ro::test::LoopbackServer;
using ro::test_support::PutU16;
using ro::test_support::PutU32;
using ro::test_support::ReadU16;
using ro::test_support::ReadU32;

namespace {

constexpr std::uint16_t kOpenOpcode = 0x0B8F;
constexpr std::uint16_t kCloseOpcode = 0x0B90;
constexpr std::uint16_t kReformOpcode = 0x0B91;
constexpr std::uint16_t kAckOpcode = 0x0B92;
constexpr std::uint32_t kRitualId = 100648;
constexpr std::uint32_t kAdulterId = 1499;
constexpr std::uint16_t kSwordSlot = 12;

[[nodiscard]] Frame OpenFrame(std::uint32_t item_id) {
  std::vector<std::uint8_t> bytes;
  PutU16(bytes, kOpenOpcode);
  PutU32(bytes, item_id);
  return Frame{.opcode = kOpenOpcode, .bytes = bytes};
}

[[nodiscard]] Frame AckFrame(std::uint16_t slot, std::uint8_t result) {
  std::vector<std::uint8_t> bytes;
  PutU16(bytes, kAckOpcode);
  PutU16(bytes, slot);
  bytes.push_back(result);
  return Frame{.opcode = kAckOpcode, .bytes = bytes};
}

}  // namespace

TEST_CASE("Réforme : l'ouverture et le résultat", "[net][item_reform]") {
  ItemReform reform;
  REQUIRE(ApplyItemReformFrame(OpenFrame(kRitualId), reform) == FrameOutcome::kApplied);
  CHECK(reform.open());
  CHECK(reform.trigger_item_id() == kRitualId);
  REQUIRE(ApplyItemReformFrame(AckFrame(kSwordSlot, 0), reform) == FrameOutcome::kApplied);
  CHECK_FALSE(reform.open());
  CHECK(reform.TakeReformed() == kSwordSlot);
}

TEST_CASE("Réforme : la demande et la fermeture partent sur le fil", "[net][item_reform]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  ro::net::Connection link = Connect(server);
  const ItemReformTrigger trigger{
      .trigger_item_id = kRitualId,
      .refine_cap = 10,
      .recipes = {ItemReformRecipe{.base_item_id = kAdulterId, .most_refine = 10}}};
  Inventory bag;
  bag.Put(InventoryItem{
      .client_slot = kSwordSlot, .item_id = kAdulterId, .amount = 1, .identified = true});
  ItemReform reform;
  reform.TellOpening(kRitualId);
  CHECK_FALSE(ro::net::RequestItemReform(link, reform, trigger, bag).has_value());
  REQUIRE(reform.Choose(kSwordSlot));
  REQUIRE(ro::net::RequestItemReform(link, reform, trigger, bag).has_value());
  CHECK(reform.waiting());
  const std::vector<std::uint8_t> request = server.Receive(8);
  REQUIRE(request.size() == 8);
  CHECK(ReadU16(request, 0) == kReformOpcode);
  CHECK(ReadU32(request, 2) == kRitualId);
  CHECK(ReadU16(request, 6) == kSwordSlot);

  reform.Close();
  ro::net::SayItemReformFarewell(link, reform);
  const std::vector<std::uint8_t> farewell = server.Receive(2);
  REQUIRE(farewell.size() == 2);
  CHECK(ReadU16(farewell, 0) == kCloseOpcode);
}
