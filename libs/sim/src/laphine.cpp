// Le Laphine. Voir l'en-tête pour la fermeture due et ce qui vient de la table.

#include "ro/sim/laphine.h"

#include <algorithm>
#include <utility>

#include "ro/sim/inventory.h"

namespace ro::sim {
namespace {

[[nodiscard]] std::size_t FilledOptions(const InventoryItem& item) noexcept {
  return static_cast<std::size_t>(std::ranges::count_if(
      item.options, [](const ItemOption& option) { return option.filled(); }));
}

[[nodiscard]] bool HoldsAnyCard(const InventoryItem& item) noexcept {
  return std::ranges::any_of(item.cards, [](std::uint32_t card) { return card != 0; });
}

}  // namespace

const LaphineMaterial* LaphineRecipe::FindMaterial(std::uint32_t item_id) const noexcept {
  const auto found = std::ranges::find_if(materials, [item_id](const LaphineMaterial& material) {
    return material.item_id == item_id;
  });
  return found == materials.end() ? nullptr : &*found;
}

LaphineRefusal JudgeLaphineItem(const LaphineRecipe& recipe, LaphineKind kind,
                                const InventoryItem& item) {
  const LaphineMaterial* material = recipe.FindMaterial(item.item_id);
  if (material == nullptr) {
    return LaphineRefusal::kNotAMaterial;
  }
  if (item.equipped()) {
    return LaphineRefusal::kWorn;
  }
  if (item.refine_level < recipe.least_refine) {
    return LaphineRefusal::kRefineTooLow;
  }
  if (item.refine_level > recipe.most_refine) {
    return LaphineRefusal::kRefineTooHigh;
  }
  if (kind == LaphineKind::kSynthesis) {
    return item.amount < material->amount ? LaphineRefusal::kTooFewInPile : LaphineRefusal::kNone;
  }
  if (!item.identified) {
    return LaphineRefusal::kUnidentified;
  }
  if (FilledOptions(item) < recipe.least_options) {
    return LaphineRefusal::kTooFewOptions;
  }
  // ⚠ LE SERVEUR REGARDE LES QUATRE CASES BRUTES : la marque d'un forgeron y compte
  // comme une carte.
  if (!recipe.cards_allowed && HoldsAnyCard(item)) {
    return LaphineRefusal::kCardsForbidden;
  }
  return LaphineRefusal::kNone;
}

LaphineWarnings WarnLaphineItem(const LaphineRecipe& recipe, const InventoryItem& item) {
  return LaphineWarnings{
      .refined_above_need = recipe.least_refine != 0 && item.refine_level > recipe.least_refine,
      .has_cards = ItemCardCount(item) > 0};
}

void Laphine::TellOpening(LaphineKind kind, std::uint32_t trigger_item_id) {
  if (open_ && kind_ != kind) {
    (kind_ == LaphineKind::kSynthesis ? synthesis_farewell_due_ : upgrade_farewell_due_) = true;
  }
  // L'ouverture prouve que le serveur a réarmé cette espèce : une fermeture encore
  // due la refermerait.
  (kind == LaphineKind::kSynthesis ? synthesis_farewell_due_ : upgrade_farewell_due_) = false;
  open_ = true;
  kind_ = kind;
  trigger_item_id_ = trigger_item_id;
  ++opening_seq_;
  piles_.clear();
  ++piles_seq_;
  waiting_ = false;
  upgraded_slot_.reset();
}

bool Laphine::Deposited(std::uint16_t client_slot) const noexcept {
  return std::ranges::find(piles_, client_slot) != piles_.end();
}

bool Laphine::Deposit(std::uint16_t client_slot) {
  if (!open_ || waiting_ || Deposited(client_slot)) {
    return false;
  }
  piles_.push_back(client_slot);
  ++piles_seq_;
  return true;
}

bool Laphine::Withdraw(std::uint16_t client_slot) {
  const auto found = std::ranges::find(piles_, client_slot);
  if (waiting_ || found == piles_.end()) {
    return false;
  }
  piles_.erase(found);
  ++piles_seq_;
  return true;
}

void Laphine::TellResult(std::uint16_t code) {
  result_ = code;
  ++result_seq_;
  waiting_ = false;
  const std::uint16_t succeeded =
      kind_ == LaphineKind::kSynthesis ? kLaphineSynthesisSucceeded : kLaphineUpgradeSucceeded;
  if (code == succeeded) {
    Close();
  }
}

std::optional<std::uint16_t> Laphine::TakeUpgraded() noexcept {
  return std::exchange(upgraded_slot_, std::nullopt);
}

void Laphine::Close() {
  if (!open_) {
    return;
  }
  (kind_ == LaphineKind::kSynthesis ? synthesis_farewell_due_ : upgrade_farewell_due_) = true;
  open_ = false;
  waiting_ = false;
  piles_.clear();
  ++piles_seq_;
}

void Laphine::CloseSilently() noexcept {
  open_ = false;
  waiting_ = false;
  piles_.clear();
  ++piles_seq_;
  synthesis_farewell_due_ = false;
  upgrade_farewell_due_ = false;
  upgraded_slot_.reset();
}

std::optional<LaphineKind> Laphine::TakeFarewell() noexcept {
  if (synthesis_farewell_due_) {
    synthesis_farewell_due_ = false;
    return LaphineKind::kSynthesis;
  }
  if (upgrade_farewell_due_) {
    upgrade_farewell_due_ = false;
    return LaphineKind::kUpgrade;
  }
  return std::nullopt;
}

LaphineCheck CheckLaphineRequest(const Laphine& laphine, const LaphineRecipe& recipe,
                                 const Inventory& inventory) {
  if (!laphine.open()) {
    return {.refusal = LaphineRefusal::kNotOpen};
  }
  if (laphine.waiting()) {
    return {.refusal = LaphineRefusal::kWaiting};
  }
  if (laphine.piles().size() != recipe.needed_piles) {
    return {.refusal = LaphineRefusal::kWrongPileCount};
  }
  for (const std::uint16_t client_slot : laphine.piles()) {
    const InventoryItem* item = inventory.Find(client_slot);
    if (item == nullptr) {
      return {.refusal = LaphineRefusal::kPileGone, .client_slot = client_slot};
    }
    if (const LaphineRefusal refusal = JudgeLaphineItem(recipe, laphine.kind(), *item);
        refusal != LaphineRefusal::kNone) {
      return {.refusal = refusal, .client_slot = client_slot};
    }
  }
  return {};
}

}  // namespace ro::sim
