#include "character/equip_slot_map.h"

#include <cstddef>

#include "ro/sim/equip_location.h"

namespace moonclient {

using ro::ui::EquipSlot;

std::uint32_t EquipLocationOf(EquipSlot slot, bool costume) {
  if (costume) {
    // QUATRE PLACES ET PAS UNE DE PLUS. Les mains, l'armure, les chaussures et
    // les accessoires n'ont pas d'apparat : rendre leur bit ordinaire y
    // montrerait la vraie pièce, comme si le costume la reprenait.
    switch (slot) {
      case EquipSlot::kHeadTop:
        return ro::sim::kEquipCostumeHeadTop;
      case EquipSlot::kHeadMid:
        return ro::sim::kEquipCostumeHeadMid;
      case EquipSlot::kHeadLow:
        return ro::sim::kEquipCostumeHeadLow;
      case EquipSlot::kGarment:
        return ro::sim::kEquipCostumeGarment;
      default:
        return 0;
    }
  }
  switch (slot) {
    case EquipSlot::kHeadTop:
      return ro::sim::kEquipHeadTop;
    case EquipSlot::kHeadMid:
      return ro::sim::kEquipHeadMid;
    case EquipSlot::kHeadLow:
      return ro::sim::kEquipHeadLow;
    case EquipSlot::kArmor:
      return ro::sim::kEquipArmor;
    case EquipSlot::kGarment:
      return ro::sim::kEquipGarment;
    case EquipSlot::kShoes:
      return ro::sim::kEquipShoes;
    case EquipSlot::kWeapon:
      return ro::sim::kEquipWeapon;
    case EquipSlot::kShield:
      return ro::sim::kEquipShield;
    case EquipSlot::kAccessoryLeft:
      return ro::sim::kEquipAccessoryLeft;
    case EquipSlot::kAccessoryRight:
      return ro::sim::kEquipAccessoryRight;
    case EquipSlot::kAmmo:
      return ro::sim::kEquipAmmo;
  }
  return 0;
}

bool ShownOnDoll(std::uint32_t worn_at) {
  if (worn_at == 0) {
    return false;
  }
  // LES DEUX ONGLETS RÉUNIS : l'équipement et le costume. Une pièce d'apparat se
  // voit sur le second, et n'interroger que le premier la ferait disparaître du
  // sac sans qu'aucune case ne la montre.
  for (std::size_t rank = 0; rank < ro::ui::kEquipSlotCount; ++rank) {
    const auto slot = static_cast<EquipSlot>(rank);
    if ((worn_at & EquipLocationOf(slot, /*costume=*/false)) != 0) {
      return true;
    }
    if ((worn_at & EquipLocationOf(slot, /*costume=*/true)) != 0) {
      return true;
    }
  }
  return false;
}

bool ShownInBag(std::uint32_t worn_at) {
  return !ShownOnDoll(worn_at) || (worn_at & ro::sim::kEquipAmmo) != 0;
}

std::uint32_t WearAt(std::uint32_t wearable_at, bool alternate) {
  if (alternate &&
      (wearable_at == ro::sim::kEquipWeapon || wearable_at == ro::sim::kEquipBothHands)) {
    return ro::sim::kEquipShield;
  }
  return wearable_at;
}

WornViews WornPieceViews(const ro::sim::Inventory& bag) {
  const ro::sim::InventoryItem* top = bag.WornAt(ro::sim::kEquipHeadTop);
  const ro::sim::InventoryItem* mid = bag.WornAt(ro::sim::kEquipHeadMid);
  const ro::sim::InventoryItem* low = bag.WornAt(ro::sim::kEquipHeadLow);
  const auto view_of = [](const ro::sim::InventoryItem* piece) -> std::uint16_t {
    return piece == nullptr ? 0 : piece->view_id;
  };
  WornViews worn;
  worn.headgears = {(low != top && low != mid) ? view_of(low) : std::uint16_t{0},
                    mid != top ? view_of(mid) : std::uint16_t{0}, view_of(top)};
  worn.robe = view_of(bag.WornAt(ro::sim::kEquipGarment));
  return worn;
}

}  // namespace moonclient
