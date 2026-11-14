// LA RÉFORME, JUGÉE : ce que le serveur dirait d'une pièce et de ses matériaux, et la
// fermeture qu'il attend.

#include "ro/sim/item_reform.h"

#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/inventory.h"

using ro::sim::CheckItemReformRequest;
using ro::sim::CountedMaterial;
using ro::sim::Inventory;
using ro::sim::InventoryItem;
using ro::sim::ItemReform;
using ro::sim::ItemReformRecipe;
using ro::sim::ItemReformRefusal;
using ro::sim::ItemReformTrigger;
using ro::sim::JudgeItemReformPiece;
using ro::sim::ReformedRefine;

namespace {

constexpr std::uint32_t kRitualId = 100648;
constexpr std::uint32_t kAdulterId = 1499;
constexpr std::uint32_t kVivatusId = 1500;
constexpr std::uint32_t kOilId = 1000501;
constexpr std::uint16_t kSwordSlot = 12;
constexpr std::uint16_t kOilSlot = 20;
constexpr std::uint16_t kSecondOilSlot = 21;
constexpr std::uint8_t kRefineCap = 10;
constexpr std::uint8_t kNeededRefine = 9;
constexpr std::uint16_t kOilNeeded = 50;
constexpr std::int8_t kRefineDrop = -5;

[[nodiscard]] ItemReformTrigger Ritual() {
  return ItemReformTrigger{
      .trigger_item_id = kRitualId,
      .refine_cap = kRefineCap,
      .recipes = {ItemReformRecipe{.base_item_id = kAdulterId,
                                   .result_item_id = kVivatusId,
                                   .least_refine = kNeededRefine,
                                   .most_refine = kRefineCap,
                                   .refine_change = kRefineDrop,
                                   .materials = {{.item_id = kOilId, .amount = kOilNeeded}}}}};
}

[[nodiscard]] InventoryItem Sword(std::uint8_t refine) {
  return InventoryItem{.client_slot = kSwordSlot,
                       .item_id = kAdulterId,
                       .amount = 1,
                       .refine_level = refine,
                       .identified = true};
}

[[nodiscard]] InventoryItem Oil(std::uint16_t slot, std::int16_t amount) {
  return InventoryItem{.client_slot = slot, .item_id = kOilId, .amount = amount};
}

}  // namespace

TEST_CASE("Réforme : le raffinement admis", "[sim][item_reform]") {
  const ItemReformTrigger ritual = Ritual();
  const ItemReformRecipe& recipe = ritual.recipes.front();
  CHECK(JudgeItemReformPiece(recipe, Sword(kNeededRefine)) == ItemReformRefusal::kNone);
  CHECK(JudgeItemReformPiece(recipe, Sword(kNeededRefine - 1)) == ItemReformRefusal::kRefineTooLow);
  InventoryItem worn = Sword(kNeededRefine);
  worn.worn_at = 2;
  CHECK(JudgeItemReformPiece(recipe, worn) == ItemReformRefusal::kWorn);
}

TEST_CASE("Réforme : le raffinement du résultat est borné", "[sim][item_reform]") {
  const ItemReformTrigger ritual = Ritual();
  CHECK(ReformedRefine(ritual, ritual.recipes.front(), kNeededRefine) == 4);
  CHECK(ReformedRefine(ritual, ritual.recipes.front(), 2) == 0);
}

TEST_CASE("Réforme : seule la première pile d'un matériau compte", "[sim][item_reform]") {
  Inventory bag;
  bag.Put(Oil(kSecondOilSlot, kOilNeeded));
  bag.Put(Oil(kOilSlot, 1));
  CHECK(CountedMaterial(bag, kOilId) == 1);
}

TEST_CASE("Réforme : la demande se juge entière", "[sim][item_reform]") {
  const ItemReformTrigger ritual = Ritual();
  Inventory bag;
  bag.Put(Sword(kNeededRefine));
  ItemReform reform;
  CHECK(CheckItemReformRequest(reform, ritual, bag).refusal == ItemReformRefusal::kNotOpen);
  reform.TellOpening(kRitualId);
  CHECK(CheckItemReformRequest(reform, ritual, bag).refusal == ItemReformRefusal::kNothingChosen);
  REQUIRE(reform.Choose(kSwordSlot));
  const auto missing = CheckItemReformRequest(reform, ritual, bag);
  CHECK(missing.refusal == ItemReformRefusal::kMissingMaterial);
  CHECK(missing.material_id == kOilId);
  bag.Put(Oil(kOilSlot, kOilNeeded));
  CHECK(CheckItemReformRequest(reform, ritual, bag).passed());
  reform.MarkRequestSent();
  CHECK_FALSE(reform.Choose(kSwordSlot));
}

TEST_CASE("Réforme : la réussite ferme sans fermeture due", "[sim][item_reform]") {
  ItemReform reform;
  reform.TellOpening(kRitualId);
  REQUIRE(reform.Choose(kSwordSlot));
  reform.MarkRequestSent();
  reform.TellResult(ro::sim::kItemReformSucceeded, kSwordSlot);
  CHECK_FALSE(reform.open());
  CHECK_FALSE(reform.TakeFarewell());
  CHECK(reform.TakeReformed() == kSwordSlot);
  CHECK_FALSE(reform.TakeReformed().has_value());
}

TEST_CASE("Réforme : le joueur qui ferme le dit au serveur, une fois", "[sim][item_reform]") {
  ItemReform reform;
  reform.TellOpening(kRitualId);
  reform.Close();
  CHECK(reform.TakeFarewell());
  CHECK_FALSE(reform.TakeFarewell());
  reform.TellOpening(kRitualId);
  reform.CloseSilently();
  CHECK_FALSE(reform.TakeFarewell());
}
