// LA PIÈCE PORTÉE QU'UNE DESCRIPTION COMPARE : la table exacte du client, et
// rien pour une pièce déjà portée.

#include "ro/sim/equip_compare.h"

#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/equip_location.h"
#include "ro/sim/inventory.h"

using ro::sim::ComparedEquipSlot;
using ro::sim::EquippedToCompare;
using ro::sim::Inventory;
using ro::sim::InventoryItem;

namespace {

[[nodiscard]] InventoryItem Piece(std::uint16_t client_slot, std::uint32_t item_id,
                                  std::uint32_t wearable_at, std::uint32_t worn_at) {
  InventoryItem item;
  item.client_slot = client_slot;
  item.item_id = item_id;
  item.amount = 1;
  item.wearable_at = wearable_at;
  item.worn_at = worn_at;
  item.identified = true;
  return item;
}

}  // namespace

TEST_CASE("Comparaison : la table des emplacements du client", "[sim][description]") {
  using namespace ro::sim;
  CHECK(ComparedEquipSlot(kEquipArmor) == kEquipArmor);
  CHECK(ComparedEquipSlot(kEquipBothHands) == kEquipWeapon);
  CHECK(ComparedEquipSlot(kEquipAccessoryRight | kEquipAccessoryLeft) == kEquipAccessoryRight);
  CHECK(ComparedEquipSlot(kEquipHeadTop | kEquipHeadMid | kEquipHeadLow) == kEquipHeadTop);
  CHECK(ComparedEquipSlot(kEquipHeadMid | kEquipHeadLow) == kEquipHeadMid);
  CHECK(ComparedEquipSlot(kEquipCostumeHeadTop | kEquipCostumeHeadLow) == kEquipCostumeHeadTop);
  // Un accessoire d'un seul côté, ou un masque inconnu : pas de comparaison.
  CHECK(ComparedEquipSlot(kEquipAccessoryLeft) == 0);
  CHECK(ComparedEquipSlot(kEquipAmmo) == 0);
  CHECK(ComparedEquipSlot(kEquipHeadTop | kEquipArmor) == 0);
}

TEST_CASE("Comparaison : la pièce portée au même emplacement", "[sim][description]") {
  using namespace ro::sim;
  Inventory bag;
  bag.BeginFill("inventaire");
  bag.Put(Piece(2, 2301, kEquipArmor, kEquipArmor));
  bag.Put(Piece(3, 1201, kEquipWeapon, 0));
  bag.Put(Piece(4, 2302, kEquipArmor, 0));
  bag.Put(Piece(5, 1116, kEquipBothHands, kEquipBothHands));
  bag.EndFill();

  const InventoryItem* spare_armor = bag.Find(4);
  REQUIRE(spare_armor != nullptr);
  const InventoryItem* worn = EquippedToCompare(bag, *spare_armor);
  REQUIRE(worn != nullptr);
  CHECK(worn->item_id == 2301);
  // Une dague contre l'arme à deux mains qu'on tient.
  const InventoryItem* dagger = bag.Find(3);
  REQUIRE(dagger != nullptr);
  REQUIRE(EquippedToCompare(bag, *dagger) != nullptr);
  CHECK(EquippedToCompare(bag, *dagger)->item_id == 1116);
  // Une pièce portée ne se compare pas.
  const InventoryItem* armor = bag.Find(2);
  REQUIRE(armor != nullptr);
  CHECK(EquippedToCompare(bag, *armor) == nullptr);
}

TEST_CASE("Comparaison : une munition contre la munition posée", "[sim][description]") {
  using namespace ro::sim;
  Inventory bag;
  bag.BeginFill("inventaire");
  bag.Put(Piece(2, 1750, kEquipAmmo, kEquipAmmo));
  bag.Put(Piece(3, 1751, kEquipAmmo, 0));
  bag.Put(Piece(4, 1201, kEquipWeapon, kEquipWeapon));
  bag.EndFill();

  const InventoryItem* spare_arrows = bag.Find(3);
  REQUIRE(spare_arrows != nullptr);
  const InventoryItem* worn = EquippedToCompare(bag, *spare_arrows);
  REQUIRE(worn != nullptr);
  CHECK(worn->item_id == 1750);
  // La munition posée ne se compare pas à elle-même.
  const InventoryItem* worn_arrows = bag.Find(2);
  REQUIRE(worn_arrows != nullptr);
  CHECK(EquippedToCompare(bag, *worn_arrows) == nullptr);
}

TEST_CASE("Comparaison : une munition sans munition posée", "[sim][description]") {
  using namespace ro::sim;
  Inventory bag;
  bag.BeginFill("inventaire");
  bag.Put(Piece(2, 1750, kEquipAmmo, 0));
  bag.Put(Piece(3, 1201, kEquipWeapon, kEquipWeapon));
  bag.EndFill();

  const InventoryItem* arrows = bag.Find(2);
  REQUIRE(arrows != nullptr);
  // L'arme portée n'est pas une munition : rien à comparer.
  CHECK(EquippedToCompare(bag, *arrows) == nullptr);
}
