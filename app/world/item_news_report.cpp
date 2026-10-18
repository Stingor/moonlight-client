// Les nouvelles d'objets dans la conversation. Voir `ItemPickupReport::NoticeNews`.

#include <cstdint>
#include <string>

#include "ro/core/i18n.h"
#include "ro/core/printed.h"
#include "ro/sim/world.h"

#include "items/inventory_view.h"
#include "world/item_pickup_report.h"

namespace moonclient {
namespace {

// Les parties qu'une pièce abîmée d'un membre du groupe nomme, par drapeau
// d'emplacement. Les autres emplacements se taisent.
constexpr std::uint16_t kRightHand = 0x0002;
constexpr std::uint16_t kArmor = 0x0010;
constexpr std::uint16_t kLeftHand = 0x0020;
constexpr std::uint16_t kHeadTop = 0x0100;

[[nodiscard]] const char* DamagedPartLabel(std::uint16_t location) {
  switch (location) {
    case kRightHand:
      return ro::core::Tr("Main droite");
    case kArmor:
      return ro::core::Tr("Armure");
    case kLeftHand:
      return ro::core::Tr("Main gauche");
    case kHeadTop:
      return ro::core::Tr("Tête");
    default:
      return nullptr;
  }
}

// La pièce que je porte à cet emplacement, ou rien.
[[nodiscard]] const ro::sim::InventoryItem* WornAt(const ro::sim::Inventory& bag,
                                                   std::uint16_t location) {
  for (const ro::sim::InventoryItem& item : bag.items()) {
    if ((item.worn_at & location) != 0) {
      return &item;
    }
  }
  return nullptr;
}

}  // namespace

void ItemPickupReport::NoticeNews(ro::sim::ItemNews& news, ro::sim::World& world,
                                  const ro::sim::PartyRoster& party, const CreatorNames& creators) {
  for (const ro::sim::PartyPickup& pickup : news.TakePartyPickups()) {
    const ro::sim::PartyMember* member = party.Find(pickup.account_id);
    if (pickup.account_id == world.player_id() || member == nullptr || items_ == nullptr) {
      continue;
    }
    const std::string name = ReportedItemName(pickup.item, creators, *items_, *words_);
    world.RecordChat(
        ro::sim::ChatKind::kServer, ro::sim::ChatType::kPartyItem, 0,
        ro::core::Printed(ro::core::Tr("%s a obtenu %s."), member->name.c_str(), name.c_str()),
        pickup.heard_ms, kPickupGainColor);
  }
  for (const ro::sim::EquipDamage& damage : news.TakeEquipDamages()) {
    if (damage.location == 0) {
      continue;
    }
    std::string line;
    if (damage.account_id == world.player_id()) {
      const ro::sim::InventoryItem* worn = WornAt(world.inventory(), damage.location);
      if (worn == nullptr || items_ == nullptr) {
        continue;
      }
      const std::string name = ReportedItemName(*worn, creators, *items_, *words_);
      line = ro::core::Printed(ro::core::Tr("L'équipement %s a été endommagé."), name.c_str());
    } else {
      const ro::sim::PartyMember* member = party.Find(damage.account_id);
      const char* part = DamagedPartLabel(damage.location);
      if (member == nullptr || part == nullptr) {
        continue;
      }
      line = ro::core::Printed(ro::core::Tr("%s : équipement endommagé (%s)."),
                               member->name.c_str(), part);
    }
    world.RecordChat(ro::sim::ChatKind::kServer, ro::sim::ChatType::kDamagedEquipment, 0, line,
                     damage.heard_ms, kPickupRefusalColor);
  }
}

}  // namespace moonclient
