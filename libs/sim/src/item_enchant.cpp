// L'enchantement. Voir l'en-tête pour ce qui vient de la table, les refus muets et
// la séance que chaque réponse clôt.

#include "ro/sim/item_enchant.h"

#include <algorithm>
#include <utility>

#include "ro/sim/item_reform.h"

namespace ro::sim {
namespace {

[[nodiscard]] bool HoldsRandomOption(const InventoryItem& item) noexcept {
  return std::ranges::any_of(item.options,
                             [](const ItemOption& option) { return option.filled(); });
}

}  // namespace

const ItemEnchantDrawGrade* ItemEnchantSlot::DrawsFor(std::uint8_t grade) const noexcept {
  const auto found = std::ranges::find_if(
      draws, [grade](const ItemEnchantDrawGrade& draw) { return draw.grade == grade; });
  return found == draws.end() || found->enchants.empty() ? nullptr : &*found;
}

const ItemEnchantPerfect* ItemEnchantSlot::FindPerfect(std::uint32_t item_id) const noexcept {
  const auto found = std::ranges::find_if(
      perfect, [item_id](const ItemEnchantPerfect& entry) { return entry.item_id == item_id; });
  return found == perfect.end() ? nullptr : &*found;
}

const ItemEnchantUpgrade* ItemEnchantSlot::FindUpgrade(std::uint32_t from_item_id) const noexcept {
  const auto found =
      std::ranges::find_if(upgrades, [from_item_id](const ItemEnchantUpgrade& entry) {
        return entry.from_item_id == from_item_id;
      });
  return found == upgrades.end() ? nullptr : &*found;
}

bool ItemEnchantGroup::Targets(std::uint32_t item_id) const noexcept {
  return std::ranges::find(target_item_ids, item_id) != target_item_ids.end();
}

const ItemEnchantSlot* ItemEnchantGroup::FindSlot(std::uint8_t slot) const noexcept {
  const auto found = std::ranges::find_if(
      slots, [slot](const ItemEnchantSlot& entry) { return entry.slot == slot; });
  return found == slots.end() ? nullptr : &*found;
}

ItemEnchantRefusal JudgeItemEnchantPiece(const ItemEnchantGroup& group, const InventoryItem& item) {
  if (!group.Targets(item.item_id)) {
    return ItemEnchantRefusal::kNotTargeted;
  }
  if (item.equipped()) {
    return ItemEnchantRefusal::kWorn;
  }
  if (item.damaged) {
    return ItemEnchantRefusal::kDamaged;
  }
  if (item.refine_level < group.least_refine) {
    return ItemEnchantRefusal::kRefineTooLow;
  }
  if (item.grade < group.least_grade) {
    return ItemEnchantRefusal::kGradeTooLow;
  }
  if (!group.random_options_allowed && HoldsRandomOption(item)) {
    return ItemEnchantRefusal::kRandomOptionsForbidden;
  }
  return ItemEnchantRefusal::kNone;
}

std::optional<std::uint8_t> NextEnchantSlot(const ItemEnchantGroup& group,
                                            const InventoryItem& item,
                                            std::uint8_t card_slots) noexcept {
  if (ItemIsForged(item)) {
    return std::nullopt;
  }
  for (const std::uint8_t slot : group.order) {
    if (slot < kItemCardSlots && item.cards[slot] == 0) {
      // ⚠ LA PREMIÈRE LIBRE, MÊME UNE CASE DE CARTE : le serveur s'y arrête, puis
      // refuse. Elle ne se saute pas.
      if (slot < card_slots) {
        return std::nullopt;
      }
      return slot;
    }
  }
  return std::nullopt;
}

std::vector<std::uint8_t> EnchantedSlots(const InventoryItem& item, std::uint8_t card_slots) {
  std::vector<std::uint8_t> slots;
  if (ItemIsForged(item)) {
    return slots;
  }
  for (std::uint8_t slot = card_slots; slot < kItemCardSlots; ++slot) {
    if (item.cards[slot] != 0) {
      slots.push_back(slot);
    }
  }
  return slots;
}

const ItemEnchantMaterial* MissingEnchantMaterial(const ItemEnchantCost& cost,
                                                  const Inventory& inventory) {
  const auto missing =
      std::ranges::find_if(cost.materials, [&inventory](const ItemEnchantMaterial& material) {
        return CountedMaterial(inventory, material.item_id) < material.amount;
      });
  return missing == cost.materials.end() ? nullptr : &*missing;
}

void ItemEnchant::Reset() noexcept {
  chosen_.reset();
  ++choice_seq_;
  pending_.reset();
}

void ItemEnchant::TellOpening(std::uint64_t group_id) {
  // L'ouverture prouve que le serveur a réarmé : une fermeture encore due le
  // refermerait.
  farewell_due_ = false;
  open_ = true;
  group_id_ = group_id;
  ++opening_seq_;
  Reset();
}

bool ItemEnchant::Choose(std::uint16_t client_slot) {
  if (!open_ || pending_.has_value()) {
    return false;
  }
  if (chosen_ != client_slot) {
    chosen_ = client_slot;
    ++choice_seq_;
  }
  return true;
}

void ItemEnchant::Forget() noexcept {
  if (chosen_.has_value()) {
    chosen_.reset();
    ++choice_seq_;
  }
}

void ItemEnchant::MarkRequestSent(ItemEnchantRequest request, std::uint8_t target_slot,
                                  std::uint8_t card_slots) noexcept {
  pending_ = Pending{.request = request,
                     .target_slot = target_slot,
                     .card_slots = card_slots,
                     .client_slot = chosen_.value_or(0)};
}

void ItemEnchant::TellResult(std::int32_t message_id, std::uint32_t enchant_item_id) {
  const Pending pending = pending_.value_or(Pending{});
  result_ = Result{.request = pending.request,
                   .target_slot = pending.target_slot,
                   .card_slots = pending.card_slots,
                   .client_slot = pending.client_slot,
                   .message_id = message_id,
                   .enchant_item_id = enchant_item_id};
  ++result_seq_;
  // 🔴 LE SERVEUR A OUBLIÉ LE GROUPE : la séance est close, sans fermeture à dire.
  open_ = false;
  farewell_due_ = false;
  Reset();
}

void ItemEnchant::Close() {
  if (!open_) {
    return;
  }
  farewell_due_ = true;
  open_ = false;
  Reset();
}

void ItemEnchant::CloseSilently() noexcept {
  open_ = false;
  farewell_due_ = false;
  Reset();
}

bool ItemEnchant::TakeFarewell() noexcept { return std::exchange(farewell_due_, false); }

const ItemEnchantCost* ItemEnchantCostOf(const ItemEnchantGroup& group, const InventoryItem& item,
                                         ItemEnchantRequest request, std::uint8_t card_slots) {
  switch (request.mode) {
    case ItemEnchantMode::kRandom:
    case ItemEnchantMode::kPerfect: {
      const std::optional<std::uint8_t> slot = NextEnchantSlot(group, item, card_slots);
      const ItemEnchantSlot* entry = slot.has_value() ? group.FindSlot(*slot) : nullptr;
      if (entry == nullptr) {
        return nullptr;
      }
      if (request.mode == ItemEnchantMode::kRandom) {
        return &entry->cost;
      }
      const ItemEnchantPerfect* perfect = entry->FindPerfect(request.enchant_item_id);
      return perfect == nullptr ? nullptr : &perfect->cost;
    }
    case ItemEnchantMode::kUpgrade: {
      if (request.slot >= kItemCardSlots) {
        return nullptr;
      }
      const ItemEnchantSlot* entry = group.FindSlot(request.slot);
      const ItemEnchantUpgrade* upgrade =
          entry == nullptr ? nullptr : entry->FindUpgrade(item.cards[request.slot]);
      return upgrade == nullptr ? nullptr : &upgrade->cost;
    }
    case ItemEnchantMode::kReset:
      return group.reset.has_value() ? &group.reset->cost : nullptr;
  }
  return nullptr;
}

ItemEnchantCheck CheckItemEnchantRequest(const ItemEnchant& enchant, const ItemEnchantGroup& group,
                                         ItemEnchantRequest request, const Inventory& inventory,
                                         std::uint8_t card_slots, std::int64_t zeny) {
  if (!enchant.open()) {
    return {.refusal = ItemEnchantRefusal::kNotOpen};
  }
  if (enchant.waiting()) {
    return {.refusal = ItemEnchantRefusal::kWaiting};
  }
  if (!enchant.chosen_slot().has_value()) {
    return {.refusal = ItemEnchantRefusal::kNothingChosen};
  }
  const InventoryItem* item = inventory.Find(*enchant.chosen_slot());
  if (item == nullptr) {
    return {.refusal = ItemEnchantRefusal::kPieceGone};
  }
  if (const ItemEnchantRefusal refusal = JudgeItemEnchantPiece(group, *item);
      refusal != ItemEnchantRefusal::kNone) {
    return {.refusal = refusal};
  }

  std::uint8_t target_slot = 0;
  switch (request.mode) {
    case ItemEnchantMode::kRandom:
    case ItemEnchantMode::kPerfect: {
      const std::optional<std::uint8_t> slot = NextEnchantSlot(group, *item, card_slots);
      if (!slot.has_value()) {
        return {.refusal = ItemEnchantRefusal::kNoFreeSlot};
      }
      target_slot = *slot;
      const ItemEnchantSlot* entry = group.FindSlot(target_slot);
      if (entry == nullptr) {
        return {.refusal = ItemEnchantRefusal::kSlotClosed, .target_slot = target_slot};
      }
      if (request.mode == ItemEnchantMode::kRandom) {
        // ⚠ CE REFUS-LÀ PARLE : le serveur répond par un échec, sans rien prendre.
        if (entry->DrawsFor(item->grade) == nullptr) {
          return {.refusal = ItemEnchantRefusal::kNoDrawForGrade, .target_slot = target_slot};
        }
      } else if (entry->FindPerfect(request.enchant_item_id) == nullptr) {
        return {.refusal = ItemEnchantRefusal::kNoSuchEnchant,
                .item_id = request.enchant_item_id,
                .target_slot = target_slot};
      }
      break;
    }
    case ItemEnchantMode::kUpgrade: {
      target_slot = request.slot;
      if (request.slot >= kItemCardSlots || request.slot < card_slots ||
          item->cards[request.slot] == 0) {
        return {.refusal = ItemEnchantRefusal::kNothingToUpgrade, .target_slot = target_slot};
      }
      const ItemEnchantSlot* entry = group.FindSlot(request.slot);
      if (entry == nullptr || entry->FindUpgrade(item->cards[request.slot]) == nullptr) {
        return {.refusal = ItemEnchantRefusal::kNothingToUpgrade, .target_slot = target_slot};
      }
      break;
    }
    case ItemEnchantMode::kReset:
      // ⚠ UNE CHANCE NULLE N'EST PAS UNE REMISE À ZÉRO : le serveur se tait.
      if (!group.reset.has_value() || group.reset->chance == 0 ||
          EnchantedSlots(*item, card_slots).empty()) {
        return {.refusal = ItemEnchantRefusal::kNothingToReset};
      }
      break;
  }

  const ItemEnchantCost* cost = ItemEnchantCostOf(group, *item, request, card_slots);
  if (cost == nullptr) {
    return {.refusal = ItemEnchantRefusal::kSlotClosed, .target_slot = target_slot};
  }
  if (zeny < cost->zeny) {
    return {.refusal = ItemEnchantRefusal::kNotEnoughZeny, .target_slot = target_slot};
  }
  if (const ItemEnchantMaterial* missing = MissingEnchantMaterial(*cost, inventory);
      missing != nullptr) {
    return {.refusal = ItemEnchantRefusal::kMissingMaterial,
            .item_id = missing->item_id,
            .target_slot = target_slot};
  }
  return {.target_slot = target_slot};
}

std::array<std::uint32_t, kItemCardSlots> EnchantedCards(
    const std::array<std::uint32_t, kItemCardSlots>& cards,
    const ItemEnchant::Result& result) noexcept {
  std::array<std::uint32_t, kItemCardSlots> enchanted = cards;
  if (!result.succeeded()) {
    return enchanted;
  }
  if (result.request.mode == ItemEnchantMode::kReset) {
    for (std::size_t slot = result.card_slots; slot < kItemCardSlots; ++slot) {
      enchanted[slot] = 0;
    }
  } else if (result.target_slot < kItemCardSlots && result.enchant_item_id != 0) {
    enchanted[result.target_slot] = result.enchant_item_id;
  }
  return enchanted;
}

}  // namespace ro::sim
