// LE SERTISSAGE, CÔTÉ MONDE : ce que le sac devient à l'accusé, et ce que la
// session retient.

#include "ro/sim/card_composition.h"

#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/inventory.h"

using ro::sim::CardComposition;
using ro::sim::Inventory;
using ro::sim::InventoryItem;

namespace {

constexpr std::uint16_t kCardSlot = 5;
constexpr std::uint16_t kEquipSlot = 9;
constexpr std::uint32_t kCardId = 4001;
constexpr std::uint32_t kEnchantId = 4700;

}  // namespace

TEST_CASE("Sertir écrit dans la PREMIÈRE case vide, même après un trou", "[sim][sertissage]") {
  Inventory bag;
  bag.Put(InventoryItem{.client_slot = kCardSlot, .item_id = kCardId, .amount = 2});
  InventoryItem equip{.client_slot = kEquipSlot, .item_id = 1101, .amount = 1};
  equip.cards = {kCardId, 0, 0, kEnchantId};
  bag.Put(equip);
  REQUIRE(bag.InsertCard(kEquipSlot, kCardSlot));
  CHECK(bag.Find(kEquipSlot)->cards[1] == kCardId);
  CHECK(bag.Find(kEquipSlot)->cards[3] == kEnchantId);
  CHECK(bag.Find(kCardSlot)->amount == 1);
}

TEST_CASE("Sertir sur quatre cases pleines décompte quand même la carte", "[sim][sertissage]") {
  Inventory bag;
  bag.Put(InventoryItem{.client_slot = kCardSlot, .item_id = kCardId, .amount = 2});
  InventoryItem equip{.client_slot = kEquipSlot, .item_id = 1101, .amount = 1};
  equip.cards = {kCardId, kCardId, kCardId, kCardId};
  bag.Put(equip);
  REQUIRE(bag.InsertCard(kEquipSlot, kCardSlot));
  CHECK(bag.Find(kCardSlot)->amount == 1);
}

TEST_CASE("Sertir sur une place vide ne change rien", "[sim][sertissage]") {
  Inventory bag;
  bag.Put(InventoryItem{.client_slot = kCardSlot, .item_id = kCardId, .amount = 2});
  CHECK_FALSE(bag.InsertCard(kEquipSlot, kCardSlot));
  CHECK(bag.Find(kCardSlot)->amount == 2);
}

TEST_CASE("Les cases prises ne se comptent que parmi celles de la pièce", "[sim][sertissage]") {
  InventoryItem equip;
  // Deux emplacements, une carte, et un enchantement dans la case HAUTE.
  equip.cards = {kCardId, 0, 0, kEnchantId};
  CHECK(ro::sim::ItemCardsInSlots(equip, 2) == 1);
  CHECK(ro::sim::ItemCardsInSlots(equip, 4) == 2);
  CHECK(ro::sim::ItemCardsInSlots(equip, 0) == 0);
  // Un objet forgé n'a aucune carte : ses nombres disent qui l'a forgé.
  constexpr std::uint32_t kForgeMark = 255;
  equip.cards = {kForgeMark, 0, 1, 2};
  CHECK(ro::sim::ItemCardsInSlots(equip, 4) == 0);
}

TEST_CASE("La session fige la carte à la réception de la liste", "[sim][sertissage]") {
  CardComposition composition;
  CHECK_FALSE(composition.open());
  composition.Ask(kCardSlot);
  composition.Offer({kEquipSlot});
  CHECK(composition.open());
  CHECK(composition.offer().card_slot == kCardSlot);
  composition.Ask(kEquipSlot);
  CHECK(composition.offer().card_slot == kCardSlot);
  // Deux listes identiques sont deux réponses.
  composition.Ask(kCardSlot);
  composition.Offer({kEquipSlot});
  CHECK(composition.offer_seq() == 2);
  composition.Close();
  CHECK_FALSE(composition.open());
}

TEST_CASE("Le sertissage rapide oublie sa réponse quand on repose la question",
          "[sim][sertissage]") {
  CardComposition composition;
  CHECK_FALSE(composition.TellCompatible(kEquipSlot, {kCardSlot}));
  composition.AskCompatible(kEquipSlot);
  REQUIRE(composition.TellCompatible(kEquipSlot, {kCardSlot}));
  composition.AskCompatible(kEquipSlot + 1);
  CHECK_FALSE(composition.compatible().answered);
  CHECK(composition.compatible().card_slots.empty());
  composition.ForgetCompatible();
  CHECK(composition.compatible().equip_slot == 0);
}
