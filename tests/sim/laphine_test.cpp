// LE LAPHINE, SANS LE FIL : l'ouverture, le dépôt, la fermeture toujours due, et
// chaque raison qu'a le serveur de refuser.

#include "ro/sim/laphine.h"

#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/inventory.h"

using ro::sim::Inventory;
using ro::sim::InventoryItem;
using ro::sim::JudgeLaphineItem;
using ro::sim::Laphine;
using ro::sim::LaphineKind;
using ro::sim::LaphineRecipe;
using ro::sim::LaphineRefusal;

namespace {

constexpr std::uint32_t kStoneId = 100003;
constexpr std::uint32_t kSwordId = 1101;
constexpr std::uint32_t kHerbId = 501;
constexpr std::uint16_t kSwordSlot = 2;
constexpr std::uint16_t kHerbSlot = 3;
constexpr std::uint16_t kSecondSwordSlot = 4;

[[nodiscard]] LaphineRecipe UpgradeRecipe() {
  return LaphineRecipe{.trigger_item_id = kStoneId,
                       .least_refine = 4,
                       .most_refine = 9,
                       .materials = {{.item_id = kSwordId}}};
}

[[nodiscard]] LaphineRecipe SynthesisRecipe() {
  return LaphineRecipe{.trigger_item_id = kStoneId,
                       .needed_piles = 2,
                       .most_refine = 10,
                       .materials = {{.item_id = kSwordId}, {.item_id = kHerbId, .amount = 5}}};
}

[[nodiscard]] InventoryItem Sword(std::uint16_t slot, std::uint8_t refine) {
  InventoryItem item{.client_slot = slot, .item_id = kSwordId, .amount = 1, .identified = true};
  item.refine_level = refine;
  return item;
}

}  // namespace

TEST_CASE("Laphine : l'amélioration juge la pièce comme le serveur", "[sim][laphine]") {
  const LaphineRecipe recipe = UpgradeRecipe();
  CHECK(JudgeLaphineItem(recipe, LaphineKind::kUpgrade, Sword(kSwordSlot, 5)) ==
        LaphineRefusal::kNone);
  CHECK(JudgeLaphineItem(recipe, LaphineKind::kUpgrade, Sword(kSwordSlot, 3)) ==
        LaphineRefusal::kRefineTooLow);
  CHECK(JudgeLaphineItem(recipe, LaphineKind::kUpgrade, Sword(kSwordSlot, 10)) ==
        LaphineRefusal::kRefineTooHigh);

  InventoryItem worn = Sword(kSwordSlot, 5);
  worn.worn_at = 2;
  CHECK(JudgeLaphineItem(recipe, LaphineKind::kUpgrade, worn) == LaphineRefusal::kWorn);

  InventoryItem veiled = Sword(kSwordSlot, 5);
  veiled.identified = false;
  CHECK(JudgeLaphineItem(recipe, LaphineKind::kUpgrade, veiled) == LaphineRefusal::kUnidentified);
  // La synthèse ne le demande pas : le serveur ne le regarde que pour l'amélioration.
  CHECK(JudgeLaphineItem(SynthesisRecipe(), LaphineKind::kSynthesis, veiled) ==
        LaphineRefusal::kNone);

  InventoryItem carded = Sword(kSwordSlot, 5);
  carded.cards[0] = 4001;
  CHECK(JudgeLaphineItem(recipe, LaphineKind::kUpgrade, carded) == LaphineRefusal::kCardsForbidden);
  LaphineRecipe admitting = recipe;
  admitting.cards_allowed = true;
  CHECK(JudgeLaphineItem(admitting, LaphineKind::kUpgrade, carded) == LaphineRefusal::kNone);

  LaphineRecipe with_options = recipe;
  with_options.least_options = 1;
  CHECK(JudgeLaphineItem(with_options, LaphineKind::kUpgrade, Sword(kSwordSlot, 5)) ==
        LaphineRefusal::kTooFewOptions);
  InventoryItem optioned = Sword(kSwordSlot, 5);
  optioned.options[1].id = 7;
  CHECK(JudgeLaphineItem(with_options, LaphineKind::kUpgrade, optioned) == LaphineRefusal::kNone);

  const InventoryItem other{.client_slot = kHerbSlot, .item_id = kHerbId, .amount = 1};
  CHECK(JudgeLaphineItem(recipe, LaphineKind::kUpgrade, other) == LaphineRefusal::kNotAMaterial);
}

TEST_CASE("Laphine : la synthèse veut la quantité de la recette dans la pile", "[sim][laphine]") {
  const LaphineRecipe recipe = SynthesisRecipe();
  const InventoryItem few{.client_slot = kHerbSlot, .item_id = kHerbId, .amount = 4};
  const InventoryItem enough{.client_slot = kHerbSlot, .item_id = kHerbId, .amount = 5};
  CHECK(JudgeLaphineItem(recipe, LaphineKind::kSynthesis, few) == LaphineRefusal::kTooFewInPile);
  CHECK(JudgeLaphineItem(recipe, LaphineKind::kSynthesis, enough) == LaphineRefusal::kNone);
}

TEST_CASE("Laphine : les mises en garde n'empêchent rien", "[sim][laphine]") {
  const LaphineRecipe recipe = UpgradeRecipe();
  CHECK_FALSE(ro::sim::WarnLaphineItem(recipe, Sword(kSwordSlot, 4)).any());
  CHECK(ro::sim::WarnLaphineItem(recipe, Sword(kSwordSlot, 5)).refined_above_need);
  InventoryItem carded = Sword(kSwordSlot, 4);
  carded.cards[0] = 4001;
  CHECK(ro::sim::WarnLaphineItem(recipe, carded).has_cards);
}

TEST_CASE("Laphine : la demande se juge sur tout le dépôt", "[sim][laphine]") {
  const LaphineRecipe recipe = SynthesisRecipe();
  Inventory bag;
  bag.Put(Sword(kSwordSlot, 0));
  bag.Put(InventoryItem{.client_slot = kHerbSlot, .item_id = kHerbId, .amount = 5});

  Laphine laphine;
  CHECK(CheckLaphineRequest(laphine, recipe, bag).refusal == LaphineRefusal::kNotOpen);

  laphine.TellOpening(LaphineKind::kSynthesis, kStoneId);
  CHECK(CheckLaphineRequest(laphine, recipe, bag).refusal == LaphineRefusal::kWrongPileCount);
  REQUIRE(laphine.Deposit(kSwordSlot));
  CHECK_FALSE(laphine.Deposit(kSwordSlot));
  REQUIRE(laphine.Deposit(kHerbSlot));
  CHECK(CheckLaphineRequest(laphine, recipe, bag).passed());

  // Une case qui s'est vidée se nomme.
  bag.Take(kHerbSlot, 5);
  const ro::sim::LaphineCheck gone = CheckLaphineRequest(laphine, recipe, bag);
  CHECK(gone.refusal == LaphineRefusal::kPileGone);
  CHECK(gone.client_slot == kHerbSlot);

  // Pendant l'attente, rien ne bouge.
  laphine.MarkRequestSent();
  CHECK(CheckLaphineRequest(laphine, recipe, bag).refusal == LaphineRefusal::kWaiting);
  CHECK_FALSE(laphine.Deposit(kSecondSwordSlot));
  CHECK_FALSE(laphine.Withdraw(kSwordSlot));
}

TEST_CASE("Laphine : une réussite ferme, et la fermeture reste due", "[sim][laphine]") {
  Laphine laphine;
  laphine.TellOpening(LaphineKind::kUpgrade, kStoneId);
  REQUIRE(laphine.Deposit(kSwordSlot));
  laphine.MarkRequestSent();

  laphine.TellResult(ro::sim::kLaphineUpgradeSucceeded);
  CHECK_FALSE(laphine.open());
  CHECK_FALSE(laphine.waiting());
  CHECK(laphine.result_seq() == 1);
  // 🔴 SANS ELLE, LE PERSONNAGE RESTERAIT FIGÉ.
  CHECK(laphine.TakeFarewell() == LaphineKind::kUpgrade);
  CHECK_FALSE(laphine.TakeFarewell().has_value());
}

TEST_CASE("Laphine : un refus laisse la fenêtre ouverte pour un autre essai", "[sim][laphine]") {
  Laphine laphine;
  laphine.TellOpening(LaphineKind::kSynthesis, kStoneId);
  REQUIRE(laphine.Deposit(kSwordSlot));
  laphine.MarkRequestSent();

  laphine.TellResult(ro::sim::kLaphineSynthesisWrongItem);
  CHECK(laphine.open());
  CHECK_FALSE(laphine.waiting());
  CHECK(laphine.Deposited(kSwordSlot));
  CHECK_FALSE(laphine.TakeFarewell().has_value());
}

TEST_CASE("Laphine : un autre déclencheur ferme d'abord la fenêtre ouverte", "[sim][laphine]") {
  Laphine laphine;
  // Rien d'ouvert : l'usage part, et rien n'est dû.
  CHECK(laphine.GiveWayToTrigger());
  CHECK_FALSE(laphine.TakeFarewell().has_value());

  laphine.TellOpening(LaphineKind::kUpgrade, kStoneId);
  REQUIRE(laphine.Deposit(kSwordSlot));
  CHECK(laphine.GiveWayToTrigger());
  CHECK_FALSE(laphine.open());
  CHECK(laphine.piles().empty());
  // 🔴 SANS ELLE, LE SERVEUR REFUSERAIT LE NOUVEL OBJET.
  CHECK(laphine.TakeFarewell() == LaphineKind::kUpgrade);
}

TEST_CASE("Laphine : une demande en attente retient l'autre déclencheur", "[sim][laphine]") {
  Laphine laphine;
  laphine.TellOpening(LaphineKind::kUpgrade, kStoneId);
  REQUIRE(laphine.Deposit(kSwordSlot));
  laphine.MarkRequestSent();

  CHECK_FALSE(laphine.GiveWayToTrigger());
  CHECK(laphine.open());
  CHECK(laphine.Deposited(kSwordSlot));
  CHECK_FALSE(laphine.TakeFarewell().has_value());
}

TEST_CASE("Laphine : ce que le serveur a désarmé ne se redit pas", "[sim][laphine]") {
  Laphine laphine;
  laphine.TellOpening(LaphineKind::kUpgrade, kStoneId);
  laphine.CloseSilently();
  CHECK_FALSE(laphine.open());
  CHECK_FALSE(laphine.TakeFarewell().has_value());

  // Fermée puis déplacée avant l'envoi : la fermeture due tombe.
  laphine.TellOpening(LaphineKind::kUpgrade, kStoneId);
  laphine.Close();
  laphine.CloseSilently();
  CHECK_FALSE(laphine.TakeFarewell().has_value());
}

TEST_CASE("Laphine : l'autre espèce qui s'ouvre laisse la première à fermer", "[sim][laphine]") {
  Laphine laphine;
  laphine.TellOpening(LaphineKind::kUpgrade, kStoneId);
  const std::uint32_t first = laphine.opening_seq();
  laphine.TellOpening(LaphineKind::kSynthesis, kStoneId);
  CHECK(laphine.opening_seq() != first);
  CHECK(laphine.kind() == LaphineKind::kSynthesis);
  CHECK(laphine.TakeFarewell() == LaphineKind::kUpgrade);
  CHECK_FALSE(laphine.TakeFarewell().has_value());
}

TEST_CASE("Laphine : la pièce améliorée se désigne une fois", "[sim][laphine]") {
  Laphine laphine;
  laphine.NoteUpgraded(kSwordSlot);
  CHECK(laphine.TakeUpgraded() == kSwordSlot);
  CHECK_FALSE(laphine.TakeUpgraded().has_value());
}
