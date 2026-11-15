// L'AMÉLIORATION DE GRADE, DÉCODÉE — et ses verbes, sur le fil. Les trames suivent
// `packets_struct.hpp` du serveur, octet par octet.

#include "ro/net/grade_enchant.h"

#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/connection.h"
#include "ro/sim/grade_enchant.h"
#include "ro/sim/inventory.h"

#include "support/loopback_link.h"
#include "support/loopback_server.h"
#include "support/wire_bytes.h"

using ro::net::ApplyGradeEnchantFrame;
using ro::net::Frame;
using ro::net::FrameOutcome;
using ro::sim::GradeEnchant;
using ro::sim::Inventory;
using ro::sim::InventoryItem;
using ro::test::Connect;
using ro::test::LoopbackServer;
using ro::test_support::PutU16;
using ro::test_support::PutU32;
using ro::test_support::ReadU16;
using ro::test_support::ReadU32;

namespace {

constexpr std::uint16_t kUiOpenOpcode = 0x0AE2;
constexpr std::uint16_t kSelectOpcode = 0x0B59;
constexpr std::uint16_t kOfferOpcode = 0x0B5A;
constexpr std::uint16_t kRequestOpcode = 0x0B5B;
constexpr std::uint16_t kCloseOpcode = 0x0B5C;
constexpr std::uint16_t kAckOpcode = 0x0B5D;
constexpr std::uint16_t kSwordSlot = 12;
constexpr std::uint16_t kJewelSlot = 20;
constexpr std::uint32_t kSwordId = 1116;
constexpr std::uint32_t kTopazId = 1000326;
constexpr std::uint32_t kDustId = 1000337;
constexpr std::uint8_t kGradeUi = 8;
constexpr std::uint8_t kBankUi = 0;

[[nodiscard]] Frame UiOpenFrame(std::uint8_t ui_type) {
  std::vector<std::uint8_t> bytes;
  PutU16(bytes, kUiOpenOpcode);
  bytes.push_back(ui_type);
  PutU32(bytes, 0);
  return Frame{.opcode = kUiOpenOpcode, .bytes = bytes};
}

void PutStake(std::vector<std::uint8_t>& bytes, std::uint32_t amount, std::uint32_t zeny,
              bool breaks) {
  PutU32(bytes, kTopazId);
  PutU32(bytes, amount);
  PutU32(bytes, zeny);
  PutU32(bytes, 0);
  bytes.push_back(breaks ? 1 : 0);
}

[[nodiscard]] Frame OfferFrame(std::int16_t slot, std::uint32_t chance) {
  std::vector<std::uint8_t> bytes;
  PutU16(bytes, kOfferOpcode);
  PutU16(bytes, 0);
  PutU16(bytes, static_cast<std::uint16_t>(slot));
  PutU32(bytes, chance);
  PutU32(bytes, kDustId);
  PutU32(bytes, 3);
  PutU32(bytes, 10);
  PutU32(bytes, 1);
  PutU32(bytes, 0);
  PutU32(bytes, 0);
  PutStake(bytes, 1, 175000, true);
  PutStake(bytes, 5, 875000, false);
  bytes[2] = static_cast<std::uint8_t>(bytes.size() & 0xFF);
  bytes[3] = static_cast<std::uint8_t>(bytes.size() >> 8);
  return Frame{.opcode = kOfferOpcode, .bytes = bytes};
}

[[nodiscard]] Frame AckFrame(std::uint16_t slot, std::uint16_t grade, std::uint32_t result) {
  std::vector<std::uint8_t> bytes;
  PutU16(bytes, kAckOpcode);
  PutU16(bytes, slot);
  PutU16(bytes, grade);
  PutU32(bytes, result);
  return Frame{.opcode = kAckOpcode, .bytes = bytes};
}

[[nodiscard]] Inventory Bag() {
  Inventory bag;
  bag.Put(InventoryItem{.client_slot = kSwordSlot,
                        .item_id = kSwordId,
                        .type = ro::sim::kGradeWeaponType,
                        .amount = 1,
                        .refine_level = 7,
                        .identified = true});
  bag.Put(InventoryItem{.client_slot = kJewelSlot, .item_id = kTopazId, .type = 3, .amount = 2});
  return bag;
}

}  // namespace

TEST_CASE("Grade : seule l'interface des grades ouvre la fenêtre", "[net][grade_enchant]") {
  GradeEnchant grade;
  Inventory bag;
  CHECK(ApplyGradeEnchantFrame(UiOpenFrame(kBankUi), grade, bag) == FrameOutcome::kUnhandled);
  CHECK_FALSE(grade.open());
  CHECK(ApplyGradeEnchantFrame(UiOpenFrame(kGradeUi), grade, bag) == FrameOutcome::kApplied);
  CHECK(grade.open());
}

TEST_CASE("Grade : la proposition et la réussite", "[net][grade_enchant]") {
  GradeEnchant grade;
  Inventory bag = Bag();
  grade.TellOpening();
  REQUIRE(grade.Choose(kSwordSlot));
  REQUIRE(ApplyGradeEnchantFrame(OfferFrame(kSwordSlot, 10), grade, bag) == FrameOutcome::kApplied);
  REQUIRE(grade.offer().has_value());
  CHECK(grade.offer()->supported);
  CHECK(grade.offer()->chance_percent == 10);
  CHECK(grade.offer()->blessing.per_step == 3);
  REQUIRE(grade.offer()->stakes.size() == 2);
  CHECK(grade.offer()->stakes[0].breaks);
  CHECK(grade.offer()->stakes[1].amount == 5);
  CHECK(grade.offer()->stakes[1].zeny == 875000);

  // LA RÉUSSITE : grade D, raffinement ramené à zéro.
  REQUIRE(ApplyGradeEnchantFrame(AckFrame(kSwordSlot, 1, 0), grade, bag) == FrameOutcome::kApplied);
  const InventoryItem* sword = bag.Find(kSwordSlot);
  REQUIRE(sword != nullptr);
  CHECK(sword->grade == 1);
  CHECK(sword->refine_level == 0);
  CHECK(grade.result() == ro::sim::GradeEnchantResult::kSucceeded);
}

TEST_CASE("Grade : un niveau que le serveur ne gère pas", "[net][grade_enchant]") {
  GradeEnchant grade;
  Inventory bag = Bag();
  grade.TellOpening();
  REQUIRE(grade.Choose(kSwordSlot));
  REQUIRE(ApplyGradeEnchantFrame(OfferFrame(-1, 0), grade, bag) == FrameOutcome::kApplied);
  REQUIRE(grade.offer().has_value());
  CHECK_FALSE(grade.offer()->supported);
}

TEST_CASE("Grade : le choix, la demande et la fermeture partent sur le fil",
          "[net][grade_enchant]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  ro::net::Connection link = Connect(server);
  GradeEnchant grade;
  Inventory bag = Bag();
  grade.TellOpening();
  REQUIRE(grade.Choose(kSwordSlot));
  ro::net::AskGradeEnchantOffer(link, grade);
  const std::vector<std::uint8_t> select = server.Receive(4);
  REQUIRE(select.size() == 4);
  CHECK(ReadU16(select, 0) == kSelectOpcode);
  CHECK(ReadU16(select, 2) == kSwordSlot);

  REQUIRE(ApplyGradeEnchantFrame(OfferFrame(kSwordSlot, 10), grade, bag) == FrameOutcome::kApplied);
  CHECK_FALSE(ro::net::RequestGradeEnchant(link, grade, {.stake = 0}, bag, 100).has_value());
  REQUIRE(ro::net::RequestGradeEnchant(link, grade, {.stake = 0}, bag, 1000000).has_value());
  CHECK(grade.waiting());
  const std::vector<std::uint8_t> request = server.Receive(14);
  REQUIRE(request.size() == 14);
  CHECK(ReadU16(request, 0) == kRequestOpcode);
  CHECK(ReadU16(request, 2) == kSwordSlot);
  CHECK(ReadU32(request, 4) == 0);
  // Sans dose, pas de drapeau de poussière.
  CHECK(request[8] == 0);

  grade.Close();
  ro::net::SayGradeEnchantFarewell(link, grade);
  const std::vector<std::uint8_t> farewell = server.Receive(2);
  REQUIRE(farewell.size() == 2);
  CHECK(ReadU16(farewell, 0) == kCloseOpcode);
}
