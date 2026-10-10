#include "ro/sim/equip_compare.h"

#include "ro/sim/equip_location.h"

namespace ro::sim {
namespace {

// Les pièces d'ombre, chacune sur son bit — `EQP_SHADOW_*`, `src/common/mmo.hpp`.
constexpr std::uint32_t kEquipShadowArmor = 0x010000;
constexpr std::uint32_t kEquipShadowWeapon = 0x020000;
constexpr std::uint32_t kEquipShadowShield = 0x040000;
constexpr std::uint32_t kEquipShadowShoes = 0x080000;
constexpr std::uint32_t kEquipShadowAccessoryRight = 0x100000;
constexpr std::uint32_t kEquipShadowAccessoryLeft = 0x200000;

constexpr std::uint32_t kBothAccessories = kEquipAccessoryRight | kEquipAccessoryLeft;

}  // namespace

std::uint32_t ComparedEquipSlot(std::uint32_t wearable_at) noexcept {
  switch (wearable_at) {
    case kEquipHeadLow:
    case kEquipGarment:
    case kEquipArmor:
    case kEquipShield:
    case kEquipShoes:
    case kEquipCostumeHeadLow:
    case kEquipCostumeGarment:
    case kEquipShadowArmor:
    case kEquipShadowWeapon:
    case kEquipShadowShield:
    case kEquipShadowShoes:
    case kEquipShadowAccessoryRight:
    case kEquipShadowAccessoryLeft:
      return wearable_at;
    case kEquipWeapon:
    case kEquipBothHands:
      return kEquipWeapon;
    case kBothAccessories:
      return kEquipAccessoryRight;
    case kEquipHeadTop:
    case kEquipHeadTop | kEquipHeadLow:
    case kEquipHeadTop | kEquipHeadMid:
    case kEquipHeadTop | kEquipHeadMid | kEquipHeadLow:
      return kEquipHeadTop;
    case kEquipHeadMid:
    case kEquipHeadMid | kEquipHeadLow:
      return kEquipHeadMid;
    case kEquipCostumeHeadTop:
    case kEquipCostumeHeadTop | kEquipCostumeHeadMid:
    case kEquipCostumeHeadTop | kEquipCostumeHeadLow:
    case kEquipCostumeHeadTop | kEquipCostumeHeadMid | kEquipCostumeHeadLow:
      return kEquipCostumeHeadTop;
    case kEquipCostumeHeadMid:
    case kEquipCostumeHeadMid | kEquipCostumeHeadLow:
      return kEquipCostumeHeadMid;
    default:
      return 0;
  }
}

const InventoryItem* EquippedToCompare(const Inventory& bag,
                                       const InventoryItem& described) noexcept {
  if (described.equipped()) {
    return nullptr;
  }
  const std::uint32_t slot = ComparedEquipSlot(described.wearable_at);
  if (slot == 0) {
    return nullptr;
  }
  for (const InventoryItem& item : bag.items()) {
    if ((item.worn_at & slot) != 0) {
      return &item;
    }
  }
  return nullptr;
}

}  // namespace ro::sim
