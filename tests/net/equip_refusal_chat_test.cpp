// Le refus d'équiper ou de retirer une pièce se dit dans la discussion, dans la
// case « Équipement mis/retiré », en rouge. Un accord ne s'y écrit pas ici : sa
// ligne nomme la pièce, et `net` ne fait que le désigner.

#include <cstdint>
#include <optional>
#include <span>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "ro/core/i18n.h"
#include "ro/net/inventory.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/chat_line.h"
#include "ro/sim/world.h"

#include "support/net_frames.h"

using ro::core::Tr;
using ro::net::ApplyInventoryFrame;
using ro::net::EquipChange;
using ro::net::FrameOutcome;
using ro::net::GrantedEquipChangeOf;
using ro::sim::ChatType;
using ro::sim::World;
using ro::test_support::FrameOf;

namespace {

constexpr std::uint32_t kNowMs = 7'000;
constexpr std::uint16_t kSlot = 4;
constexpr std::uint32_t kHeadTop = 0x0100;
constexpr std::uint32_t kRed = 0xFF0000;

// Les codes du fil : équiper refuse par 1 (niveau) ou 2 (échec), retirer par 1.
constexpr std::uint8_t kGranted = 0;
constexpr std::uint8_t kEquipLevelRefusal = 1;
constexpr std::uint8_t kEquipRefusal = 2;
constexpr std::uint8_t kTakeOffRefusal = 1;
constexpr std::uint8_t kUnknownCode = 3;

const char* RequirementsText() {
  return Tr("Vous ne pouvez pas équiper cet item : vous n'en remplissez pas les conditions.");
}

FrameOutcome Equip(World& world, std::uint8_t result) {
  ro::proto::ZcReqWearEquipAck packet;
  packet.index = kSlot;
  packet.wearLocation = kHeadTop;
  packet.result = result;
  return ApplyInventoryFrame(FrameOf(ro::proto::Encode(packet)), world, kNowMs);
}

FrameOutcome TakeOff(World& world, std::uint8_t result) {
  ro::proto::PacketUnequipitemAck packet;
  packet.index = kSlot;
  packet.wearLocation = kHeadTop;
  packet.result = result;
  return ApplyInventoryFrame(FrameOf(ro::proto::Encode(packet)), world, kNowMs);
}

void CheckOnlyRefusal(const World& world, const char* text) {
  REQUIRE(world.chat_lines().size() == 1);
  const ro::sim::ChatLine& line = world.chat_lines().front();
  CHECK(line.text == text);
  CHECK(line.type == ChatType::kEquipmentOnOff);
  CHECK(line.kind == ro::sim::ChatKind::kServer);
  CHECK(line.speaker_id == 0);
  CHECK(line.color == kRed);
}

}  // namespace

TEST_CASE("Équipement : un refus faute de conditions se dit, en rouge, case 7", "[net][chat]") {
  World world;
  CHECK(Equip(world, kEquipLevelRefusal) == FrameOutcome::kIgnored);
  CheckOnlyRefusal(world, RequirementsText());
}

TEST_CASE("Équipement : un échec d'équiper dit qu'on ne peut pas l'équiper", "[net][chat]") {
  World world;
  CHECK(Equip(world, kEquipRefusal) == FrameOutcome::kIgnored);
  CheckOnlyRefusal(world, Tr("Vous ne pouvez pas équiper cet item."));
}

TEST_CASE("Équipement : un échec de retrait reprend la phrase des conditions", "[net][chat]") {
  World world;
  CHECK(TakeOff(world, kTakeOffRefusal) == FrameOutcome::kIgnored);
  CheckOnlyRefusal(world, RequirementsText());
}

TEST_CASE("Équipement : un accord ou un code inconnu ne dit rien", "[net][chat]") {
  World world;
  // Rien dans le sac : l'accord ne trouve pas la pièce, mais ne parle pas.
  (void)Equip(world, kGranted);
  (void)TakeOff(world, kGranted);
  CHECK(Equip(world, kUnknownCode) == FrameOutcome::kIgnored);
  CHECK(TakeOff(world, kUnknownCode) == FrameOutcome::kIgnored);
  CHECK(world.chat_lines().empty());
}

TEST_CASE("Équipement : seul un accusé accordé désigne une pièce posée ou retirée", "[net][chat]") {
  ro::proto::ZcReqWearEquipAck worn;
  worn.index = kSlot;
  worn.wearLocation = kHeadTop;
  worn.result = kGranted;
  const std::optional<EquipChange> put_on = GrantedEquipChangeOf(FrameOf(ro::proto::Encode(worn)));
  REQUIRE(put_on.has_value());
  CHECK(put_on->client_slot == kSlot);
  CHECK(put_on->equipped);

  ro::proto::PacketUnequipitemAck removed;
  removed.index = kSlot;
  removed.wearLocation = kHeadTop;
  removed.result = kGranted;
  const std::optional<EquipChange> taken_off =
      GrantedEquipChangeOf(FrameOf(ro::proto::Encode(removed)));
  REQUIRE(taken_off.has_value());
  CHECK(taken_off->client_slot == kSlot);
  CHECK_FALSE(taken_off->equipped);

  // Un refus, des deux côtés, ne désigne rien : il a sa propre ligne.
  worn.result = kEquipRefusal;
  removed.result = kTakeOffRefusal;
  CHECK_FALSE(GrantedEquipChangeOf(FrameOf(ro::proto::Encode(worn))).has_value());
  CHECK_FALSE(GrantedEquipChangeOf(FrameOf(ro::proto::Encode(removed))).has_value());
}
