#include "ro/sim/storage_facts.h"

#include <algorithm>

namespace ro::sim {

void StorageFacts::Tell(std::vector<StorageItemFacts> told) {
  facts_ = std::move(told);
  std::ranges::sort(facts_, {}, &StorageItemFacts::item_id);
  // ⚠ LE MÊME IDENTIFIANT REVIENT : le serveur envoie une entrée par PLACE du
  // coffre, et deux piles du même objet en font deux. Elles portent la même
  // chose — on n'en garde qu'une, sans quoi la recherche binaire rendrait
  // l'une ou l'autre selon l'humeur du tri.
  const auto doubles = std::ranges::unique(facts_, {}, &StorageItemFacts::item_id);
  facts_.erase(doubles.begin(), doubles.end());
}

const StorageItemFacts* StorageFacts::Of(std::uint32_t item_id) const {
  const auto found = std::ranges::lower_bound(facts_, item_id, {}, &StorageItemFacts::item_id);
  if (found == facts_.end() || found->item_id != item_id) {
    return nullptr;
  }
  return &*found;
}

}  // namespace ro::sim
