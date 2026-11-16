// LE LIVRE DE L'ENCHANTEMENT ET CE QUE SA FENÊTRE MONTRE : la table lue sur un texte
// inventé, et la vue composée, cases, pages, coûts et refus compris.

#include "game/item_enchant_view.h"

#include <cstdint>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/inventory.h"
#include "ro/sim/item_enchant.h"

#include "items/item_enchant_book.h"

using moonclient::ItemEnchantBook;
using moonclient::ItemEnchantLooks;
using moonclient::ItemEnchantResultText;
using moonclient::ItemEnchantScene;
using moonclient::ItemEnchantViewComposer;
using ro::sim::Inventory;
using ro::sim::InventoryItem;
using ro::sim::ItemEnchant;

namespace {

constexpr std::uint64_t kGroupId = 26;
constexpr std::uint32_t kArmorId = 450206;
constexpr std::uint32_t kStrOrbId = 311092;
constexpr std::uint32_t kDexOrbId = 311093;
constexpr std::uint32_t kStoneId = 1000711;
constexpr std::uint16_t kArmorSlot = 12;
constexpr std::uint16_t kStoneSlot = 30;

constexpr const char* kBookText = R"(
version: 1
mode: Prerenewal
groups:
  - id: 26
    targets: [450206]
    least_refine: 0
    least_grade: 0
    random_options: true
    order: [3, 2]
    reset: {chance: 50000, zeny: 10, materials: []}
    slots:
      - slot: 3
        zeny: 100000
        materials: []
        chance: 60000
        draws:
          - {grade: 0, enchants: [[311092, 50000], [311093, 50000]]}
        perfect: [[311092, 0, [[1000711, 1]]]]
        upgrades: []
      - slot: 2
        zeny: 0
        materials: []
        chance: 100000
        draws: []
        perfect: []
        upgrades: []
)";

[[nodiscard]] ItemEnchantLooks TestLooks() {
  return ItemEnchantLooks{
      .name_of_held = [](const InventoryItem& item) { return "#" + std::to_string(item.item_id); },
      .name_of_id = [](std::uint32_t item_id) { return "#" + std::to_string(item_id); },
      .icon_of = [](std::uint32_t) { return ro::ui::SkinImage{}; },
      .card_slots_of = [](std::uint32_t) { return std::uint8_t{1}; }};
}

[[nodiscard]] Inventory Bag() {
  Inventory bag;
  bag.Put(InventoryItem{
      .client_slot = kArmorSlot, .item_id = kArmorId, .amount = 1, .identified = true});
  return bag;
}

}  // namespace

TEST_CASE("Livre de l'enchantement : lu, il trouve son groupe", "[app][item_enchant]") {
  const auto book = ItemEnchantBook::Parse(kBookText);
  REQUIRE(book.has_value());
  const ro::sim::ItemEnchantGroup* group = book->Find(kGroupId);
  REQUIRE(group != nullptr);
  CHECK(group->order == std::vector<std::uint8_t>{3, 2});
  REQUIRE(group->reset.has_value());
  CHECK(group->reset->chance == 50000);
  const ro::sim::ItemEnchantSlot* slot = group->FindSlot(3);
  REQUIRE(slot != nullptr);
  CHECK(slot->chance == 60000);
  REQUIRE(slot->DrawsFor(0) != nullptr);
  CHECK(slot->DrawsFor(0)->enchants.size() == 2);
  REQUIRE(slot->FindPerfect(kStrOrbId) != nullptr);
  CHECK(slot->FindPerfect(kStrOrbId)->cost.materials[0].item_id == kStoneId);
  CHECK(book->Find(kArmorId) == nullptr);
}

TEST_CASE("Livre de l'enchantement : une autre version se refuse", "[app][item_enchant]") {
  CHECK_FALSE(ItemEnchantBook::Parse("version: 2\n").has_value());
}

TEST_CASE("Vue de l'enchantement : cases, tirage et choix sûr", "[app][item_enchant]") {
  const auto book = ItemEnchantBook::Parse(kBookText);
  REQUIRE(book.has_value());
  const ItemEnchantLooks looks = TestLooks();
  const Inventory bag = Bag();
  ItemEnchant enchant;
  enchant.TellOpening(kGroupId);
  REQUIRE(enchant.Choose(kArmorSlot));
  ro::ui::ItemEnchantWindowState state;
  ro::ui::OpenItemEnchantOn(state, enchant.opening_seq());
  ItemEnchantViewComposer composer;

  const auto& random = composer.Compose(ItemEnchantScene{.enchant = enchant,
                                                         .group = *book->Find(kGroupId),
                                                         .inventory = bag,
                                                         .looks = looks,
                                                         .state = state,
                                                         .chosen_slot = enchant.chosen_slot(),
                                                         .zeny = 1000000});
  REQUIRE(random.slots.size() == ro::sim::kItemCardSlots);
  CHECK(random.slots[0].card_slot);
  CHECK(random.slots[3].next);
  CHECK(random.random_open);
  CHECK(random.perfect_open);
  CHECK_FALSE(random.upgrade_open);
  CHECK_FALSE(random.reset_open);
  REQUIRE(random.options.size() == 2);
  CHECK(random.options[0].odds == "50.00 %");
  CHECK(random.zeny == 100000);
  CHECK(random.blockers.empty());

  state.perfect = true;
  const auto& perfect = composer.Compose(ItemEnchantScene{.enchant = enchant,
                                                          .group = *book->Find(kGroupId),
                                                          .inventory = bag,
                                                          .looks = looks,
                                                          .state = state,
                                                          .chosen_slot = enchant.chosen_slot(),
                                                          .zeny = 1000000});
  REQUIRE(perfect.options.size() == 1);
  CHECK(perfect.options[0].selectable);
  // La pierre manque : la ligne se grise, et la demande attend son choix.
  CHECK(perfect.options[0].short_of);
  REQUIRE(perfect.blockers.size() == 1);

  Inventory stocked = Bag();
  stocked.Put(InventoryItem{.client_slot = kStoneSlot, .item_id = kStoneId, .amount = 1});
  state.enchant_item_id = kStrOrbId;
  const auto& ready = composer.Compose(ItemEnchantScene{.enchant = enchant,
                                                        .group = *book->Find(kGroupId),
                                                        .inventory = stocked,
                                                        .looks = looks,
                                                        .state = state,
                                                        .chosen_slot = enchant.chosen_slot(),
                                                        .zeny = 1000000});
  CHECK(ready.blockers.empty());
  REQUIRE(ready.materials.size() == 1);
  CHECK(ready.materials[0].owned == 1);
}

TEST_CASE("Vue de l'enchantement : une séance close se montre sans se toucher",
          "[app][item_enchant]") {
  const auto book = ItemEnchantBook::Parse(kBookText);
  REQUIRE(book.has_value());
  const ItemEnchantLooks looks = TestLooks();
  const Inventory bag = Bag();
  ItemEnchant enchant;
  enchant.TellOpening(kGroupId);
  REQUIRE(enchant.Choose(kArmorSlot));
  enchant.MarkRequestSent({.mode = ro::sim::ItemEnchantMode::kRandom}, 3, 1);
  enchant.TellResult(ro::sim::kItemEnchantSucceededMessage, kDexOrbId);
  const ro::ui::ItemEnchantWindowState state;
  ItemEnchantViewComposer composer;
  const auto& shown = composer.Compose(ItemEnchantScene{.enchant = enchant,
                                                        .group = *book->Find(kGroupId),
                                                        .inventory = bag,
                                                        .looks = looks,
                                                        .state = state,
                                                        .chosen_slot = kArmorSlot,
                                                        .notice = "posé",
                                                        .notice_good = true,
                                                        .concluded = true});
  CHECK(shown.waiting);
  CHECK(shown.blockers.empty());
  CHECK(shown.chosen.item_id == kArmorId);
  CHECK(shown.notice == "posé");
}

TEST_CASE("Vue de l'enchantement : chaque pièce dit où en sont ses cases", "[app][item_enchant]") {
  const auto book = ItemEnchantBook::Parse(kBookText);
  REQUIRE(book.has_value());
  const ItemEnchantLooks looks = TestLooks();
  ItemEnchant enchant;
  enchant.TellOpening(kGroupId);
  const ro::ui::ItemEnchantWindowState state;
  ItemEnchantViewComposer composer;
  const auto compose = [&](const Inventory& bag) -> const ro::ui::ItemEnchantView& {
    return composer.Compose(ItemEnchantScene{.enchant = enchant,
                                             .group = *book->Find(kGroupId),
                                             .inventory = bag,
                                             .looks = looks,
                                             .state = state});
  };

  const Inventory blank = Bag();
  REQUIRE(compose(blank).candidates.size() == 1);
  CHECK(compose(blank).candidates[0].progress == "0/2 enchantée(s)");
  CHECK_FALSE(compose(blank).candidates[0].full);

  InventoryItem half{
      .client_slot = kArmorSlot, .item_id = kArmorId, .amount = 1, .identified = true};
  half.cards[3] = kStrOrbId;
  Inventory one;
  one.Put(half);
  CHECK(compose(one).candidates[0].progress == "1/2 enchantée(s)");

  half.cards[2] = kDexOrbId;
  Inventory both;
  both.Put(half);
  CHECK(compose(both).candidates[0].progress == "pleine");
  CHECK(compose(both).candidates[0].full);
}

TEST_CASE("Vue de l'enchantement : l'effet en une ligne, tiré de la fiche", "[app][item_enchant]") {
  const std::vector<std::string> description{"STR +1#", "^0000FFRefine Level +7:^000000", "STR +1#",
                                             "^000088[Enchantable Equipment]^000000",
                                             "Gray Wolf Suits"};
  CHECK(moonclient::ItemEffectLine(description) == "STR +1 · Refine Level +7: STR +1");
  // Trop longue : coupée sur un caractère entier, et marquée.
  const std::string cut = moonclient::ItemEffectLine(description, 8);
  CHECK(cut.starts_with("STR +1"));
  CHECK(cut.ends_with("…"));
  CHECK(moonclient::ItemEffectLine({}).empty());
}

TEST_CASE("Vue de l'enchantement : l'aperçu montre la case avant et après", "[app][item_enchant]") {
  const auto book = ItemEnchantBook::Parse(kBookText);
  REQUIRE(book.has_value());
  ItemEnchantLooks looks = TestLooks();
  looks.effect_of = [](std::uint32_t item_id) { return "effet " + std::to_string(item_id); };
  const Inventory bag = Bag();
  ItemEnchant enchant;
  enchant.TellOpening(kGroupId);
  REQUIRE(enchant.Choose(kArmorSlot));
  ro::ui::ItemEnchantWindowState state;
  ro::ui::OpenItemEnchantOn(state, enchant.opening_seq());
  ItemEnchantViewComposer composer;
  const auto compose = [&]() -> const ro::ui::ItemEnchantView& {
    return composer.Compose(ItemEnchantScene{.enchant = enchant,
                                             .group = *book->Find(kGroupId),
                                             .inventory = bag,
                                             .looks = looks,
                                             .state = state,
                                             .chosen_slot = enchant.chosen_slot(),
                                             .zeny = 1000000});
  };

  // Tirage : la case suivante, sans savoir ce qu'elle recevra.
  REQUIRE(compose().preview.size() == 1);
  CHECK(compose().preview[0].slot == 3);
  CHECK(compose().preview[0].drawn);
  REQUIRE(compose().options.size() == 2);
  CHECK(compose().options[0].effect == "effet 311092");

  // Choix sûr : rien tant que rien n'est choisi, puis l'orbe et son effet.
  state.perfect = true;
  CHECK(compose().preview.empty());
  state.enchant_item_id = kStrOrbId;
  REQUIRE(compose().preview.size() == 1);
  CHECK(compose().preview[0].after.item_id == kStrOrbId);
  CHECK(compose().preview[0].effect == "effet 311092");
}

TEST_CASE("Vue de l'enchantement : le tirage du grade suivant se montre à part",
          "[app][item_enchant]") {
  constexpr const char* kGradedText = R"(
version: 1
mode: Prerenewal
groups:
  - id: 26
    targets: [450206]
    least_refine: 0
    least_grade: 0
    random_options: true
    order: [3]
    slots:
      - slot: 3
        zeny: 0
        materials: []
        chance: 100000
        draws:
          - {grade: 0, enchants: [[311092, 100000]]}
          - {grade: 1, enchants: [[311093, 100000]]}
        perfect: []
        upgrades: []
)";
  const auto book = ItemEnchantBook::Parse(kGradedText);
  REQUIRE(book.has_value());
  const ItemEnchantLooks looks = TestLooks();
  const Inventory bag = Bag();
  ItemEnchant enchant;
  enchant.TellOpening(kGroupId);
  REQUIRE(enchant.Choose(kArmorSlot));
  ro::ui::ItemEnchantWindowState state;
  ro::ui::OpenItemEnchantOn(state, enchant.opening_seq());
  ItemEnchantViewComposer composer;
  const auto compose = [&]() -> const ro::ui::ItemEnchantView& {
    return composer.Compose(ItemEnchantScene{.enchant = enchant,
                                             .group = *book->Find(kGroupId),
                                             .inventory = bag,
                                             .looks = looks,
                                             .state = state,
                                             .chosen_slot = enchant.chosen_slot()});
  };
  CHECK_FALSE(compose().other_grade_label.empty());
  REQUIRE(compose().options.size() == 1);
  CHECK(compose().options[0].item.item_id == kStrOrbId);
  state.other_grade = true;
  REQUIRE(compose().options.size() == 1);
  CHECK(compose().options[0].item.item_id == kDexOrbId);
  // La tentative, elle, tire toujours au grade de la pièce.
  CHECK(moonclient::ItemEnchantRequestOf(state).mode == ro::sim::ItemEnchantMode::kRandom);

  // Un groupe qui tire la même chose à chaque grade n'offre rien.
  const auto plain = ItemEnchantBook::Parse(kBookText);
  REQUIRE(plain.has_value());
  const auto& same = composer.Compose(ItemEnchantScene{.enchant = enchant,
                                                       .group = *plain->Find(kGroupId),
                                                       .inventory = bag,
                                                       .looks = looks,
                                                       .state = state,
                                                       .chosen_slot = enchant.chosen_slot()});
  CHECK(same.other_grade_label.empty());
}

TEST_CASE("Vue de l'enchantement : historique, et Recommencer quand la séance est close",
          "[app][item_enchant]") {
  const auto book = ItemEnchantBook::Parse(kBookText);
  REQUIRE(book.has_value());
  const ItemEnchantLooks looks = TestLooks();
  const Inventory bag = Bag();
  ItemEnchant enchant;
  const ro::ui::ItemEnchantWindowState state;
  const std::vector<moonclient::ItemEnchantAttempt> history{{.text = "posé", .good = true},
                                                            {.text = "raté", .good = false}};
  ItemEnchantViewComposer composer;
  const auto compose = [&](bool concluded, bool npc_known) -> const ro::ui::ItemEnchantView& {
    return composer.Compose(ItemEnchantScene{.enchant = enchant,
                                             .group = *book->Find(kGroupId),
                                             .inventory = bag,
                                             .looks = looks,
                                             .state = state,
                                             .chosen_slot = kArmorSlot,
                                             .concluded = concluded,
                                             .history = history,
                                             .npc_known = npc_known});
  };
  REQUIRE(compose(true, true).history.size() == 2);
  CHECK(compose(true, true).history[0].text == "posé");
  CHECK(compose(true, true).history[0].good);
  CHECK(compose(true, true).retry_open);
  // Une séance close n'a plus rien à prévoir.
  CHECK(compose(true, true).preview.empty());
  CHECK_FALSE(compose(true, false).retry_open);
  CHECK_FALSE(compose(false, true).retry_open);
}

TEST_CASE("Vue de l'enchantement : le résultat se dit", "[app][item_enchant]") {
  ItemEnchant::Result result{.request = {.mode = ro::sim::ItemEnchantMode::kRandom},
                             .message_id = ro::sim::kItemEnchantSucceededMessage,
                             .enchant_item_id = kDexOrbId};
  CHECK(ItemEnchantResultText(result, "DEX").find("DEX") != std::string::npos);
  result.message_id = ro::sim::kItemEnchantFailedMessage;
  CHECK_FALSE(ItemEnchantResultText(result, "DEX").empty());
}
