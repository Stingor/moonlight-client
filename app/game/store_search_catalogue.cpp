// Le catalogue de la recherche d'échoppes. Voir l'en-tête.

#include "game/store_search_catalogue.h"

#include <algorithm>

namespace moonclient {

std::vector<ro::sim::StoreSearchCatalogueEntry> StoreSearchCatalogueOf(
    const ro::vfs::ItemTable& table, const ro::vfs::ItemWords& words,
    const StoreSearchCardTables& cards) {
  std::vector<ro::sim::StoreSearchCatalogueEntry> catalogue;
  catalogue.reserve(table.size());
  for (const auto& [item_id, entry] : table.entries()) {
    ro::sim::StoreSearchCatalogueEntry line{.item_id = item_id, .name = entry.name};
    if (const ro::vfs::CardWord* card = words.FindCard(item_id); card != nullptr) {
      line.card = true;
      line.card_word = card->text;
    }
    if (cards.illustrations != nullptr && !cards.illustrations->Find(item_id).empty()) {
      line.card = true;
    }
    if (cards.listed != nullptr && cards.listed->Contains(item_id)) {
      line.card = true;
    }
    catalogue.push_back(line);
  }
  std::ranges::sort(catalogue, {}, &ro::sim::StoreSearchCatalogueEntry::item_id);
  return catalogue;
}

}  // namespace moonclient
