// La réforme. Voir l'en-tête pour ce qui vient de la table et les refus muets.

#include "ro/sim/item_reform.h"

#include <algorithm>
#include <utility>

#include "ro/sim/inventory.h"

namespace ro::sim {
namespace {

// ⚠ LE SERVEUR COMPTE JUSQU'À LA DERNIÈRE OPTION REMPLIE, pas les options
// remplies : une case vide avant elle compte comme pleine.
[[nodiscard]] std::size_t OptionReach(const InventoryItem& item) noexcept {
  for (std::size_t rank = item.options.size(); rank > 0; --rank) {
    if (item.options[rank - 1].filled()) {
      return rank;
    }
  }
  return 0;
}

[[nodiscard]] bool HoldsAnyCard(const InventoryItem& item) noexcept {
  return std::ranges::any_of(item.cards, [](std::uint32_t card) { return card != 0; });
}

}  // namespace

const ItemReformRecipe* ItemReformTrigger::FindRecipe(std::uint32_t base_item_id) const noexcept {
  const auto found = std::ranges::find_if(recipes, [base_item_id](const ItemReformRecipe& recipe) {
    return recipe.base_item_id == base_item_id;
  });
  return found == recipes.end() ? nullptr : &*found;
}

ItemReformRefusal JudgeItemReformPiece(const ItemReformRecipe& recipe, const InventoryItem& item) {
  if (item.item_id != recipe.base_item_id) {
    return ItemReformRefusal::kNotReformable;
  }
  if (!item.identified) {
    return ItemReformRefusal::kUnidentified;
  }
  if (item.equipped()) {
    return ItemReformRefusal::kWorn;
  }
  if (item.refine_level < recipe.least_refine) {
    return ItemReformRefusal::kRefineTooLow;
  }
  if (item.refine_level > recipe.most_refine) {
    return ItemReformRefusal::kRefineTooHigh;
  }
  // ⚠ LES QUATRE CASES BRUTES : la marque d'un forgeron y compte comme une carte.
  if (!recipe.cards_allowed && HoldsAnyCard(item)) {
    return ItemReformRefusal::kCardsForbidden;
  }
  if (OptionReach(item) < recipe.least_options) {
    return ItemReformRefusal::kTooFewOptions;
  }
  return ItemReformRefusal::kNone;
}

std::uint32_t CountedMaterial(const Inventory& inventory, std::uint32_t item_id) {
  const InventoryItem* first = nullptr;
  for (const InventoryItem& item : inventory.items()) {
    if (item.item_id == item_id && item.amount > 0 &&
        (first == nullptr || item.client_slot < first->client_slot)) {
      first = &item;
    }
  }
  return first == nullptr ? 0U : static_cast<std::uint32_t>(first->amount);
}

const ItemReformMaterial* MissingMaterial(const ItemReformRecipe& recipe,
                                          const Inventory& inventory) {
  const auto missing =
      std::ranges::find_if(recipe.materials, [&inventory](const ItemReformMaterial& material) {
        return CountedMaterial(inventory, material.item_id) < material.amount;
      });
  return missing == recipe.materials.end() ? nullptr : &*missing;
}

std::uint8_t ReformedRefine(const ItemReformTrigger& trigger, const ItemReformRecipe& recipe,
                            std::uint8_t refine_level) noexcept {
  const int reformed = static_cast<int>(refine_level) + recipe.refine_change;
  return static_cast<std::uint8_t>(std::clamp(reformed, 0, static_cast<int>(trigger.refine_cap)));
}

void ItemReform::Reset() noexcept {
  chosen_.reset();
  ++choice_seq_;
  waiting_ = false;
}

void ItemReform::TellOpening(std::uint32_t trigger_item_id) {
  // L'ouverture prouve que le serveur a réarmé : une fermeture encore due le
  // refermerait.
  farewell_due_ = false;
  open_ = true;
  trigger_item_id_ = trigger_item_id;
  ++opening_seq_;
  reformed_slot_.reset();
  Reset();
}

bool ItemReform::Choose(std::uint16_t client_slot) {
  if (!open_ || waiting_) {
    return false;
  }
  if (chosen_ != client_slot) {
    chosen_ = client_slot;
    ++choice_seq_;
  }
  return true;
}

void ItemReform::TellResult(std::uint8_t code, std::uint16_t client_slot) {
  result_ = code;
  ++result_seq_;
  waiting_ = false;
  if (code == kItemReformSucceeded && open_) {
    open_ = false;
    reformed_slot_ = client_slot;
    Reset();
  }
}

std::optional<std::uint16_t> ItemReform::TakeReformed() noexcept {
  return std::exchange(reformed_slot_, std::nullopt);
}

void ItemReform::Close() {
  if (!open_) {
    return;
  }
  farewell_due_ = true;
  open_ = false;
  Reset();
}

void ItemReform::CloseSilently() noexcept {
  open_ = false;
  farewell_due_ = false;
  reformed_slot_.reset();
  Reset();
}

bool ItemReform::TakeFarewell() noexcept { return std::exchange(farewell_due_, false); }

ItemReformCheck CheckItemReformRequest(const ItemReform& reform, const ItemReformTrigger& trigger,
                                       const Inventory& inventory) {
  if (!reform.open()) {
    return {.refusal = ItemReformRefusal::kNotOpen};
  }
  if (reform.waiting()) {
    return {.refusal = ItemReformRefusal::kWaiting};
  }
  if (!reform.chosen_slot().has_value()) {
    return {.refusal = ItemReformRefusal::kNothingChosen};
  }
  const InventoryItem* item = inventory.Find(*reform.chosen_slot());
  if (item == nullptr) {
    return {.refusal = ItemReformRefusal::kPieceGone};
  }
  const ItemReformRecipe* recipe = trigger.FindRecipe(item->item_id);
  if (recipe == nullptr) {
    return {.refusal = ItemReformRefusal::kNotReformable};
  }
  if (const ItemReformRefusal refusal = JudgeItemReformPiece(*recipe, *item);
      refusal != ItemReformRefusal::kNone) {
    return {.refusal = refusal};
  }
  if (const ItemReformMaterial* missing = MissingMaterial(*recipe, inventory); missing != nullptr) {
    return {.refusal = ItemReformRefusal::kMissingMaterial, .material_id = missing->item_id};
  }
  return {};
}

}  // namespace ro::sim
