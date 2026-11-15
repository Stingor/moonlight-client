// L'amélioration de grade. Voir l'en-tête pour les refus muets.

#include "ro/sim/grade_enchant.h"

#include <algorithm>
#include <utility>

#include "ro/sim/equip_location.h"
#include "ro/sim/inventory.h"
#include "ro/sim/item_reform.h"

namespace ro::sim {

GradeEnchantRefusal JudgeGradePiece(const InventoryItem& item) {
  // ⚠ L'ARME D'OMBRE ARRIVE COMME UNE ARME, mais le serveur ne lui connaît aucun
  // grade : il ne répondrait pas.
  if (item.type != kGradeWeaponType || IsShadowLocation(item.wearable_at)) {
    return GradeEnchantRefusal::kNotGradable;
  }
  if (!item.identified) {
    return GradeEnchantRefusal::kUnidentified;
  }
  // PORTÉE, ELLE NE SE CHOISIT PAS : le serveur l'accepterait, mais ne recalculerait
  // pas l'attaque du porteur.
  if (item.equipped()) {
    return GradeEnchantRefusal::kWorn;
  }
  if (item.grade >= kGradeTop) {
    return GradeEnchantRefusal::kTopGrade;
  }
  return GradeEnchantRefusal::kNone;
}

std::uint32_t GradeEnchantChance(const GradeEnchantOffer& offer,
                                 std::uint32_t blessing_steps) noexcept {
  if (offer.chance_percent == 0) {
    return 0;
  }
  const std::uint32_t steps = std::min(blessing_steps, offer.blessing.most_steps);
  return std::min<std::uint32_t>(offer.chance_percent + steps * offer.blessing.bonus_percent, 100);
}

void GradeEnchant::Reset() noexcept {
  chosen_.reset();
  offer_.reset();
  selection_due_ = false;
  ++choice_seq_;
  waiting_ = false;
}

void GradeEnchant::TellOpening() {
  // L'ouverture prouve que le serveur a réarmé : une fermeture encore due le
  // refermerait.
  farewell_due_ = false;
  open_ = true;
  ++opening_seq_;
  Reset();
}

bool GradeEnchant::Choose(std::uint16_t client_slot) {
  if (!open_ || waiting_) {
    return false;
  }
  if (chosen_ != client_slot) {
    chosen_ = client_slot;
    offer_.reset();
    selection_due_ = true;
    ++choice_seq_;
  }
  return true;
}

void GradeEnchant::Forget() noexcept {
  if (chosen_.has_value()) {
    chosen_.reset();
    offer_.reset();
    selection_due_ = false;
    ++choice_seq_;
  }
}

std::optional<std::uint16_t> GradeEnchant::TakeSelection() noexcept {
  if (!std::exchange(selection_due_, false) || !open_) {
    return std::nullopt;
  }
  return chosen_;
}

void GradeEnchant::TellOffer(GradeEnchantOffer offer) {
  if (!open_ || chosen_ != offer.client_slot) {
    return;
  }
  offer_ = std::move(offer);
  ++choice_seq_;
}

void GradeEnchant::TellResult(GradeEnchantResult result) {
  result_ = result;
  ++result_seq_;
  waiting_ = false;
  // LA PIÈCE A CHANGÉ OU DISPARU : sa proposition ne vaut plus.
  offer_.reset();
  selection_due_ = open_ && chosen_.has_value();
  ++choice_seq_;
}

void GradeEnchant::Close() {
  if (!open_) {
    return;
  }
  farewell_due_ = true;
  open_ = false;
  Reset();
}

void GradeEnchant::CloseSilently() noexcept {
  open_ = false;
  farewell_due_ = false;
  Reset();
}

bool GradeEnchant::TakeFarewell() noexcept { return std::exchange(farewell_due_, false); }

GradeEnchantCheck CheckGradeEnchantRequest(const GradeEnchant& grade, GradeEnchantAttempt attempt,
                                           const Inventory& inventory, std::int64_t zeny) {
  if (!grade.open()) {
    return {.refusal = GradeEnchantRefusal::kNotOpen};
  }
  if (grade.waiting()) {
    return {.refusal = GradeEnchantRefusal::kWaiting};
  }
  if (!grade.chosen_slot().has_value()) {
    return {.refusal = GradeEnchantRefusal::kNothingChosen};
  }
  const InventoryItem* item = inventory.Find(*grade.chosen_slot());
  if (item == nullptr) {
    return {.refusal = GradeEnchantRefusal::kPieceGone};
  }
  if (const GradeEnchantRefusal refusal = JudgeGradePiece(*item);
      refusal != GradeEnchantRefusal::kNone) {
    return {.refusal = refusal};
  }
  const std::optional<GradeEnchantOffer>& offer = grade.offer();
  if (!offer.has_value()) {
    return {.refusal = GradeEnchantRefusal::kAwaitingOffer};
  }
  if (!offer->supported) {
    return {.refusal = GradeEnchantRefusal::kLevelUnsupported};
  }
  if (offer->chance_percent == 0) {
    return {.refusal = GradeEnchantRefusal::kRefineTooLow};
  }
  if (attempt.stake >= offer->stakes.size()) {
    return {.refusal = GradeEnchantRefusal::kNoSuchStake};
  }
  const GradeEnchantStake& stake = offer->stakes[attempt.stake];
  const std::uint32_t steps = std::min(attempt.blessing_steps, offer->blessing.most_steps);
  const std::uint32_t dust = offer->blessing.item_id == 0 ? 0 : offer->blessing.per_step * steps;
  // ⚠ LA POUSSIÈRE ET LE JOYAU PEUVENT ÊTRE LE MÊME OBJET : le serveur additionne.
  const bool same_item = dust > 0 && offer->blessing.item_id == stake.item_id;
  const std::uint32_t stake_needed = stake.amount + (same_item ? dust : 0);
  if (CountedMaterial(inventory, stake.item_id) < stake_needed) {
    return {.refusal = GradeEnchantRefusal::kMissingStake, .item_id = stake.item_id};
  }
  if (dust > 0 && !same_item && CountedMaterial(inventory, offer->blessing.item_id) < dust) {
    return {.refusal = GradeEnchantRefusal::kMissingBlessing, .item_id = offer->blessing.item_id};
  }
  if (zeny < stake.zeny) {
    return {.refusal = GradeEnchantRefusal::kNotEnoughZeny};
  }
  return {};
}

}  // namespace ro::sim
