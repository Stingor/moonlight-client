// L'ENCHANTEMENT, DÉCODÉ — et ses verbes, sur le fil. Les trames suivent
// `packets_struct.hpp` du serveur, octet par octet.

#include "ro/net/item_enchant.h"

#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/connection.h"
#include "ro/sim/inventory.h"
#include "ro/sim/item_enchant.h"

#include "support/loopback_link.h"
#include "support/loopback_server.h"
#include "support/wire_bytes.h"

using ro::net::ApplyItemEnchantFrame;
using ro::net::Frame;
using ro::net::FrameOutcome;
using ro::sim::Inventory;
using ro::sim::InventoryItem;
using ro::sim::ItemEnchant;
using ro::sim::ItemEnchantGroup;
using ro::test::Connect;
using ro::test::LoopbackServer;
using ro::test_support::PutU16;
using ro::test_support::PutU32;
using ro::test_support::PutU64;
using ro::test_support::ReadU16;
using ro::test_support::ReadU32;

namespace {

constexpr std::uint16_t kUiOpenOpcode = 0x0B9A;
constexpr std::uint16_t kRandomOpcode = 0x0B9B;
constexpr std::uint16_t kResponseOpcode = 0x0B9F;
constexpr std::uint16_t kCloseOpcode = 0x0BA0;
constexpr std::uint8_t kGradeUiType = 8;
constexpr std::uint64_t kGroupId = 26;
constexpr std::uint32_t kArmorId = 450206;
constexpr std::uint32_t kOrbId = 311092;
constexpr std::uint16_t kArmorSlot = 12;
constexpr std::uint8_t kCardSlots = 1;
constexpr std::int64_t kRich = 1000000;

[[nodiscard]] Frame OpenFrame(std::uint8_t ui_type, std::uint64_t data) {
  std::vector<std::uint8_t> bytes;
  PutU16(bytes, kUiOpenOpcode);
  bytes.push_back(ui_type);
  PutU64(bytes, data);
  return Frame{.opcode = kUiOpenOpcode, .bytes = bytes};
}

[[nodiscard]] Frame ResponseFrame(std::uint32_t message_id, std::uint32_t item_id) {
  std::vector<std::uint8_t> bytes;
  PutU16(bytes, kResponseOpcode);
  PutU32(bytes, message_id);
  PutU32(bytes, item_id);
  return Frame{.opcode = kResponseOpcode, .bytes = bytes};
}

[[nodiscard]] ItemEnchantGroup Group() {
  ItemEnchantGroup group{.group_id = kGroupId, .target_item_ids = {kArmorId}, .order = {3}};
  group.slots.push_back(ro::sim::ItemEnchantSlot{
      .slot = 3, .draws = {{.grade = 0, .enchants = {{.item_id = kOrbId, .odds = 100000}}}}});
  return group;
}

}  // namespace

TEST_CASE("Enchantement : une autre interface ne lui appartient pas", "[net][item_enchant]") {
  ItemEnchant enchant;
  Inventory bag;
  CHECK(ApplyItemEnchantFrame(OpenFrame(kGradeUiType, 0), enchant, bag) ==
        FrameOutcome::kUnhandled);
  CHECK_FALSE(enchant.open());
  REQUIRE(ApplyItemEnchantFrame(OpenFrame(ro::sim::kItemEnchantUiType, kGroupId), enchant, bag) ==
          FrameOutcome::kApplied);
  CHECK(enchant.open());
  CHECK(enchant.group_id() == kGroupId);
}

TEST_CASE("Enchantement : la demande part, la réussite pose l'enchantement au sac",
          "[net][item_enchant]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  ro::net::Connection link = Connect(server);
  Inventory bag;
  bag.Put(InventoryItem{
      .client_slot = kArmorSlot, .item_id = kArmorId, .amount = 1, .identified = true});
  ItemEnchant enchant;
  enchant.TellOpening(kGroupId);
  const ItemEnchantGroup group = Group();
  const ro::sim::ItemEnchantRequest random{.mode = ro::sim::ItemEnchantMode::kRandom};
  // Rien de choisi : rien ne part.
  CHECK_FALSE(ro::net::RequestItemEnchant(link, enchant, group, random, bag, kCardSlots, kRich)
                  .has_value());
  REQUIRE(enchant.Choose(kArmorSlot));
  REQUIRE(ro::net::RequestItemEnchant(link, enchant, group, random, bag, kCardSlots, kRich)
              .has_value());
  CHECK(enchant.waiting());
  const std::vector<std::uint8_t> request = server.Receive(12);
  REQUIRE(request.size() == 12);
  CHECK(ReadU16(request, 0) == kRandomOpcode);
  CHECK(ReadU32(request, 2) == kGroupId);
  CHECK(ReadU32(request, 6) == 0);
  CHECK(ReadU16(request, 10) == kArmorSlot);

  REQUIRE(ApplyItemEnchantFrame(ResponseFrame(ro::sim::kItemEnchantSucceededMessage, kOrbId),
                                enchant, bag) == FrameOutcome::kApplied);
  CHECK_FALSE(enchant.open());
  CHECK(bag.Find(kArmorSlot)->cards[3] == kOrbId);
}

TEST_CASE("Enchantement : la fermeture du joueur part sur le fil", "[net][item_enchant]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  ro::net::Connection link = Connect(server);
  ItemEnchant enchant;
  enchant.TellOpening(kGroupId);
  enchant.Close();
  ro::net::SayItemEnchantFarewell(link, enchant);
  const std::vector<std::uint8_t> farewell = server.Receive(2);
  REQUIRE(farewell.size() == 2);
  CHECK(ReadU16(farewell, 0) == kCloseOpcode);
}
