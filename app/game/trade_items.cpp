#include "game/trade_items.h"

#include "ro/vfs/item_name.h"

#include "items/inventory_view.h"

namespace moonclient {

void TradeItems::See(const ro::vfs::ItemTable* items, ro::render::ItemIcons* icons,
                     const ro::vfs::ItemWords* words) noexcept {
  items_ = items;
  icons_ = icons;
  words_ = words;
}

ro::ui::SkinImage TradeItems::IconOf(std::uint32_t item_id) const {
  if (icons_ == nullptr) {
    return {};
  }
  const ro::render::IconTexture& icon = icons_->Get(item_id);
  return ro::ui::SkinImage{
      .id = icon.usable() ? static_cast<ImTextureID>(icon.texture.name()) : ImTextureID{0},
      .width = static_cast<float>(icon.width_px),
      .height = static_cast<float>(icon.height_px)};
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
