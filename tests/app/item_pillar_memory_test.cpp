#include "world/item_pillar_memory.h"

#include <cstdint>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/actor.h"
#include "ro/sim/world.h"

using moonclient::ItemPillarMemory;
using moonclient::kRememberedPillarsMax;
using ro::sim::Actor;
using ro::sim::ActorKind;
using ro::sim::CellPosition;
using ro::sim::GroundItem;
using ro::sim::World;

namespace {

constexpr std::uint32_t kFloorId = 700'001;
constexpr std::uint32_t kItemId = 607;
constexpr std::uint32_t kOtherItemId = 501;
constexpr std::uint16_t kCellX = 150;
constexpr std::uint16_t kCellY = 180;
constexpr std::uint16_t kNeighbourCellX = 151;
constexpr std::uint16_t kArrivalX = 156;
constexpr std::uint16_t kArrivalY = 191;
constexpr std::uint16_t kPurpleMode = 3;
constexpr std::uint32_t kNowMs = 10'000;

const std::string kLootMap = "prontera";
const std::string kOtherMap = "geffen";

// Ce que `ZC_ITEM_FALL_ENTRY5 0x0ADD` fait naître : un butin qui tombe, et dit
// son pilier.
Actor Fallen(std::uint32_t floor_id, std::uint32_t item_id, std::uint16_t mode) {
  Actor item;
  item.id = floor_id;
  item.kind = ActorKind::kItem;
  item.position = CellPosition{kCellX, kCellY, 0};
  item.ground_item = GroundItem{.since_ms = kNowMs,
                                .falling = true,
                                .show_drop_effect = mode != 0,
                                .drop_effect_mode = mode,
                                .item_id = item_id};
  return item;
}

// Ce que `ZC_ITEM_ENTRY 0x009D` fait naître : un objet déjà posé, sans un mot
// de son pilier.
Actor AlreadyThere(std::uint32_t floor_id, std::uint32_t item_id, std::uint16_t x,
                   std::uint16_t y) {
  Actor item;
  item.id = floor_id;
  item.kind = ActorKind::kItem;
  item.position = CellPosition{x, y, 0};
  item.ground_item = GroundItem{.since_ms = kNowMs, .falling = false, .item_id = item_id};
  return item;
}

// Le mode que la mémoire rend pour l'objet que le monde tient sous cet
// identifiant.
std::uint16_t ModeInWorld(ItemPillarMemory& memory, const World& world, std::uint32_t floor_id) {
  const Actor* item = world.Find(floor_id);
  REQUIRE(item != nullptr);
  return memory.ModeOf(world.map_name(), *item);
}

}  // namespace

TEST_CASE("Pilier : un butin retrouvé après un changement de carte garde sa colonne",
          "[app][item_drop]") {
  World world;
  world.EnterMap(kLootMap, kArrivalX, kArrivalY);
  ItemPillarMemory memory;
  world.Spawn(Fallen(kFloorId, kItemId, kPurpleMode));
  CHECK(ModeInWorld(memory, world, kFloorId) == kPurpleMode);

  // L'aller-retour vide le monde, et le serveur réannonce le butin resté à
  // terre par le paquet qui ne dit pas son pilier.
  world.EnterMap(kOtherMap, kArrivalX, kArrivalY);
  CHECK(world.Find(kFloorId) == nullptr);
  world.EnterMap(kLootMap, kArrivalX, kArrivalY);
  world.Spawn(AlreadyThere(kFloorId, kItemId, kCellX, kCellY));
  CHECK(ModeInWorld(memory, world, kFloorId) == kPurpleMode);
}

TEST_CASE("Pilier : un téléport sur la même carte le garde aussi", "[app][item_drop]") {
  World world;
  world.EnterMap(kLootMap, kArrivalX, kArrivalY);
  ItemPillarMemory memory;
  world.Spawn(Fallen(kFloorId, kItemId, kPurpleMode));
  CHECK(ModeInWorld(memory, world, kFloorId) == kPurpleMode);

  world.EnterMap(kLootMap, kCellX, kCellY);
  world.Spawn(AlreadyThere(kFloorId, kItemId, kCellX, kCellY));
  CHECK(ModeInWorld(memory, world, kFloorId) == kPurpleMode);
}

TEST_CASE("Pilier : un identifiant réattribué ne passe pas pour le même butin",
          "[app][item_drop]") {
  World world;
  world.EnterMap(kLootMap, kArrivalX, kArrivalY);
  ItemPillarMemory memory;
  world.Spawn(Fallen(kFloorId, kItemId, kPurpleMode));
  CHECK(ModeInWorld(memory, world, kFloorId) == kPurpleMode);

  SECTION("un autre objet") {
    world.Spawn(AlreadyThere(kFloorId, kOtherItemId, kCellX, kCellY));
    CHECK(ModeInWorld(memory, world, kFloorId) == 0);
  }
  SECTION("une autre case") {
    world.Spawn(AlreadyThere(kFloorId, kItemId, kNeighbourCellX, kCellY));
    CHECK(ModeInWorld(memory, world, kFloorId) == 0);
  }
  SECTION("une autre carte") {
    world.EnterMap(kOtherMap, kArrivalX, kArrivalY);
    world.Spawn(AlreadyThere(kFloorId, kItemId, kCellX, kCellY));
    CHECK(ModeInWorld(memory, world, kFloorId) == 0);
  }
  SECTION("une chute sans pilier efface le souvenir") {
    world.Spawn(Fallen(kFloorId, kItemId, 0));
    CHECK(ModeInWorld(memory, world, kFloorId) == 0);
    world.Spawn(AlreadyThere(kFloorId, kItemId, kCellX, kCellY));
    CHECK(ModeInWorld(memory, world, kFloorId) == 0);
  }
}

TEST_CASE("Pilier : un objet jamais vu tomber reste sans colonne", "[app][item_drop]") {
  World world;
  world.EnterMap(kLootMap, kArrivalX, kArrivalY);
  ItemPillarMemory memory;
  world.Spawn(AlreadyThere(kFloorId, kItemId, kCellX, kCellY));
  CHECK(ModeInWorld(memory, world, kFloorId) == 0);
  CHECK(memory.remembered() == 0);
}

TEST_CASE("Pilier : la mémoire est bornée, et le plus ancien part le premier", "[app][item_drop]") {
  ItemPillarMemory memory;
  for (std::uint32_t rank = 0; rank <= kRememberedPillarsMax; ++rank) {
    CHECK(memory.ModeOf(kLootMap, Fallen(kFloorId + rank, kItemId, kPurpleMode)) == kPurpleMode);
  }
  CHECK(memory.remembered() == kRememberedPillarsMax);
  CHECK(memory.ModeOf(kLootMap, AlreadyThere(kFloorId, kItemId, kCellX, kCellY)) == 0);
  CHECK(memory.ModeOf(kLootMap, AlreadyThere(kFloorId + 1, kItemId, kCellX, kCellY)) ==
        kPurpleMode);
}
