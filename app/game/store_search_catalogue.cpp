// Le catalogue de la recherche d'échoppes. Voir l'en-tête.

#include "game/store_search_catalogue.h"

#include <algorithm>

namespace moonclient {
namespace {

// Une carte : un mot de carte, une illustration, ou la liste des cartes.
[[nodiscard]] bool IsCardItem(std::uint32_t item_id, const ro::vfs::ItemWords& words,
                              const StoreSearchCardTables& cards) {
  return words.FindCard(item_id) != nullptr ||
         (cards.illustrations != nullptr && !cards.illustrations->Find(item_id).empty()) ||
         (cards.listed != nullptr && cards.listed->Contains(item_id));
}

}  // namespace

std::vector<ro::sim::StoreSearchCatalogueEntry> StoreSearchCatalogueOf(
    const ro::vfs::ItemTable& table, const ro::vfs::ItemWords& words,
    const StoreSearchCardTables& cards) {
  std::vector<ro::sim::StoreSearchCatalogueEntry> catalogue;
  catalogue.reserve(table.size());
  for (const auto& [item_id, entry] : table.entries()) {
    ro::sim::StoreSearchCatalogueEntry line{.item_id = item_id, .name = entry.name};
    line.card = IsCardItem(item_id, words, cards);
    if (const ro::vfs::CardWord* card = words.FindCard(item_id); card != nullptr) {
      line.card_word = card->text;
    }
    catalogue.push_back(line);
  }
  std::ranges::sort(catalogue, {}, &ro::sim::StoreSearchCatalogueEntry::item_id);
  return catalogue;
}

std::vector<std::uint32_t> CardItemIdsOf(const ro::vfs::ItemTable& table,
                                         const ro::vfs::ItemWords& words,
                                         const StoreSearchCardTables& cards) {
  std::vector<std::uint32_t> card_ids;
  for (const auto& [item_id, entry] : table.entries()) {
    (void)entry;
    if (IsCardItem(item_id, words, cards)) {
      card_ids.push_back(item_id);
    }
  }
  return card_ids;
}

}  // namespace moonclient
