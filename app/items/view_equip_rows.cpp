#include "items/view_equip_rows.h"

#include <utility>

#include "ro/core/i18n.h"
#include "ro/sim/equip_location.h"

namespace moonclient {

using ro::core::Tr;

PiecePlace PlaceOf(std::uint32_t worn_at) {
  PiecePlace found;
  if ((worn_at & ro::sim::kEquipAmmo) != 0) {
    found.section = EquipSection::kOther;
    found.ammo = true;
    return found;
  }
  if (ro::sim::IsShadowLocation(worn_at)) {
    found.section = EquipSection::kOther;
    found.shadow = true;
    return found;
  }
  if (ro::sim::IsCostumeLocation(worn_at)) {
    found.section = EquipSection::kCostume;
    if ((worn_at & ro::sim::kEquipCostumeHeadTop) != 0) {
      found.place = EquipPlace::kHeadTop;
    } else if ((worn_at & ro::sim::kEquipCostumeHeadMid) != 0) {
      found.place = EquipPlace::kHeadMid;
    } else if ((worn_at & ro::sim::kEquipCostumeHeadLow) != 0) {
      found.place = EquipPlace::kHeadLow;
    } else {
      found.place = EquipPlace::kGarment;
    }
    return found;
  }
  found.two_handed = ro::sim::IsTwoHanded(worn_at);
  // L'ordre des tests est celui qui départage une pièce à plusieurs places.
  constexpr std::array<std::pair<std::uint32_t, EquipPlace>, 10> kByPriority = {{
      {ro::sim::kEquipHeadTop, EquipPlace::kHeadTop},
      {ro::sim::kEquipHeadMid, EquipPlace::kHeadMid},
      {ro::sim::kEquipHeadLow, EquipPlace::kHeadLow},
      {ro::sim::kEquipArmor, EquipPlace::kArmor},
      {ro::sim::kEquipWeapon, EquipPlace::kWeapon},
      {ro::sim::kEquipShield, EquipPlace::kShield},
      {ro::sim::kEquipGarment, EquipPlace::kGarment},
      {ro::sim::kEquipShoes, EquipPlace::kShoes},
      {ro::sim::kEquipAccessoryRight, EquipPlace::kAccessoryLeft},
      {ro::sim::kEquipAccessoryLeft, EquipPlace::kAccessoryRight},
  }};
  for (const auto& [bit, place] : kByPriority) {
    if ((worn_at & bit) != 0) {
      found.place = place;
      return found;
    }
  }
  found.section = EquipSection::kOther;
  return found;
}

const char* EquipPlaceLabel(EquipPlace place) {
  switch (place) {
    case EquipPlace::kHeadLow:
      return Tr("Head (bas)");
    case EquipPlace::kWeapon:
      return Tr("Arme");
    case EquipPlace::kGarment:
      return Tr("Cape");
    case EquipPlace::kAccessoryLeft:
      return Tr("Accessoire gauche");
    case EquipPlace::kArmor:
      return Tr("Armure");
    case EquipPlace::kShield:
      return Tr("Bouclier");
    case EquipPlace::kShoes:
      return Tr("Chaussures");
    case EquipPlace::kAccessoryRight:
      return Tr("Accessoire droit");
    case EquipPlace::kHeadTop:
      return Tr("Head (haut)");
    case EquipPlace::kHeadMid:
      return Tr("Head (milieu)");
  }
  return "?";
}

std::optional<std::size_t> RankAt(std::span<const ro::sim::InventoryItem> pieces, EquipPlace place,
                                  EquipSection section) {
  for (std::size_t rank = 0; rank < pieces.size(); ++rank) {
    const PiecePlace where = PlaceOf(pieces[rank].worn_at);
    if (where.section == section && where.place == place) {
      return rank;
    }
  }
  return std::nullopt;
}

const ro::sim::InventoryItem* PieceAt(std::span<const ro::sim::InventoryItem> pieces,
                                      EquipPlace place, EquipSection section) {
  const std::optional<std::size_t> rank = RankAt(pieces, place, section);
  return rank.has_value() ? &pieces[*rank] : nullptr;
}

std::uint32_t LocationOf(EquipPlace place, EquipSection section) {
  // Les costumes n'ont que quatre places : les autres n'y ont pas de bit.
  if (section == EquipSection::kCostume) {
    constexpr std::array<std::pair<EquipPlace, std::uint32_t>, 4> kCostumeBits = {{
        {EquipPlace::kHeadTop, ro::sim::kEquipCostumeHeadTop},
        {EquipPlace::kHeadMid, ro::sim::kEquipCostumeHeadMid},
        {EquipPlace::kHeadLow, ro::sim::kEquipCostumeHeadLow},
        {EquipPlace::kGarment, ro::sim::kEquipCostumeGarment},
    }};
    for (const auto& [costume_place, bit] : kCostumeBits) {
      if (costume_place == place) {
        return bit;
      }
    }
    return 0;
  }
  if (section != EquipSection::kEquipment) {
    return 0;
  }
  switch (place) {
    case EquipPlace::kHeadLow:
      return ro::sim::kEquipHeadLow;
    case EquipPlace::kWeapon:
      return ro::sim::kEquipWeapon;
    case EquipPlace::kGarment:
      return ro::sim::kEquipGarment;
    case EquipPlace::kAccessoryLeft:
      return ro::sim::kEquipAccessoryRight;
    case EquipPlace::kArmor:
      return ro::sim::kEquipArmor;
    case EquipPlace::kShield:
      return ro::sim::kEquipShield;
    case EquipPlace::kShoes:
      return ro::sim::kEquipShoes;
    case EquipPlace::kAccessoryRight:
      return ro::sim::kEquipAccessoryLeft;
    case EquipPlace::kHeadTop:
      return ro::sim::kEquipHeadTop;
    case EquipPlace::kHeadMid:
      return ro::sim::kEquipHeadMid;
  }
  return 0;
}

}  // namespace moonclient
