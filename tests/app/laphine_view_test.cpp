// LE LIVRE DU LAPHINE ET CE QUE SA FENÊTRE MONTRE : la table lue sur un texte inventé,
// et la vue composée, refus et dépôts compris.

#include "game/laphine_view.h"

#include <cstdint>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/inventory.h"
#include "ro/sim/laphine.h"

#include "items/laphine_book.h"

using moonclient::LaphineBook;
using moonclient::LaphineLooks;
using moonclient::LaphineResultText;
using moonclient::LaphineScene;
using moonclient::LaphineViewComposer;
using ro::sim::Inventory;
using ro::sim::InventoryItem;
using ro::sim::Laphine;
using ro::sim::LaphineKind;
using ro::sim::LaphineRecipe;

namespace {

constexpr std::uint32_t kStoneId = 100003;
constexpr std::uint32_t kSwordId = 28725;
constexpr std::uint32_t kAxeId = 1326;
constexpr std::uint16_t kSwordSlot = 12;
constexpr std::uint16_t kAxeSlot = 13;

constexpr const char* kBookText = R"(
version: 1
mode: Prerenewal
synthesis:
  - item: 9000
    needed: 3
    least_refine: 1
    most_refine: 10
    materials: [[501, 5], [1101, 1]]
upgrade:
  - item: 100003
    least_refine: 0
    most_refine: 10
    least_options: 2
    cards_allowed: true
    targets: [1326, 28725]
)";

[[nodiscard]] LaphineLooks TestLooks() {
  return LaphineLooks{
      .name_of_held =
          [](const InventoryItem& item) {
            return "+" + std::to_string(item.refine_level) + " #" + std::to_string(item.item_id);
          },
      .name_of_id = [](std::uint32_t item_id) { return "#" + std::to_string(item_id); },
      .icon_of = [](std::uint32_t) { return ro::ui::SkinImage{}; }};
}

[[nodiscard]] LaphineRecipe Recipe() {
  return LaphineRecipe{.trigger_item_id = kStoneId,
                       .most_refine = 10,
                       .materials = {{.item_id = kAxeId}, {.item_id = kSwordId}}};
}

[[nodiscard]] InventoryItem Piece(std::uint16_t slot, std::uint32_t item_id, std::uint8_t refine) {
  InventoryItem item{.client_slot = slot, .item_id = item_id, .amount = 1, .identified = true};
  item.refine_level = refine;
  return item;
}

}  // namespace

TEST_CASE("Livre du Laphine : la table se lit par espèce", "[app][laphine]") {
  const auto book = LaphineBook::Parse(kBookText);
  REQUIRE(book.has_value());

  const LaphineRecipe* synthesis = book->Find(LaphineKind::kSynthesis, 9000);
  REQUIRE(synthesis != nullptr);
  CHECK(synthesis->needed_piles == 3);
  CHECK(synthesis->least_refine == 1);
  REQUIRE(synthesis->materials.size() == 2);
  CHECK(synthesis->materials[0].item_id == 501);
  CHECK(synthesis->materials[0].amount == 5);

  const LaphineRecipe* upgrade = book->Find(LaphineKind::kUpgrade, kStoneId);
  REQUIRE(upgrade != nullptr);
  CHECK(upgrade->least_options == 2);
  CHECK(upgrade->cards_allowed);
  REQUIRE(upgrade->materials.size() == 2);
  CHECK(upgrade->materials[1].item_id == kSwordId);
  CHECK(upgrade->materials[1].amount == 1);

  // Le déclencheur d'une espèce n'ouvre pas l'autre.
  CHECK(book->Find(LaphineKind::kUpgrade, 9000) == nullptr);
  CHECK(book->Find(LaphineKind::kSynthesis, kStoneId) == nullptr);

  // Un déclencheur se reconnaît, de l'une ou l'autre espèce.
  CHECK(book->Triggers(9000));
  CHECK(book->Triggers(kStoneId));
  CHECK_FALSE(book->Triggers(kSwordId));
}

TEST_CASE("Livre du Laphine : une table illisible se refuse", "[app][laphine]") {
  CHECK_FALSE(LaphineBook::Parse("version: 99\n").has_value());
  CHECK_FALSE(LaphineBook::Parse("[1, 2]").has_value());
  CHECK_FALSE(LaphineBook::Parse("{ pas fermé").has_value());
  const auto empty = LaphineBook::Parse("version: 1\n");
  REQUIRE(empty.has_value());
  CHECK(empty->upgrade().empty());
}

TEST_CASE("Laphine : le résultat ne se dit que s'il refuse", "[app][laphine]") {
  CHECK(LaphineResultText(LaphineKind::kUpgrade, ro::sim::kLaphineUpgradeSucceeded).empty());
  CHECK_FALSE(LaphineResultText(LaphineKind::kUpgrade, 1).empty());
  CHECK(LaphineResultText(LaphineKind::kSynthesis, ro::sim::kLaphineSynthesisSucceeded).empty());
  CHECK(LaphineResultText(LaphineKind::kSynthesis, ro::sim::kLaphineSynthesisWrongAmount) ==
        LaphineResultText(LaphineKind::kSynthesis, ro::sim::kLaphineSynthesisWrongItem));
}

TEST_CASE("Laphine : la vue sépare le sac et le dépôt", "[app][laphine]") {
  const LaphineRecipe recipe = Recipe();
  const LaphineLooks looks = TestLooks();
  Inventory bag;
  bag.Put(Piece(kSwordSlot, kSwordId, 3));
  InventoryItem worn = Piece(kAxeSlot, kAxeId, 2);
  worn.worn_at = 2;
  bag.Put(worn);
  // Ce que la recette n'accepte pas n'apparaît pas.
  bag.Put(Piece(14, 501, 0));

  Laphine laphine;
  laphine.TellOpening(LaphineKind::kUpgrade, kStoneId);

  LaphineViewComposer composer;
  const ro::ui::LaphineView& first = composer.Compose(LaphineScene{
      .laphine = laphine, .recipe = recipe, .inventory = bag, .looks = looks, .notice = "refusé"});
  CHECK(first.title == "#100003");
  CHECK(first.notice == "refusé");
  REQUIRE(first.candidates.size() == 2);
  CHECK(first.candidates[0].client_slot == kSwordSlot);
  CHECK(first.candidates[0].item.name == "+3 #28725");
  CHECK(first.candidates[0].refusal.empty());
  // La pièce portée se montre, avec sa raison.
  CHECK(first.candidates[1].client_slot == kAxeSlot);
  CHECK_FALSE(first.candidates[1].refusal.empty());
  CHECK(first.deposited.empty());
  CHECK(first.accepted.size() == 2);
  CHECK(first.blockers.size() == 1);

  REQUIRE(laphine.Deposit(kSwordSlot));
  const ro::ui::LaphineView& second = composer.Compose(LaphineScene{
      .laphine = laphine, .recipe = recipe, .inventory = bag, .looks = looks, .notice = {}});
  REQUIRE(second.candidates.size() == 1);
  CHECK(second.candidates[0].client_slot == kAxeSlot);
  REQUIRE(second.deposited.size() == 1);
  CHECK(second.deposited[0].item.name == "+3 #28725");
  CHECK(second.blockers.empty());
  CHECK(second.piles_seq == laphine.piles_seq());
  // La confirmation nomme ce qui part.
  CHECK(second.confirmation.find("+3 #28725") != std::string_view::npos);
}

TEST_CASE("Laphine : une synthèse montre la quantité et une attente se voit", "[app][laphine]") {
  const LaphineRecipe recipe{.trigger_item_id = kStoneId,
                             .needed_piles = 1,
                             .most_refine = 10,
                             .materials = {{.item_id = kSwordId, .amount = 5}}};
  const LaphineLooks looks = TestLooks();
  Inventory bag;
  InventoryItem pile = Piece(kSwordSlot, kSwordId, 0);
  pile.amount = 4;
  bag.Put(pile);
  Laphine laphine;
  laphine.TellOpening(LaphineKind::kSynthesis, kStoneId);

  LaphineViewComposer composer;
  const ro::ui::LaphineView& view = composer.Compose(LaphineScene{
      .laphine = laphine, .recipe = recipe, .inventory = bag, .looks = looks, .notice = {}});
  REQUIRE(view.candidates.size() == 1);
  CHECK(view.candidates[0].amount == 5);
  // Quatre sur cinq : la ligne le dit.
  CHECK_FALSE(view.candidates[0].refusal.empty());
  REQUIRE(view.accepted.size() == 1);
  CHECK(view.accepted[0].amount == 5);
  CHECK_FALSE(view.waiting);

  laphine.MarkRequestSent();
  const ro::ui::LaphineView& waiting = composer.Compose(LaphineScene{
      .laphine = laphine, .recipe = recipe, .inventory = bag, .looks = looks, .notice = {}});
  CHECK(waiting.waiting);
  // L'attente retient le bouton sans être un refus à lire.
  CHECK(waiting.blockers.empty());
}
