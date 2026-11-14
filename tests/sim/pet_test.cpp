// LE FAMILIER : ses paliers, ses gardes, sa présence, son avertissement.

#include "ro/sim/pet.h"

#include <cstdint>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/hunger_alarm.h"
#include "ro/sim/inventory.h"

using ro::sim::Pet;
using ro::sim::PetEvolutionRefusal;
using ro::sim::PetIntimacyWord;
using ro::sim::PetStateChange;

namespace {

constexpr std::uint32_t kPetActor = 110000123;
constexpr std::uint32_t kPoring = 1002;
constexpr std::uint32_t kDoramMount = 4051;
constexpr std::uint32_t kMapEntry = 1;

Pet HatchedPet(int intimacy) {
  Pet pet;
  CHECK(pet.ApplyStateChange(PetStateChange::kBirth, kPetActor, 0));
  pet.ApplyProperty(ro::sim::PetProperty{.name = "Popo",
                                         .renamed = false,
                                         .level = 1,
                                         .hunger = 50,
                                         .intimacy = intimacy,
                                         .accessory_item_id = 0,
                                         .pet_class = kPoring});
  return pet;
}

}  // namespace

TEST_CASE("PetSim : les paliers de faim et d'intimité sont ceux du client", "[petsim]") {
  CHECK(ro::sim::PetHungerRank(10) == 0);
  CHECK(ro::sim::PetHungerRank(11) == 1);
  CHECK(ro::sim::PetHungerRank(25) == 1);
  CHECK(ro::sim::PetHungerRank(26) == 2);
  CHECK(ro::sim::PetHungerRank(76) == 3);
  CHECK(ro::sim::PetHungerRank(91) == 4);
  CHECK(ro::sim::PetIntimacyRank(100) == 0);
  CHECK(ro::sim::PetIntimacyRank(101) == 1);
  CHECK(ro::sim::PetIntimacyRank(900) == 3);
  CHECK(ro::sim::PetIntimacyRank(901) == 4);
}

TEST_CASE("PetSim : le mot de l'intimité, monture Doram comprise", "[petsim]") {
  CHECK(ro::sim::PetIntimacyWordOf(400, kPoring) == PetIntimacyWord::kNeutral);
  CHECK(ro::sim::PetIntimacyWordOf(250, kPoring) == PetIntimacyWord::kShy);
  CHECK(ro::sim::PetIntimacyWordOf(1000, kPoring) == PetIntimacyWord::kLoyal);
  CHECK(ro::sim::PetIntimacyWordOf(1001, kPoring) == PetIntimacyWord::kUnknown);
  CHECK(ro::sim::PetIntimacyWordOf(15, kPoring) == PetIntimacyWord::kAwkward);
  CHECK(ro::sim::PetIntimacyWordOf(15, kDoramMount) == PetIntimacyWord::kHateWithPassion);
  CHECK(ro::sim::PetIntimacyWordOf(35, kDoramMount) == PetIntimacyWord::kHate);
  CHECK(ro::sim::PetIntimacyWordOf(60, kDoramMount) == PetIntimacyWord::kAwkward);
  CHECK(ro::sim::PetEggRankWord(3) == PetIntimacyWord::kNeutral);
  CHECK_FALSE(ro::sim::PetEggRankWord(0).has_value());
}

TEST_CASE("PetSim : l'évolution se refuse au seuil du SERVEUR, dans son ordre", "[petsim]") {
  // 905 : le client la propose, le serveur la refuse.
  CHECK(ro::sim::PetEvolutionRefusalOf(905, false, true) == PetEvolutionRefusal::kIntimacy);
  CHECK(ro::sim::PetEvolutionRefusalOf(910, true, false) == PetEvolutionRefusal::kAccessory);
  CHECK(ro::sim::PetEvolutionRefusalOf(910, false, false) == PetEvolutionRefusal::kMaterials);
  CHECK(ro::sim::PetEvolutionRefusalOf(950, false, true) == PetEvolutionRefusal::kNone);
}

TEST_CASE("PetSim : un nom vide ou de plus de vingt-trois octets est refusé", "[petsim]") {
  CHECK(ro::sim::CheckPetName("") == ro::sim::PetNameRefusal::kEmpty);
  CHECK(ro::sim::CheckPetName(std::string(23, 'a')) == ro::sim::PetNameRefusal::kNone);
  CHECK(ro::sim::CheckPetName(std::string(24, 'a')) == ro::sim::PetNameRefusal::kTooLong);
}

TEST_CASE("PetSim : la faim précédente suit chaque changement", "[petsim]") {
  Pet pet = HatchedPet(400);
  CHECK(pet.ApplyStateChange(PetStateChange::kHunger, kPetActor, 45));
  CHECK(pet.hunger() == 45);
  CHECK(pet.previous_hunger() == 50);
  // Le changement d'un autre familier ne touche pas le nôtre.
  CHECK_FALSE(pet.ApplyStateChange(PetStateChange::kHunger, kPetActor + 1, 10));
  CHECK(pet.hunger() == 45);
  // L'apparence ne regarde pas l'état.
  CHECK_FALSE(pet.ApplyStateChange(PetStateChange::kPerformance, kPetActor, 2));
}

TEST_CASE("PetSim : la basse intimité n'avertit qu'une fois, et se réarme en remontant",
          "[petsim]") {
  Pet pet = HatchedPet(400);
  CHECK(pet.ApplyStateChange(PetStateChange::kIntimacy, kPetActor, 99));
  CHECK(pet.TakeLowIntimacyWarning());
  CHECK_FALSE(pet.TakeLowIntimacyWarning());
  CHECK(pet.ApplyStateChange(PetStateChange::kIntimacy, kPetActor, 90));
  CHECK_FALSE(pet.TakeLowIntimacyWarning());
  CHECK(pet.ApplyStateChange(PetStateChange::kIntimacy, kPetActor, 150));
  CHECK(pet.ApplyStateChange(PetStateChange::kIntimacy, kPetActor, 80));
  CHECK(pet.TakeLowIntimacyWarning());
}

TEST_CASE("PetSim : le familier est parti après une absence CONTINUE, hors entrée sur carte",
          "[petsim]") {
  Pet pet = HatchedPet(400);
  pet.ObservePresence(true, kMapEntry, 0);
  pet.ObservePresence(true, kMapEntry, 100);
  CHECK(pet.seen());
  // Une entrée sur une carte LENTE : l'absence ne compte pas tant que le
  // familier n'a pas reparu, quelle que soit sa durée.
  pet.ObservePresence(false, kMapEntry + 1, 5000);
  pet.ObservePresence(false, kMapEntry + 1, 9000);
  pet.ObservePresence(false, kMapEntry + 1, 9500);
  CHECK(pet.known());
  CHECK_FALSE(pet.seen());
  pet.ObservePresence(true, kMapEntry + 1, 9600);
  CHECK(pet.seen());
  CHECK(pet.actor_id() == kPetActor);
  // Le vrai départ.
  pet.ObservePresence(false, kMapEntry + 1, 10000);
  pet.ObservePresence(false, kMapEntry + 1, 10500);
  CHECK(pet.known());
  pet.ObservePresence(false, kMapEntry + 1, 11000);
  CHECK_FALSE(pet.known());
  CHECK(pet.actor_id() == 0);
  // Il revient : il n'est plus tenu pour parti.
  pet.ObservePresence(true, kMapEntry + 1, 11100);
  CHECK(pet.seen());
}

TEST_CASE("PetSim : l'accessoire suit le changement d'état, et redemande la fiche", "[petsim]") {
  Pet pet = HatchedPet(950);
  CHECK_FALSE(pet.wears_accessory());
  constexpr std::int32_t kAccessoryView = 10013;
  CHECK(pet.ApplyStateChange(PetStateChange::kAccessory, kPetActor, kAccessoryView));
  CHECK(pet.wears_accessory());
  CHECK(pet.TakeSheetWanted());
  CHECK_FALSE(pet.TakeSheetWanted());
  // La même apparence redite — à l'apparition sur une carte — ne redemande rien.
  CHECK(pet.ApplyStateChange(PetStateChange::kAccessory, kPetActor, kAccessoryView));
  CHECK_FALSE(pet.TakeSheetWanted());
  CHECK(pet.ApplyStateChange(PetStateChange::kAccessory, kPetActor, 0));
  CHECK_FALSE(pet.wears_accessory());
  CHECK(pet.accessory_item_id() == 0);
  // Celui d'un autre familier ne touche pas le nôtre.
  CHECK_FALSE(pet.ApplyStateChange(PetStateChange::kAccessory, kPetActor + 1, kAccessoryView));
  CHECK_FALSE(pet.wears_accessory());
}

TEST_CASE("PetSim : l'œuf du familier sorti est celui que le serveur marque éclos", "[petsim]") {
  ro::sim::Inventory bag;
  bag.Put(ro::sim::InventoryItem{.client_slot = 2, .item_id = 9001, .amount = 1});
  CHECK(ro::sim::HatchedEggOf(bag) == 0);
  ro::sim::InventoryItem hatched{.client_slot = 3, .item_id = 9069, .amount = 1};
  hatched.damaged = true;
  bag.Put(hatched);
  CHECK(ro::sim::HatchedEggOf(bag) == 9069);
  bag.Put(ro::sim::InventoryItem{.client_slot = 4, .item_id = 610, .amount = 7});
  bag.Put(ro::sim::InventoryItem{.client_slot = 5, .item_id = 610, .amount = 5});
  CHECK(bag.AmountOf(610) == 12);
  // Une pile négative, qu'une trame fautive poserait, ne retranche rien.
  bag.Put(ro::sim::InventoryItem{.client_slot = 6, .item_id = 610, .amount = -3});
  CHECK(bag.AmountOf(610) == 12);
  CHECK(bag.AmountOf(611) == 0);
}

TEST_CASE("PetSim : un accessoire de pet ne se porte pas sur le personnage", "[petsim]") {
  constexpr std::uint32_t kBackpack = 10013;
  constexpr std::uint32_t kHeadTop = 0x100;
  constexpr std::uint8_t kWeaponType = 5;
  ro::sim::InventoryItem accessory{.client_slot = 2, .item_id = kBackpack, .amount = 1};
  accessory.type = ro::sim::kPetAccessoryItemType;
  CHECK(ro::sim::IsPetAccessoryItem(accessory));
  // UN EMPLACEMENT D'ÉQUIPEMENT en fait une pièce du personnage, quelle que
  // soit sa famille.
  ro::sim::InventoryItem worn = accessory;
  worn.wearable_at = kHeadTop;
  CHECK_FALSE(ro::sim::IsPetAccessoryItem(worn));
  // Une autre famille n'en est pas un.
  ro::sim::InventoryItem weapon = accessory;
  weapon.type = kWeaponType;
  CHECK_FALSE(ro::sim::IsPetAccessoryItem(weapon));
}

TEST_CASE("PetSim : l'alerte de faim sonne une fois par descente au palier", "[petsim]") {
  ro::sim::HungerAlarm alarm;
  CHECK_FALSE(alarm.Observe(true, 50));
  CHECK_FALSE(alarm.Observe(true, ro::sim::kCompanionStarvingHunger + 1));
  CHECK(alarm.Observe(true, ro::sim::kCompanionStarvingHunger));
  // La faim renvoyée point par point ne la refait pas sonner.
  CHECK_FALSE(alarm.Observe(true, 9));
  CHECK_FALSE(alarm.Observe(true, 3));
  // Nourri, il remonte : elle se réarme.
  CHECK_FALSE(alarm.Observe(true, 40));
  CHECK(alarm.Observe(true, 8));
}

TEST_CASE("PetSim : un compagnon inconnu réarme l'alerte de faim", "[petsim]") {
  ro::sim::HungerAlarm alarm;
  CHECK(alarm.Observe(true, 5));
  // Rangé dans l'œuf, puis un autre sort, affamé : on le dit.
  CHECK_FALSE(alarm.Observe(false, 5));
  CHECK(alarm.Observe(true, 5));
}
