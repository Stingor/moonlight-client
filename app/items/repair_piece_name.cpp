#include "items/repair_piece_name.h"

#include <cstdint>

#include "ro/vfs/item_name.h"

namespace moonclient {
namespace {

// Le type que le client donne à une arme et à une armure : le serveur les
// échange avant de les écrire.
constexpr std::int32_t kWeaponItemType = 4;
constexpr std::int32_t kArmorItemType = 5;

}  // namespace

std::string RepairPieceName(const ro::sim::RepairableItem& item, int weapon_level,
                            const ro::vfs::ItemTable& items, const ro::vfs::ItemWords& words,
                            const CreatorNames& creators) {
  ro::vfs::ItemNameFacts facts;
  facts.item_id = item.item_id;
  facts.type = weapon_level > 0 ? kWeaponItemType : kArmorItemType;
  facts.refine_level = item.refine;
  // ⚠ PAS LE GRADE : le serveur ne l'écrit pas dans cette liste, l'octet y est
  // ce que son tampon contenait. Le lire peint « [] » devant le nom.
  facts.cards = item.cards;
  creators.Fill(facts);
  return ro::vfs::ItemNameText(ro::vfs::ComposeItemName(facts, items, words));
}

}  // namespace moonclient
