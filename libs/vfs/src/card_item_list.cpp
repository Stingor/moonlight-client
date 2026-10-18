#include "ro/vfs/card_item_list.h"

#include "ro/core/text.h"

#include "text_table.h"

namespace ro::vfs {

core::Result<CardItemList> ReadCardItemList(Vfs& vfs) {
  CardItemList cards;
  auto bytes = ReadOptionalFile(vfs, kCardItemListPath);
  if (!bytes) {
    return std::unexpected(bytes.error());
  }
  for (const std::string_view line : SplitLines(core::AsText(*bytes))) {
    std::uint32_t item_id = 0;
    // Le champ qui suit est toujours vide ; seule la tête compte.
    std::string_view unused;
    if (!SplitTableRow(line, item_id, unused)) {
      continue;
    }
    cards.item_ids.insert(item_id);
  }
  return cards;
}

}  // namespace ro::vfs
