// L'EMPLACEMENT D'UN ITEM DE LA BOUTIQUE DE VOTE, lu dans son masque
// d'équipement. Voir `CashShopSlotOf`.

#include <cstdint>

#include "ro/core/i18n.h"
#include "ro/ui/cash_shop_window.h"

namespace ro::ui {
namespace {

// L'ORDRE DES EMPLACEMENTS DANS LE FILTRE : les coiffes, le corps, les
// accessoires, les armes, puis les costumes — qui passent pourtant AVANT à la
// lecture d'un masque, ce catalogue en étant fait.
enum SlotKey : int {
  kSlotHeadTop = 0,
  kSlotHeadMid = 1,
  kSlotHeadLow = 2,
  kSlotArmor = 3,
  kSlotGarment = 4,
  kSlotShoes = 5,
  kSlotAccessory = 6,
  kSlotShield = 7,
  kSlotWeapon = 8,
  kSlotAmmo = 9,
  kSlotCostumeHead = 10,
  kSlotCostumeGarment = 13,
};

// Les masques d'équipement du serveur.
constexpr std::uint32_t kEquipHeadLow = 0x0001;
constexpr std::uint32_t kEquipHandRight = 0x0002;
constexpr std::uint32_t kEquipGarment = 0x0004;
constexpr std::uint32_t kEquipAccessoryLeft = 0x0008;
constexpr std::uint32_t kEquipArmor = 0x0010;
constexpr std::uint32_t kEquipHandLeft = 0x0020;
constexpr std::uint32_t kEquipShoes = 0x0040;
constexpr std::uint32_t kEquipAccessoryRight = 0x0080;
constexpr std::uint32_t kEquipHeadTop = 0x0100;
constexpr std::uint32_t kEquipHeadMid = 0x0200;
constexpr std::uint32_t kEquipCostumeHeadTop = 0x0400;
constexpr std::uint32_t kEquipCostumeHeadMid = 0x0800;
constexpr std::uint32_t kEquipCostumeHeadLow = 0x1000;
constexpr std::uint32_t kEquipCostumeGarment = 0x2000;
constexpr std::uint32_t kEquipAmmo = 0x8000;

}  // namespace

CashShopSlot CashShopSlotOf(std::uint32_t equip_location) {
  if ((equip_location & kEquipCostumeGarment) != 0) {
    return {kSlotCostumeGarment, core::Tr("Costume garment")};
  }
  if ((equip_location & (kEquipCostumeHeadTop | kEquipCostumeHeadMid | kEquipCostumeHeadLow)) !=
      0) {
    return {kSlotCostumeHead, core::Tr("Costume head")};
  }
  if ((equip_location & kEquipHeadTop) != 0) {
    return {kSlotHeadTop, core::Tr("Head top")};
  }
  if ((equip_location & kEquipHeadMid) != 0) {
    return {kSlotHeadMid, core::Tr("Head mid")};
  }
  if ((equip_location & kEquipHeadLow) != 0) {
    return {kSlotHeadLow, core::Tr("Head bot")};
  }
  if ((equip_location & kEquipArmor) != 0) {
    return {kSlotArmor, core::Tr("Armor")};
  }
  if ((equip_location & kEquipGarment) != 0) {
    return {kSlotGarment, core::Tr("Garment")};
  }
  if ((equip_location & kEquipShoes) != 0) {
    return {kSlotShoes, core::Tr("Shoes")};
  }
  if ((equip_location & (kEquipAccessoryLeft | kEquipAccessoryRight)) != 0) {
    return {kSlotAccessory, core::Tr("Accessory")};
  }
  if ((equip_location & kEquipHandLeft) != 0) {
    return {kSlotShield, core::Tr("Shield")};
  }
  if ((equip_location & kEquipHandRight) != 0) {
    return {kSlotWeapon, core::Tr("Weapon")};
  }
  if ((equip_location & kEquipAmmo) != 0) {
    return {kSlotAmmo, core::Tr("Ammunition")};
  }
  return {kCashShopSlotOther, core::Tr("Autre")};
}

}  // namespace ro::ui
