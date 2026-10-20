#include "ro/sim/storage_facts.h"

#include <algorithm>
#include <iterator>

namespace ro::sim {

void StorageFacts::Tell(std::vector<StorageItemFacts> told, StorageFactsPart part) {
  if (part == StorageFactsPart::kStartsList) {
    facts_.clear();
  }
  // ⚠ UN TRI STABLE, puis une fusion qui place la table AVANT la trame : à
  // identifiant égal, c'est l'entrée reçue la première qui précède, et c'est
  // elle que le dédoublonnage garde.
  std::ranges::stable_sort(told, {}, &StorageItemFacts::item_id);
  std::vector<StorageItemFacts> merged;
  merged.reserve(facts_.size() + told.size());
  std::ranges::merge(facts_, told, std::back_inserter(merged), {}, &StorageItemFacts::item_id,
                     &StorageItemFacts::item_id);
  // ⚠ LE MÊME IDENTIFIANT REVIENT : le serveur envoie une entrée par PLACE du
  // coffre, et deux piles du même objet en font deux. Elles portent la même
  // chose — on n'en garde qu'une, sans quoi la recherche binaire rendrait
  // l'une ou l'autre selon l'humeur du tri.
  const auto doubles = std::ranges::unique(merged, {}, &StorageItemFacts::item_id);
  merged.erase(doubles.begin(), doubles.end());
  facts_ = std::move(merged);
  ++generation_;
}

const StorageItemFacts* StorageFacts::Of(std::uint32_t item_id) const {
  const auto found = std::ranges::lower_bound(facts_, item_id, {}, &StorageItemFacts::item_id);
  if (found == facts_.end() || found->item_id != item_id) {
    return nullptr;
  }
  return &*found;
}

}  // namespace ro::sim
