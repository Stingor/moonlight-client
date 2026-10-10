#include "items/creator_names.h"

#include "ro/sim/inventory.h"

namespace moonclient {

bool CreatorNames::Fill(ro::vfs::ItemNameFacts& facts) const {
  static_assert(ro::sim::kItemCardSlots == ro::vfs::kItemNameCardSlots,
                "le fil porte quatre cartes des deux côtés de la frontière");
  const std::uint32_t creator = ro::sim::CreatorIdOfCards(facts.cards);
  if (creator == 0 || asker_ == nullptr) {
    return false;
  }
  if (const std::string* known = asker_->facts().CreatorOf(creator); known != nullptr) {
    facts.creator_name = *known;
    return false;
  }
  asker_->WantCreator(creator);
  return true;
}

bool AwaitsCreator(const ro::sim::Inventory& bag, std::uint64_t seen_seq,
                   const CreatorNames& creators, std::uint32_t now_ms) {
  for (const ro::sim::ObtainedItem& entry : bag.obtained()) {
    if (entry.seq <= seen_seq) {
      continue;
    }
    ro::vfs::ItemNameFacts facts;
    facts.cards = entry.picked.cards;
    if (creators.Fill(facts) && now_ms - entry.obtained_ms < kItemFactsAnswerWaitMs) {
      return true;
    }
  }
  return false;
}

}  // namespace moonclient
