// L'ENCHANTEMENT, JUGÉ COMME LE SERVEUR LE JUGE : les pièces admises, la case que le
// tirage remplit, les coûts, et la séance que chaque réponse clôt.

#include "ro/sim/item_enchant.h"

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/inventory.h"

using ro::sim::CheckItemEnchantRequest;
using ro::sim::EnchantedCards;
using ro::sim::EnchantedSlots;
using ro::sim::Inventory;
using ro::sim::InventoryItem;
using ro::sim::ItemEnchant;
using ro::sim::ItemEnchantGroup;
using ro::sim::ItemEnchantMode;
using ro::sim::ItemEnchantRefusal;
using ro::sim::ItemEnchantRequest;
using ro::sim::JudgeItemEnchantPiece;
using ro::sim::NextEnchantSlot;

namespace {

constexpr std::uint64_t kGroupId = 26;
constexpr std::uint32_t kArmorId = 450206;
constexpr std::uint32_t kStoneId = 1000711;
constexpr std::uint32_t kStrOrbId = 311092;
constexpr std::uint32_t kDexOrbId = 311093;
constexpr std::uint32_t kGreatStrOrbId = 311192;
constexpr std::uint16_t kArmorSlot = 12;
constexpr std::uint16_t kStoneSlot = 30;
constexpr std::uint8_t kCardSlots = 1;
constexpr std::int64_t kRandomZeny = 100000;
constexpr std::int64_t kRich = 1000000;
constexpr std::uint32_t kHalf = 50000;

// Trois cases au-delà de la carte, remplies dans l'ordre 3, 2, 1.
[[nodiscard]] ItemEnchantGroup Snowflower() {
  ItemEnchantGroup group{.group_id = kGroupId,
                         .target_item_ids = {kArmorId},
                         .order = {3, 2, 1},
                         .reset = ro::sim::ItemEnchantReset{.chance = kHalf}};
  ro::sim::ItemEnchantSlot slot3{
      .slot = 3,
      .cost = {.zeny = kRandomZeny},
      .draws = {{.grade = 0,
                 .enchants = {{.item_id = kStrOrbId, .odds = kHalf},
                              {.item_id = kDexOrbId, .odds = kHalf}}}},
      .perfect = {{.item_id = kStrOrbId, .cost = {.materials = {{.item_id = kStoneId}}}}},
      .upgrades = {{.from_item_id = kStrOrbId, .to_item_id = kGreatStrOrbId}}};
  group.slots.push_back(slot3);
  group.slots.push_back(ro::sim::ItemEnchantSlot{.slot = 2});
  return group;
}

[[nodiscard]] InventoryItem Armor() {
  return InventoryItem{
      .client_slot = kArmorSlot, .item_id = kArmorId, .amount = 1, .identified = true};
}

[[nodiscard]] ItemEnchant Opened(std::uint16_t chosen) {
  ItemEnchant enchant;
  enchant.TellOpening(kGroupId);
  REQUIRE(enchant.Choose(chosen));
  return enchant;
}

}  // namespace

TEST_CASE("Enchantement : la pièce admise est ôtée, intacte, assez raffinée",
          "[sim][item_enchant]") {
  ItemEnchantGroup group = Snowflower();
  group.least_refine = 7;
  InventoryItem armor = Armor();
  CHECK(JudgeItemEnchantPiece(group, armor) == ItemEnchantRefusal::kRefineTooLow);
  armor.refine_level = 7;
  CHECK(JudgeItemEnchantPiece(group, armor) == ItemEnchantRefusal::kNone);
  armor.worn_at = 0x10;
  CHECK(JudgeItemEnchantPiece(group, armor) == ItemEnchantRefusal::kWorn);
  armor.worn_at = 0;
  armor.damaged = true;
  CHECK(JudgeItemEnchantPiece(group, armor) == ItemEnchantRefusal::kDamaged);
  armor.damaged = false;
  armor.item_id = kStoneId;
  CHECK(JudgeItemEnchantPiece(group, armor) == ItemEnchantRefusal::kNotTargeted);
}

TEST_CASE("Enchantement : la première case libre de l'ordre", "[sim][item_enchant]") {
  const ItemEnchantGroup group = Snowflower();
  InventoryItem armor = Armor();
  CHECK(NextEnchantSlot(group, armor, kCardSlots) == 3);
  armor.cards[3] = kStrOrbId;
  CHECK(NextEnchantSlot(group, armor, kCardSlots) == 2);
  armor.cards[2] = kDexOrbId;
  // ⚠ LA CASE 1 EST LA PREMIÈRE LIBRE, MAIS LE SERVEUR LA REFUSE quand la pièce a
  // deux cases de carte : rien.
  CHECK(NextEnchantSlot(group, armor, 2) == std::nullopt);
  CHECK(NextEnchantSlot(group, armor, kCardSlots) == 1);
  CHECK(EnchantedSlots(armor, kCardSlots) == std::vector<std::uint8_t>{2, 3});
}

TEST_CASE("Enchantement : le tirage se juge contre la bourse et le grade", "[sim][item_enchant]") {
  const ItemEnchantGroup group = Snowflower();
  Inventory bag;
  bag.Put(Armor());
  const ItemEnchant enchant = Opened(kArmorSlot);
  const ItemEnchantRequest random{.mode = ItemEnchantMode::kRandom};
  CHECK(CheckItemEnchantRequest(enchant, group, random, bag, kCardSlots, 0).refusal ==
        ItemEnchantRefusal::kNotEnoughZeny);
  const auto passed = CheckItemEnchantRequest(enchant, group, random, bag, kCardSlots, kRich);
  CHECK(passed.passed());
  CHECK(passed.target_slot == 3);

  Inventory graded;
  InventoryItem armor = Armor();
  armor.grade = 1;
  graded.Put(armor);
  CHECK(CheckItemEnchantRequest(enchant, group, random, graded, kCardSlots, kRich).refusal ==
        ItemEnchantRefusal::kNoDrawForGrade);
}

TEST_CASE("Enchantement : le choix sûr demande son enchantement et ses matériaux",
          "[sim][item_enchant]") {
  const ItemEnchantGroup group = Snowflower();
  Inventory bag;
  bag.Put(Armor());
  const ItemEnchant enchant = Opened(kArmorSlot);
  CHECK(CheckItemEnchantRequest(enchant, group, {.mode = ItemEnchantMode::kPerfect}, bag,
                                kCardSlots, kRich)
            .refusal == ItemEnchantRefusal::kNoSuchEnchant);
  const ItemEnchantRequest perfect{.mode = ItemEnchantMode::kPerfect, .enchant_item_id = kStrOrbId};
  const auto missing = CheckItemEnchantRequest(enchant, group, perfect, bag, kCardSlots, kRich);
  CHECK(missing.refusal == ItemEnchantRefusal::kMissingMaterial);
  CHECK(missing.item_id == kStoneId);
  bag.Put(InventoryItem{.client_slot = kStoneSlot, .item_id = kStoneId, .amount = 1});
  CHECK(CheckItemEnchantRequest(enchant, group, perfect, bag, kCardSlots, kRich).passed());
}

TEST_CASE("Enchantement : l'amélioration vise une case qui porte ce qui monte",
          "[sim][item_enchant]") {
  const ItemEnchantGroup group = Snowflower();
  Inventory bag;
  InventoryItem armor = Armor();
  armor.cards[3] = kDexOrbId;
  bag.Put(armor);
  const ItemEnchant enchant = Opened(kArmorSlot);
  const ItemEnchantRequest upgrade{.mode = ItemEnchantMode::kUpgrade, .slot = 3};
  CHECK(CheckItemEnchantRequest(enchant, group, upgrade, bag, kCardSlots, kRich).refusal ==
        ItemEnchantRefusal::kNothingToUpgrade);
  Inventory upgraded;
  armor.cards[3] = kStrOrbId;
  upgraded.Put(armor);
  CHECK(CheckItemEnchantRequest(enchant, group, upgrade, upgraded, kCardSlots, kRich).passed());
}

TEST_CASE("Enchantement : la remise à zéro demande un enchantement", "[sim][item_enchant]") {
  const ItemEnchantGroup group = Snowflower();
  Inventory bag;
  bag.Put(Armor());
  const ItemEnchant enchant = Opened(kArmorSlot);
  const ItemEnchantRequest reset{.mode = ItemEnchantMode::kReset};
  CHECK(CheckItemEnchantRequest(enchant, group, reset, bag, kCardSlots, kRich).refusal ==
        ItemEnchantRefusal::kNothingToReset);
}

TEST_CASE("Enchantement : la réponse clôt la séance, sans fermeture due", "[sim][item_enchant]") {
  ItemEnchant enchant = Opened(kArmorSlot);
  enchant.MarkRequestSent({.mode = ItemEnchantMode::kRandom}, 3, kCardSlots);
  CHECK(enchant.waiting());
  enchant.TellResult(ro::sim::kItemEnchantSucceededMessage, kDexOrbId);
  CHECK_FALSE(enchant.open());
  CHECK_FALSE(enchant.TakeFarewell());
  CHECK(enchant.result_seq() == 1);
  CHECK(enchant.result().client_slot == kArmorSlot);
  CHECK(enchant.result().target_slot == 3);
}

TEST_CASE("Enchantement : la réussite pose ou ôte dans les cartes", "[sim][item_enchant]") {
  std::array<std::uint32_t, ro::sim::kItemCardSlots> cards = {4001, 0, kStrOrbId, 0};
  ItemEnchant::Result result{.request = {.mode = ItemEnchantMode::kRandom},
                             .target_slot = 3,
                             .card_slots = kCardSlots,
                             .message_id = ro::sim::kItemEnchantSucceededMessage,
                             .enchant_item_id = kDexOrbId};
  CHECK(EnchantedCards(cards, result)[3] == kDexOrbId);
  result.request.mode = ItemEnchantMode::kReset;
  const auto wiped = EnchantedCards(cards, result);
  // LA CARTE DE LA PIÈCE RESTE, les enchantements s'en vont.
  CHECK(wiped[0] == 4001);
  CHECK(wiped[2] == 0);
  result.message_id = ro::sim::kItemEnchantFailedMessage;
  CHECK(EnchantedCards(cards, result) == cards);
}

TEST_CASE("Enchantement : le joueur qui ferme le dit au serveur, une fois", "[sim][item_enchant]") {
  ItemEnchant enchant = Opened(kArmorSlot);
  enchant.Close();
  CHECK_FALSE(enchant.open());
  CHECK(enchant.TakeFarewell());
  CHECK_FALSE(enchant.TakeFarewell());
}
