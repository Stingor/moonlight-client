#include "game/trade_items.h"

#include "ro/vfs/item_name.h"

#include "game/skin_image_of.h"
#include "game/store_search_grade.h"
#include "items/inventory_view.h"

namespace moonclient {

void TradeItems::See(const ro::vfs::ItemTable* items, ro::render::ItemIcons* icons,
                     const ro::vfs::ItemWords* words) noexcept {
  items_ = items;
  icons_ = icons;
  words_ = words;
}

ro::ui::SkinImage TradeItems::IconOf(std::uint32_t item_id) const {
  return ItemIconImage(icons_, item_id);
}

ro::ui::SkinImage GradeBadgeOf(std::uint8_t grade, ro::render::GameImages* images) {
  const std::string path = StoreSearchGradeBadgePath(grade);
  if (path.empty()) {
    return {};
  }
  return SkinImageAt(images, path);
}

std::string TradeItems::NameOf(std::uint32_t item_id) const {
  if (items_ == nullptr || words_ == nullptr) {
    return "#" + std::to_string(item_id);
  }
  ro::vfs::ItemNameFacts facts;
  facts.item_id = item_id;
  return ro::vfs::ItemNameText(ro::vfs::ComposeItemName(facts, *items_, *words_));
}

std::string TradeItems::NameOf(const ro::sim::InventoryItem& item,
                               const CreatorNames& creators) const {
  if (items_ == nullptr || words_ == nullptr) {
    return "#" + std::to_string(item.item_id);
  }
  ro::vfs::ItemNameFacts facts = ItemNameFactsOf(item);
  creators.Fill(facts);
  return ro::vfs::ItemNameText(ro::vfs::ComposeItemName(facts, *items_, *words_));
}

int TradeItems::CardSlotsOf(std::uint32_t item_id) const {
  if (items_ == nullptr) {
    return 0;
  }
  const ro::vfs::ItemEntry* entry = items_->Find(item_id);
  return entry == nullptr ? 0 : entry->card_slots;
}

}  // namespace moonclient
