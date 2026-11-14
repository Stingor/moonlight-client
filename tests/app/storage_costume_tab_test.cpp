// L'EMPLACEMENT D'UN APPARAT QU'ON VIENT DE DÉPOSER AU COFFRE.
//
// 🔴 CE QUE CE BANC TIENT : le paquet de dépôt (`ZC_ADD_ITEM_TO_STORE`) ne porte
// pas l'emplacement. Une coiffe d'apparat y arrive avec le type « armure » et un
// masque nul ; seuls les faits du coffre (`ZC 0x0F0F`, poussés juste derrière)
// disent que c'est un apparat. Sans eux elle tombait sous « Armures » jusqu'à la
// réouverture du coffre — signalé en jeu avec #20257.
//
// ⚠ `ItemBelongsToTab` vit dans l'exécutable : c'est le masque qu'il lit,
// `WearableAtOf`, qui se tient ici.

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/equip_location.h"
#include "ro/sim/inventory.h"
#include "ro/sim/storage_facts.h"

#include "items/inventory_view.h"

using moonclient::ItemCellsAsk;
using ro::sim::InventoryItem;
using ro::sim::StorageFacts;
using ro::sim::StorageFactsPart;
using ro::sim::StorageItemFacts;

namespace {

constexpr std::uint32_t kBlackRabbitBonnet = 20257;
constexpr std::uint8_t kArmorType = 4;

[[nodiscard]] StorageFacts BonnetFacts() {
  StorageFacts facts;
  facts.Tell({StorageItemFacts{.item_id = kBlackRabbitBonnet,
                               .equip_mask = ro::sim::kEquipCostumeHeadTop}},
             StorageFactsPart::kExtendsList);
  return facts;
}

}  // namespace

TEST_CASE("Coffre : un apparat déposé se sait apparat dès que le serveur le dit", "[app][coffre]") {
  // Tel que le dépôt le pose : type armure, aucun emplacement.
  const InventoryItem deposited{.item_id = kBlackRabbitBonnet, .type = kArmorType};
  const StorageFacts facts = BonnetFacts();

  const ItemCellsAsk ask{.storage_facts = &facts};
  CHECK(ask.WearableAtOf(deposited) == ro::sim::kEquipCostumeHeadTop);
  CHECK(ro::sim::IsCostumeLocation(ask.WearableAtOf(deposited)));
}

TEST_CASE("Coffre : sans les faits du serveur, le masque reste celui du fil", "[app][coffre]") {
  // ⚠ LA TRAME PEUT MANQUER — une image avant qu'elle arrive, ou un autre
  // contenant que le coffre : l'objet reste alors une armure, visible.
  const InventoryItem deposited{.item_id = kBlackRabbitBonnet, .type = kArmorType};
  const StorageFacts empty;

  CHECK(ItemCellsAsk{.storage_facts = &empty}.WearableAtOf(deposited) == 0);
  CHECK(ItemCellsAsk{}.WearableAtOf(deposited) == 0);
}

TEST_CASE("Coffre : l'emplacement du fil l'emporte sur les faits", "[app][coffre]") {
  // La liste d'ouverture porte l'emplacement : les faits ne servent qu'à combler
  // un masque nul, jamais à en contredire un.
  InventoryItem listed{.item_id = kBlackRabbitBonnet, .type = kArmorType};
  listed.wearable_at = ro::sim::kEquipHeadTop;
  const StorageFacts facts = BonnetFacts();

  CHECK(ItemCellsAsk{.storage_facts = &facts}.WearableAtOf(listed) == ro::sim::kEquipHeadTop);
}
