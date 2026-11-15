// L'AMÉLIORATION DE GRADE : les pièces que le serveur admet, la proposition qui ne
// vaut que pour la pièce choisie, et la demande jugée contre le sac et la bourse.

#include "ro/sim/grade_enchant.h"

#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/equip_location.h"
#include "ro/sim/inventory.h"

using ro::sim::GradeEnchant;
using ro::sim::GradeEnchantAttempt;
using ro::sim::GradeEnchantOffer;
using ro::sim::GradeEnchantRefusal;
using ro::sim::GradeEnchantResult;
using ro::sim::GradeEnchantStake;
using ro::sim::Inventory;
using ro::sim::InventoryItem;

namespace {

constexpr std::uint16_t kSwordSlot = 12;
constexpr std::uint16_t kJewelSlot = 20;
constexpr std::uint16_t kDustSlot = 21;
constexpr std::uint32_t kSwordId = 1116;
constexpr std::uint32_t kTopazId = 1000326;
constexpr std::uint32_t kDustId = 1000337;
// Le type d'arme sur le fil.
constexpr std::uint8_t kWeaponType = ro::sim::kGradeWeaponType;

[[nodiscard]] InventoryItem Sword() {
  return InventoryItem{.client_slot = kSwordSlot,
                       .item_id = kSwordId,
                       .type = kWeaponType,
                       .amount = 1,
                       .wearable_at = 2,
                       .refine_level = 8,
                       .grade = 1,
                       .identified = true};
}

[[nodiscard]] GradeEnchantOffer Offer() {
  GradeEnchantOffer offer;
  offer.client_slot = kSwordSlot;
  offer.supported = true;
  offer.chance_percent = 20;
  offer.blessing = {.item_id = kDustId, .per_step = 3, .most_steps = 10, .bonus_percent = 1};
  offer.stakes = {
      GradeEnchantStake{.item_id = kTopazId, .amount = 1, .zeny = 175000, .breaks = true},
      GradeEnchantStake{.item_id = kTopazId, .amount = 5, .zeny = 875000}};
  return offer;
}

[[nodiscard]] GradeEnchant Chosen() {
  GradeEnchant grade;
  grade.TellOpening();
  REQUIRE(grade.Choose(kSwordSlot));
  return grade;
}

}  // namespace

TEST_CASE("Grade : seules les armes libres et sous A se choisissent", "[sim][grade_enchant]") {
  CHECK(ro::sim::JudgeGradePiece(Sword()) == GradeEnchantRefusal::kNone);
  InventoryItem worn = Sword();
  worn.worn_at = 2;
  CHECK(ro::sim::JudgeGradePiece(worn) == GradeEnchantRefusal::kWorn);
  InventoryItem top = Sword();
  top.grade = ro::sim::kGradeTop;
  CHECK(ro::sim::JudgeGradePiece(top) == GradeEnchantRefusal::kTopGrade);
  InventoryItem jewel{.client_slot = kJewelSlot, .item_id = kTopazId, .type = 3, .amount = 1};
  CHECK(ro::sim::JudgeGradePiece(jewel) == GradeEnchantRefusal::kNotGradable);
  InventoryItem shadow = Sword();
  shadow.wearable_at = ro::sim::kEquipShadowMask;
  CHECK(ro::sim::JudgeGradePiece(shadow) == GradeEnchantRefusal::kNotGradable);
}

TEST_CASE("Grade : le choix part une fois, la proposition d'une autre pièce tombe",
          "[sim][grade_enchant]") {
  GradeEnchant grade = Chosen();
  CHECK(grade.TakeSelection() == kSwordSlot);
  CHECK_FALSE(grade.TakeSelection().has_value());
  GradeEnchantOffer other = Offer();
  other.client_slot = 99;
  grade.TellOffer(other);
  CHECK_FALSE(grade.offer().has_value());
  grade.TellOffer(Offer());
  REQUIRE(grade.offer().has_value());

  // Un résultat périme la proposition, qui se redemande.
  grade.MarkRequestSent();
  grade.TellResult(GradeEnchantResult::kFailed);
  CHECK_FALSE(grade.waiting());
  CHECK_FALSE(grade.offer().has_value());
  CHECK(grade.TakeSelection() == kSwordSlot);
}

TEST_CASE("Grade : la demande se juge contre le sac et la bourse", "[sim][grade_enchant]") {
  GradeEnchant grade = Chosen();
  Inventory bag;
  bag.Put(Sword());
  const GradeEnchantAttempt small{.stake = 0, .blessing_steps = 0};
  CHECK(ro::sim::CheckGradeEnchantRequest(grade, small, bag, 1000000).refusal ==
        GradeEnchantRefusal::kAwaitingOffer);
  grade.TellOffer(Offer());
  CHECK(ro::sim::CheckGradeEnchantRequest(grade, small, bag, 1000000).refusal ==
        GradeEnchantRefusal::kMissingStake);
  bag.Put(InventoryItem{.client_slot = kJewelSlot, .item_id = kTopazId, .type = 3, .amount = 1});
  CHECK(ro::sim::CheckGradeEnchantRequest(grade, small, bag, 1000000).passed());
  CHECK(ro::sim::CheckGradeEnchantRequest(grade, small, bag, 100).refusal ==
        GradeEnchantRefusal::kNotEnoughZeny);
  CHECK(ro::sim::CheckGradeEnchantRequest(grade, {.stake = 1}, bag, 1000000).refusal ==
        GradeEnchantRefusal::kMissingStake);

  const GradeEnchantAttempt blessed{.stake = 0, .blessing_steps = 2};
  CHECK(ro::sim::CheckGradeEnchantRequest(grade, blessed, bag, 1000000).refusal ==
        GradeEnchantRefusal::kMissingBlessing);
  bag.Put(InventoryItem{.client_slot = kDustSlot, .item_id = kDustId, .type = 3, .amount = 6});
  CHECK(ro::sim::CheckGradeEnchantRequest(grade, blessed, bag, 1000000).passed());
  CHECK(ro::sim::GradeEnchantChance(*grade.offer(), 2) == 22);
  CHECK(ro::sim::GradeEnchantChance(*grade.offer(), 99) == 30);
}

TEST_CASE("Grade : sous le raffinement exigé, la chance est nulle", "[sim][grade_enchant]") {
  GradeEnchant grade = Chosen();
  Inventory bag;
  bag.Put(Sword());
  GradeEnchantOffer offer = Offer();
  offer.chance_percent = 0;
  grade.TellOffer(offer);
  CHECK(ro::sim::CheckGradeEnchantRequest(grade, {}, bag, 1000000).refusal ==
        GradeEnchantRefusal::kRefineTooLow);
}

TEST_CASE("Grade : la fermeture du joueur se dit, celle d'un départ non", "[sim][grade_enchant]") {
  GradeEnchant grade = Chosen();
  grade.Close();
  CHECK_FALSE(grade.open());
  CHECK(grade.TakeFarewell());
  CHECK_FALSE(grade.TakeFarewell());
  grade.TellOpening();
  grade.CloseSilently();
  CHECK_FALSE(grade.TakeFarewell());
}
