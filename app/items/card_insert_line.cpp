#include "items/card_insert_line.h"

#include <algorithm>
#include <cstddef>

#include "ro/core/i18n.h"
#include "ro/core/printed.h"

namespace moonclient {
namespace {

// La phrase générale, pour un accusé qu'on ne sait pas nommer.
[[nodiscard]] std::string GeneralLine(const ro::vfs::ItemWords& words, bool inserted) {
  return std::string(ro::vfs::CardInsertText(words, inserted));
}

}  // namespace

int CardInsertSlotRank(const ro::sim::InventoryItem& equip_before, int total_slots) {
  if (ro::sim::ItemIsForged(equip_before) || total_slots <= 0) {
    return 0;
  }
  const std::size_t slots =
      std::min(static_cast<std::size_t>(total_slots), equip_before.cards.size());
  for (std::size_t slot = 0; slot < slots; ++slot) {
    if (equip_before.cards[slot] == 0) {
      return static_cast<int>(slot) + 1;
    }
  }
  return 0;
}

std::string CardInsertLine(const ro::sim::CardInsertReport& report, const ro::vfs::ItemTable* items,
                           const ro::vfs::ItemWords& words, const ItemTagOf& tag_of) {
  const bool inserted = report.verdict == ro::sim::CardInsertVerdict::kInserted;
  if (!report.matched || !tag_of) {
    return GeneralLine(words, inserted);
  }
  const std::string card = tag_of(report.card);
  const std::string equip = tag_of(report.equip);
  if (card.empty() || equip.empty()) {
    return GeneralLine(words, inserted);
  }
  if (!inserted) {
    return ro::core::Printed(ro::core::Tr("%s n'a pas pu être sertie dans %s."), card.c_str(),
                             equip.c_str());
  }
  const ro::vfs::ItemEntry* entry = items != nullptr ? items->Find(report.equip.item_id) : nullptr;
  const int total = entry != nullptr ? entry->card_slots : 0;
  const int rank = CardInsertSlotRank(report.equip, total);
  if (rank == 0) {
    return GeneralLine(words, inserted);
  }
  return ro::core::Printed(ro::core::Tr("%s sertie dans %s (emplacement %d/%d)."), card.c_str(),
                           equip.c_str(), rank, total);
}

}  // namespace moonclient
