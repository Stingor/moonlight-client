#include "ro/sim/adoption.h"

#include <algorithm>

namespace ro::sim {

bool WearsWeddingRing(const Inventory& bag) {
  return std::ranges::any_of(bag.items(), [](const InventoryItem& item) {
    return item.equipped() &&
           (item.item_id == kGroomWeddingRingItemId || item.item_id == kBrideWeddingRingItemId);
  });
}

bool AdoptionLooksPossible(const AdoptionFacts& facts) noexcept {
  return facts.own_base_level >= kAdoptionMinBaseLevel && facts.wears_wedding_ring &&
         !facts.has_child && facts.target_in_party && AdoptableJob(facts.target_job_id);
}

AdoptionFacts AdoptionFactsOf(std::int64_t own_base_level, const Inventory& bag,
                              const SkillBook& skills, bool target_in_party,
                              std::uint16_t target_job_id) {
  return AdoptionFacts{.own_base_level = own_base_level,
                       .wears_wedding_ring = WearsWeddingRing(bag),
                       .has_child = skills.Find(kCallBabySkillId) != nullptr,
                       .target_in_party = target_in_party,
                       .target_job_id = target_job_id};
}

}  // namespace ro::sim
