// L'AMÉLIORATION DE GRADE VUE PAR LA FENÊTRE : les armes listées, la proposition
// montrée, et ce qui retient le bouton.

#include "game/grade_enchant_view.h"

#include <cstdint>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/grade_enchant.h"
#include "ro/sim/inventory.h"

using moonclient::GradeEnchantLooks;
using moonclient::GradeEnchantScene;
using moonclient::GradeEnchantViewComposer;
using ro::sim::GradeEnchant;
using ro::sim::GradeEnchantOffer;
using ro::sim::GradeEnchantStake;
using ro::sim::Inventory;
using ro::sim::InventoryItem;

namespace {

constexpr std::uint16_t kSwordSlot = 12;
constexpr std::uint16_t kWornSlot = 13;
constexpr std::uint16_t kJewelSlot = 20;
constexpr std::uint32_t kSwordId = 1116;
constexpr std::uint32_t kTopazId = 1000326;
constexpr std::int64_t kRich = 10000000;

[[nodiscard]] GradeEnchantLooks Looks() {
  return GradeEnchantLooks{
      .name_of_held = [](const InventoryItem& item) { return "#" + std::to_string(item.item_id); },
      .name_of_id = [](std::uint32_t item_id) { return "#" + std::to_string(item_id); },
      .icon_of = [](std::uint32_t) { return ro::ui::SkinImage{}; }};
}

[[nodiscard]] Inventory Bag() {
  Inventory bag;
  bag.Put(InventoryItem{.client_slot = kSwordSlot,
                        .item_id = kSwordId,
                        .type = ro::sim::kGradeWeaponType,
                        .amount = 1,
                        .refine_level = 8,
                        .grade = 1,
                        .identified = true});
  bag.Put(InventoryItem{.client_slot = kWornSlot,
                        .item_id = kSwordId,
                        .type = ro::sim::kGradeWeaponType,
                        .amount = 1,
                        .worn_at = 2,
                        .identified = true});
  bag.Put(InventoryItem{.client_slot = kJewelSlot, .item_id = kTopazId, .type = 3, .amount = 1});
  return bag;
}

[[nodiscard]] GradeEnchantOffer Offer() {
  GradeEnchantOffer offer;
  offer.client_slot = kSwordSlot;
  offer.supported = true;
  offer.chance_percent = 20;
  offer.stakes = {
      GradeEnchantStake{.item_id = kTopazId, .amount = 1, .zeny = 175000, .breaks = true},
      GradeEnchantStake{.item_id = kTopazId, .amount = 5, .zeny = 875000}};
  return offer;
}

}  // namespace

TEST_CASE("Vue des grades : seules les armes se listent", "[app][grade_enchant]") {
  const Inventory bag = Bag();
  GradeEnchant grade;
  grade.TellOpening();
  const GradeEnchantLooks looks = Looks();
  GradeEnchantViewComposer composer;
  const ro::ui::GradeEnchantView& view = composer.Compose(
      GradeEnchantScene{.grade = grade, .inventory = bag, .looks = looks, .zeny = kRich});
  REQUIRE(view.candidates.size() == 2);
  CHECK(view.candidates[0].refusal.empty());
  // L'arme portée se montre, mais refusée.
  CHECK_FALSE(view.candidates[1].refusal.empty());
  CHECK(view.chosen.item_id == 0);
  REQUIRE(view.blockers.size() == 1);
}

TEST_CASE("Vue des grades : la proposition montre les mises", "[app][grade_enchant]") {
  const Inventory bag = Bag();
  GradeEnchant grade;
  grade.TellOpening();
  REQUIRE(grade.Choose(kSwordSlot));
  const GradeEnchantLooks looks = Looks();
  GradeEnchantViewComposer composer;
  const GradeEnchantScene waiting{.grade = grade, .inventory = bag, .looks = looks, .zeny = kRich};
  CHECK_FALSE(composer.Compose(waiting).offer_ready);

  grade.TellOffer(Offer());
  const ro::ui::GradeEnchantView& view = composer.Compose(
      GradeEnchantScene{.grade = grade, .inventory = bag, .looks = looks, .zeny = kRich});
  CHECK(view.offer_ready);
  CHECK(view.offer_refusal.empty());
  CHECK(view.step == "Grade : D, puis C");
  CHECK(view.chance_percent == 20);
  REQUIRE(view.stakes.size() == 2);
  CHECK(view.stakes[0].risky);
  CHECK(view.stakes[1].owned == 1);
  CHECK(view.stakes[1].needed == 5);
  CHECK(view.blockers.empty());
  CHECK_FALSE(view.confirmation.empty());

  // La grosse mise manque de joyaux : le bouton se retient.
  const ro::ui::GradeEnchantView& big = composer.Compose(GradeEnchantScene{
      .grade = grade, .inventory = bag, .looks = looks, .attempt = {.stake = 1}, .zeny = kRich});
  CHECK(big.blockers.size() == 1);
}

TEST_CASE("Vue des grades : sous le raffinement exigé, la proposition le dit",
          "[app][grade_enchant]") {
  const Inventory bag = Bag();
  GradeEnchant grade;
  grade.TellOpening();
  REQUIRE(grade.Choose(kSwordSlot));
  GradeEnchantOffer offer = Offer();
  offer.chance_percent = 0;
  grade.TellOffer(offer);
  const GradeEnchantLooks looks = Looks();
  GradeEnchantViewComposer composer;
  const ro::ui::GradeEnchantView& view = composer.Compose(
      GradeEnchantScene{.grade = grade, .inventory = bag, .looks = looks, .zeny = kRich});
  CHECK(view.offer_ready);
  CHECK_FALSE(view.offer_refusal.empty());
  CHECK(view.stakes.empty());
}
