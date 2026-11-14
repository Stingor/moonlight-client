// LE LIVRE DE LA RÉFORME ET CE QUE SA FENÊTRE MONTRE : la table lue sur un texte
// inventé, et la vue composée, refus et matériaux compris.

#include "game/item_reform_view.h"

#include <cstdint>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/inventory.h"
#include "ro/sim/item_reform.h"

#include "items/item_reform_book.h"

using moonclient::ItemReformBook;
using moonclient::ItemReformLooks;
using moonclient::ItemReformResultText;
using moonclient::ItemReformScene;
using moonclient::ItemReformViewComposer;
using ro::sim::Inventory;
using ro::sim::InventoryItem;
using ro::sim::ItemReform;

namespace {

constexpr std::uint32_t kRitualId = 100648;
constexpr std::uint32_t kAdulterId = 1499;
constexpr std::uint32_t kVivatusId = 1500;
constexpr std::uint32_t kOilId = 1000501;
constexpr std::uint16_t kSwordSlot = 12;
constexpr std::uint16_t kLowSwordSlot = 13;
constexpr std::uint16_t kOilSlot = 20;

constexpr const char* kBookText = R"(
version: 1
mode: Prerenewal
refine_cap: 10
triggers:
  - item: 100648
    recipes:
      - base: 1499
        result: 1500
        least_refine: 9
        most_refine: 10
        least_options: 0
        cards_allowed: true
        refine_change: -5
        materials: [[1000501, 50]]
)";

[[nodiscard]] ItemReformLooks TestLooks() {
  return ItemReformLooks{
      .name_of_held =
          [](const InventoryItem& item) {
            return "+" + std::to_string(item.refine_level) + " #" + std::to_string(item.item_id);
          },
      .name_of_id = [](std::uint32_t item_id) { return "#" + std::to_string(item_id); },
      .icon_of = [](std::uint32_t) { return ro::ui::SkinImage{}; }};
}

[[nodiscard]] Inventory Bag(std::int16_t oil) {
  Inventory bag;
  bag.Put(InventoryItem{.client_slot = kSwordSlot,
                        .item_id = kAdulterId,
                        .amount = 1,
                        .refine_level = 9,
                        .identified = true});
  bag.Put(InventoryItem{.client_slot = kLowSwordSlot,
                        .item_id = kAdulterId,
                        .amount = 1,
                        .refine_level = 4,
                        .identified = true});
  bag.Put(InventoryItem{.client_slot = kOilSlot, .item_id = kOilId, .amount = oil});
  return bag;
}

}  // namespace

TEST_CASE("Livre de la réforme : lu, il trouve son déclencheur", "[app][item_reform]") {
  const auto book = ItemReformBook::Parse(kBookText);
  REQUIRE(book.has_value());
  const ro::sim::ItemReformTrigger* ritual = book->Find(kRitualId);
  REQUIRE(ritual != nullptr);
  CHECK(ritual->refine_cap == 10);
  REQUIRE(ritual->recipes.size() == 1);
  CHECK(ritual->recipes[0].result_item_id == kVivatusId);
  CHECK(ritual->recipes[0].refine_change == -5);
  REQUIRE(ritual->recipes[0].materials.size() == 1);
  CHECK(ritual->recipes[0].materials[0].amount == 50);
  CHECK(book->Find(kAdulterId) == nullptr);
}

TEST_CASE("Livre de la réforme : une autre version se refuse", "[app][item_reform]") {
  CHECK_FALSE(ItemReformBook::Parse("version: 2\n").has_value());
}

TEST_CASE("Vue de la réforme : pièces, résultat et matériaux", "[app][item_reform]") {
  const auto book = ItemReformBook::Parse(kBookText);
  REQUIRE(book.has_value());
  const ItemReformLooks looks = TestLooks();
  const Inventory bag = Bag(10);
  ItemReform reform;
  reform.TellOpening(kRitualId);
  ItemReformViewComposer composer;

  const auto& empty = composer.Compose(ItemReformScene{
      .reform = reform, .trigger = *book->Find(kRitualId), .inventory = bag, .looks = looks});
  REQUIRE(empty.candidates.size() == 2);
  CHECK(empty.candidates[0].refusal.empty());
  CHECK_FALSE(empty.candidates[1].refusal.empty());
  CHECK(empty.chosen.item_id == 0);
  REQUIRE(empty.blockers.size() == 1);

  REQUIRE(reform.Choose(kSwordSlot));
  const auto& chosen = composer.Compose(ItemReformScene{
      .reform = reform, .trigger = *book->Find(kRitualId), .inventory = bag, .looks = looks});
  CHECK(chosen.result.item_id == kVivatusId);
  CHECK(chosen.result.name == "+4 #1500");
  REQUIRE(chosen.materials.size() == 1);
  CHECK(chosen.materials[0].owned == 10);
  CHECK(chosen.materials[0].needed == 50);
  REQUIRE(chosen.blockers.size() == 1);

  const Inventory rich = Bag(50);
  const auto& ready = composer.Compose(ItemReformScene{
      .reform = reform, .trigger = *book->Find(kRitualId), .inventory = rich, .looks = looks});
  CHECK(ready.blockers.empty());
}

TEST_CASE("Vue de la réforme : un refus du serveur se dit", "[app][item_reform]") {
  CHECK(ItemReformResultText(ro::sim::kItemReformSucceeded).empty());
  CHECK_FALSE(ItemReformResultText(1).empty());
}
