// Les nouvelles d'objets à dire. Voir l'en-tête.

#include "ro/sim/item_news.h"

#include <utility>

namespace ro::sim {
namespace {

template <typename Fact>
void Keep(std::vector<Fact>& queue, Fact fact) {
  if (queue.size() >= kItemNewsKept) {
    queue.erase(queue.begin());
  }
  queue.push_back(std::move(fact));
}

}  // namespace

void ItemNews::NotePartyPickup(PartyPickup pickup) { Keep(pickups_, std::move(pickup)); }

void ItemNews::NoteEquipDamage(EquipDamage damage) { Keep(damages_, damage); }

void ItemNews::NoteRareObtain(RareItemObtain obtained) { Keep(rare_obtains_, std::move(obtained)); }

void ItemNews::NoteRefine(RefineAnnounce refined) { Keep(refines_, std::move(refined)); }

void ItemNews::NoteGrade(GradeAnnounce graded) { Keep(grades_, std::move(graded)); }

std::vector<RareItemObtain> ItemNews::TakeRareObtains() { return std::exchange(rare_obtains_, {}); }

std::vector<RefineAnnounce> ItemNews::TakeRefines() { return std::exchange(refines_, {}); }

std::vector<GradeAnnounce> ItemNews::TakeGrades() { return std::exchange(grades_, {}); }

std::vector<PartyPickup> ItemNews::TakePartyPickups() { return std::exchange(pickups_, {}); }

std::vector<EquipDamage> ItemNews::TakeEquipDamages() { return std::exchange(damages_, {}); }

}  // namespace ro::sim
