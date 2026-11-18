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

TEST_CASE("Usage : l'accusé d'un VOISIN ne touche pas à notre sac", "[net][inventaire]") {
  // 🔴 UN USAGE RÉUSSI PART À TOUTE LA ZONE. Un voisin qui ouvre une boîte
  // d'une pile de 17 775 annonce « case 2, reste 17 774 » ; lu sans regarder
  // son identifiant, ce reste s'écrivait sur NOTRE case 2 — une coiffe y a
  // compté 17 774 exemplaires.
  constexpr std::uint32_t kMe = 2000001;
  constexpr std::uint32_t kNeighbour = 2000002;
  constexpr std::uint16_t kSlot = 2;
  constexpr std::uint32_t kUnicornHorn = 2257;
  constexpr std::uint32_t kOldBlueBox = 603;
  constexpr std::int16_t kNeighbourLeft = 17774;
  World world;
  world.set_player_id(kMe);
  world.inventory().Put(InventoryItem{.client_slot = kSlot, .item_id = kUnicornHorn, .amount = 1});

  ro::proto::ZcUseItemAck used;
  used.index = kSlot;
  used.itemId = kOldBlueBox;
  used.AID = kNeighbour;
  used.amount = kNeighbourLeft;
  used.result = 1;
  CHECK(ApplyInventoryFrame(FrameOf(Encode(used)), world, kNowMs) == FrameOutcome::kIgnored);
  REQUIRE(world.inventory().Find(kSlot) != nullptr);
  CHECK(world.inventory().Find(kSlot)->amount == 1);

  // Sa DERNIÈRE boîte effaçait la ligne.
  used.amount = 0;
  CHECK(ApplyInventoryFrame(FrameOf(Encode(used)), world, kNowMs) == FrameOutcome::kIgnored);
  CHECK(world.inventory().Find(kSlot) != nullptr);

  // Le nôtre s'applique.
  used.AID = kMe;
  CHECK(ApplyInventoryFrame(FrameOf(Encode(used)), world, kNowMs) == FrameOutcome::kApplied);
  CHECK(world.inventory().Find(kSlot) == nullptr);
}

TEST_CASE("Sac : une somme passagère au-delà de seize bits ne perd pas la pile",
          "[net][inventaire]") {
  // Une mise en charrette refusée revient en « plus N », puis « moins N », sur
  // la même case. À 20 000 + 20 000, seize bits signés débordaient, et le
  // retrait effaçait la ligne entière.
  constexpr std::uint16_t kSlot = 7;
  constexpr std::int16_t kStack = 20000;
  constexpr std::uint32_t kStarCrumb = 1000;
  World world;
  const auto added = Encode(Picked(kSlot, kStarCrumb, static_cast<std::uint16_t>(kStack)));
  REQUIRE(ApplyInventoryFrame(FrameOf(added), world, kNowMs) == FrameOutcome::kApplied);
  REQUIRE(ApplyInventoryFrame(FrameOf(added), world, kNowMs) == FrameOutcome::kApplied);
  CHECK(world.inventory().Find(kSlot)->amount == 2 * kStack);

  ro::proto::ZcDeleteItemFromBody removed;
  removed.index = kSlot;
  removed.count = kStack;
  REQUIRE(ApplyInventoryFrame(FrameOf(Encode(removed)), world, kNowMs) == FrameOutcome::kApplied);
  REQUIRE(world.inventory().Find(kSlot) != nullptr);
  CHECK(world.inventory().Find(kSlot)->amount == kStack);
}

TEST_CASE("Sac : une location échue quitte le sac, case entière", "[net][inventaire]") {
  // ⚠ CE PAQUET EST PARFOIS LE SEUL : un objet déjà échu à son arrivée n'est
  // suivi d'aucun retrait ordinaire, et restait affiché.
  constexpr std::uint16_t kSlot = 11;
  constexpr std::uint32_t kRental = 12215;
  World world;
  world.inventory().Put(InventoryItem{.client_slot = kSlot, .item_id = kRental, .amount = 3});

  ro::proto::ZcCashItemDelete expired;
  expired.index = kSlot;
  expired.itemId = kRental;
  CHECK(ApplyInventoryFrame(FrameOf(Encode(expired)), world, kNowMs) == FrameOutcome::kApplied);
  CHECK(world.inventory().Find(kSlot) == nullptr);

  // Quand l'échéance tombe en jeu, un retrait ordinaire suit et trouve la case
  // vide : sans effet.
  CHECK(ApplyInventoryFrame(FrameOf(Encode(expired)), world, kNowMs) == FrameOutcome::kIgnored);
}
