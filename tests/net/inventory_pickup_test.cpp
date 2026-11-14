// LE RAMASSAGE : ce qu'une case du sac devient quand un objet y arrive.

#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/inventory.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/equip_location.h"
#include "ro/sim/inventory.h"
#include "ro/sim/world.h"

#include "support/net_frames.h"

using ro::net::ApplyInventoryFrame;
using ro::net::FrameOutcome;
using ro::sim::InventoryItem;
using ro::sim::World;
using ro::test_support::FrameOf;

namespace {

constexpr std::uint32_t kNowMs = 1000;

[[nodiscard]] ro::proto::ZcItemPickupAck Picked(std::uint16_t slot, std::uint32_t item_id,
                                                std::uint16_t count) {
  ro::proto::ZcItemPickupAck picked;
  picked.Index = slot;
  picked.count = count;
  picked.nameid = item_id;
  picked.IsIdentified = 1;
  picked.result = 0;
  return picked;
}

}  // namespace

TEST_CASE("Ramassage : une case d'un AUTRE objet est remplacée, pas additionnée",
          "[net][inventaire]") {
  // La même règle que le chariot et le coffre (`AddToHolder`) : additionner
  // deux objets différents ferait une pile de pommes à quinze potions.
  constexpr std::uint16_t kSlot = 2;
  World world;
  REQUIRE(ApplyInventoryFrame(FrameOf(Encode(Picked(kSlot, 501, 10))), world, kNowMs) ==
          FrameOutcome::kApplied);
  REQUIRE(ApplyInventoryFrame(FrameOf(Encode(Picked(kSlot, 512, 5))), world, kNowMs) ==
          FrameOutcome::kApplied);

  const InventoryItem* item = world.inventory().Find(kSlot);
  REQUIRE(item != nullptr);
  CHECK(item->item_id == 512);
  CHECK(item->amount == 5);
}

TEST_CASE("Ramassage : une pile portée qu'on complète reste portée", "[net][inventaire]") {
  // Des flèches équipées qu'on rachète ou ramasse arrivent par leur case, et
  // aucun accusé de pose ne suit.
  constexpr std::uint16_t kSlot = 4;
  constexpr std::uint32_t kArrow = 1750;
  World world;
  world.inventory().Put(InventoryItem{.client_slot = kSlot,
                                      .item_id = kArrow,
                                      .amount = 100,
                                      .wearable_at = ro::sim::kEquipAmmo,
                                      .worn_at = ro::sim::kEquipAmmo});
  REQUIRE(ApplyInventoryFrame(FrameOf(Encode(Picked(kSlot, kArrow, 50))), world, kNowMs) ==
          FrameOutcome::kApplied);

  const InventoryItem* arrows = world.inventory().Find(kSlot);
  REQUIRE(arrows != nullptr);
  CHECK(arrows->amount == 150);
  CHECK(arrows->worn_at == ro::sim::kEquipAmmo);
}

TEST_CASE("Jet au sol : noté avant le retrait, et jamais à zéro", "[net][inventaire]") {
  constexpr std::uint16_t kSlot = 2;
  World world;
  REQUIRE(ApplyInventoryFrame(FrameOf(Encode(Picked(kSlot, 501, 10))), world, kNowMs) ==
          FrameOutcome::kApplied);

  // Le jet est noté AVANT le retrait : la case vidée ne nommerait plus rien.
  ro::proto::ZcItemThrowAck thrown;
  thrown.index = kSlot;
  thrown.count = 4;
  REQUIRE(ApplyInventoryFrame(FrameOf(Encode(thrown)), world, kNowMs) == FrameOutcome::kApplied);
  REQUIRE(world.inventory().dropped().size() == 1);
  CHECK(world.inventory().dropped().front().dropped.item_id == 501);
  CHECK(world.inventory().dropped().front().dropped.amount == 4);
  CHECK(world.inventory().dropped().front().dropped_ms == kNowMs);
  REQUIRE(world.inventory().Find(kSlot) != nullptr);
  CHECK(world.inventory().Find(kSlot)->amount == 6);

  // Le serveur réemploie le paquet à zéro pour faire relire une case : ce
  // n'est pas un jet, et aucune ligne ne doit en sortir.
  thrown.count = 0;
  REQUIRE(ApplyInventoryFrame(FrameOf(Encode(thrown)), world, kNowMs) == FrameOutcome::kApplied);
  CHECK(world.inventory().dropped().size() == 1);
  CHECK(world.inventory().Find(kSlot)->amount == 6);
}
