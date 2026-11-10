#include "ro/sim/style_outfit.h"

namespace ro::sim {

void StyleOutfitDesk::Hear(OutfitReply reply, std::vector<StyleWardrobeEntry> wardrobe) {
  reply.sequence = ++last_sequence_;
  if (reply.action == OutfitAction::kWardrobeList && reply.accepted()) {
    wardrobe_ = std::move(wardrobe);
    wardrobe_sequence_ = reply.sequence;
  }
  last_ = std::move(reply);
}

const StyleWardrobeEntry* StyleOutfitDesk::Slot(std::uint8_t slot) const {
  for (const StyleWardrobeEntry& entry : wardrobe_) {
    if (entry.slot == slot) {
      return &entry;
    }
  }
  return nullptr;
}

}  // namespace ro::sim
