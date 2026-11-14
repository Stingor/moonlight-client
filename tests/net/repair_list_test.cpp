// LA LISTE DE REPAIR WEAPON : celle que le serveur propose, la pièce choisie
// qui lui revient à l'identique, le refus à moins un, et l'issue.

#include "ro/net/repair_list.h"

#include <array>
#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/connection.h"
#include "ro/proto/map_packets.h"
#include "ro/proto/repair_packets.h"
#include "ro/sim/repair_list.h"

#include "support/loopback_link.h"
#include "support/loopback_server.h"
#include "support/net_frames.h"

using ro::net::ApplyRepairFrame;
using ro::net::FrameOutcome;
using ro::net::RequestRepair;
using ro::test::Connect;
using ro::test::LoopbackServer;
using ro::test_support::FrameOf;

namespace {

constexpr std::uint32_t kKnife = 1201;
constexpr std::uint32_t kCard = 4001;

ro::proto::RepairableEntry Knife() {
  return ro::proto::RepairableEntry{
      .index = 7, .item_id = kKnife, .cards = {kCard, 0, 0, 0}, .refine = 4, .grade = 1};
}

}  // namespace

TEST_CASE("Réparation : la liste porte vingt-quatre octets par pièce et ouvre la fenêtre",
          "[net][repair]") {
  const std::vector<std::uint8_t> bytes =
      ro::proto::EncodeRepairItemList(ro::proto::RepairItemList{{Knife()}});
  REQUIRE(bytes.size() ==
          ro::proto::RepairItemList::kHeaderSize + ro::proto::RepairItemList::kEntrySize);
  std::vector<std::uint8_t> ragged = bytes;
  ragged.pop_back();
  CHECK_FALSE(ro::proto::DecodeRepairItemList(ragged).has_value());

  ro::sim::RepairListOffer offer;
  CHECK(ApplyRepairFrame(FrameOf(bytes), offer) == FrameOutcome::kApplied);
  REQUIRE(offer.open());
  REQUIRE(offer.items().size() == 1);
  CHECK(offer.items()[0].index == 7);
  CHECK(offer.items()[0].item_id == kKnife);
  CHECK(offer.items()[0].cards[0] == kCard);
  CHECK(offer.items()[0].refine == 4);
  CHECK(offer.items()[0].grade == 1);
}

TEST_CASE("Réparation : la pièce choisie repart telle quelle, le refus à moins un",
          "[net][repair]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  ro::net::Connection link = Connect(server);

  ro::sim::RepairListOffer offer;
  REQUIRE(ApplyRepairFrame(
              FrameOf(ro::proto::EncodeRepairItemList(ro::proto::RepairItemList{{Knife()}})),
              offer) == FrameOutcome::kApplied);
  REQUIRE(RequestRepair(link, &offer.items()[0]).has_value());
  REQUIRE(RequestRepair(link, nullptr).has_value());

  const std::vector<std::uint8_t> chosen = server.Receive(ro::proto::RepairItemRequest::kWireSize);
  REQUIRE(chosen.size() == ro::proto::RepairItemRequest::kWireSize);
  CHECK(chosen[0] == 0x66);
  CHECK(chosen[1] == 0x0B);
  CHECK(chosen[2] == 7);
  CHECK(chosen[4] == (kKnife & 0xFFU));
  CHECK(chosen[24] == 4);
  CHECK(chosen[25] == 1);

  const std::vector<std::uint8_t> refused = server.Receive(ro::proto::RepairItemRequest::kWireSize);
  REQUIRE(refused.size() == ro::proto::RepairItemRequest::kWireSize);
  CHECK(refused[2] == 0xFF);
  CHECK(refused[3] == 0xFF);
}

TEST_CASE("Réparation : l'accusé dit l'issue, une seule fois", "[net][repair]") {
  ro::sim::RepairListOffer offer;
  CHECK(ApplyRepairFrame(
            FrameOf(ro::proto::Encode(ro::proto::ZcAckItemrepair{.index = 7, .result = 0})),
            offer) == FrameOutcome::kApplied);
  CHECK(offer.TakeOutcome() == std::optional<bool>(true));
  CHECK_FALSE(offer.TakeOutcome().has_value());
  CHECK(ApplyRepairFrame(
            FrameOf(ro::proto::Encode(ro::proto::ZcAckItemrepair{.index = 7, .result = 1})),
            offer) == FrameOutcome::kApplied);
  CHECK(offer.TakeOutcome() == std::optional<bool>(false));
}

TEST_CASE("Réparation : le matériau suit le niveau d'arme, Steel pour une armure",
          "[net][repair]") {
  using ro::sim::RepairMaterialFor;
  CHECK(RepairMaterialFor(0) == 999);
  CHECK(RepairMaterialFor(1) == 1002);
  CHECK(RepairMaterialFor(2) == 998);
  CHECK(RepairMaterialFor(3) == 999);
  CHECK(RepairMaterialFor(4) == 756);
  CHECK(RepairMaterialFor(5) == 0);
  CHECK(RepairMaterialFor(-1) == 0);
}
