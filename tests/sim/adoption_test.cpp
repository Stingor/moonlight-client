// L'ADOPTION VUE DU CLIENT : ce qu'il sait juger avant de demander, et ce que
// le serveur lui en dit.

#include "ro/sim/adoption.h"

#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/inventory.h"
#include "ro/sim/skill_book.h"

using ro::sim::AdoptableJob;
using ro::sim::AdoptionDesk;
using ro::sim::AdoptionFacts;
using ro::sim::AdoptionInvitation;
using ro::sim::AdoptionLooksPossible;
using ro::sim::AdoptionRefusal;

namespace {

constexpr std::uint16_t kSwordmanJob = 1;
constexpr std::uint16_t kKnightJob = 7;
constexpr std::uint16_t kBabyNoviceJob = 4023;
// Un emplacement d'accessoire, tel que le serveur le code.
constexpr std::uint32_t kAccessorySlot = 0x0008;
constexpr std::uint32_t kClip = 2607;

[[nodiscard]] AdoptionFacts Eligible() {
  return AdoptionFacts{.own_base_level = ro::sim::kAdoptionMinBaseLevel,
                       .wears_wedding_ring = true,
                       .has_child = false,
                       .target_in_party = true,
                       .target_job_id = ro::sim::kSuperNoviceJobId};
}

[[nodiscard]] ro::sim::InventoryItem Item(std::uint16_t slot, std::uint32_t item_id,
                                          std::uint32_t worn_at) {
  ro::sim::InventoryItem item;
  item.client_slot = slot;
  item.item_id = item_id;
  item.worn_at = worn_at;
  return item;
}

}  // namespace

TEST_CASE("Adoption : seules les classes de départ s'adoptent", "[sim][adoption]") {
  CHECK(AdoptableJob(0));
  CHECK(AdoptableJob(kSwordmanJob));
  CHECK(AdoptableJob(ro::sim::kThiefJobId));
  CHECK(AdoptableJob(ro::sim::kSuperNoviceJobId));
  CHECK(AdoptableJob(ro::sim::kExpandedSuperNoviceJobId));
  CHECK_FALSE(AdoptableJob(kKnightJob));
  // 🔴 UN BÉBÉ EST DÉJÀ ADOPTÉ : sa classe le dit.
  CHECK_FALSE(AdoptableJob(kBabyNoviceJob));
}

TEST_CASE("Adoption : chaque condition connue manque à son tour", "[sim][adoption]") {
  CHECK(AdoptionLooksPossible(Eligible()));

  AdoptionFacts facts = Eligible();
  facts.own_base_level = ro::sim::kAdoptionMinBaseLevel - 1;
  CHECK_FALSE(AdoptionLooksPossible(facts));
  // Un niveau que le serveur n'a pas encore dit ne permet rien.
  facts.own_base_level = ro::sim::kUnknownStat;
  CHECK_FALSE(AdoptionLooksPossible(facts));

  facts = Eligible();
  facts.wears_wedding_ring = false;
  CHECK_FALSE(AdoptionLooksPossible(facts));

  facts = Eligible();
  facts.has_child = true;
  CHECK_FALSE(AdoptionLooksPossible(facts));

  facts = Eligible();
  facts.target_in_party = false;
  CHECK_FALSE(AdoptionLooksPossible(facts));

  facts = Eligible();
  facts.target_job_id = kKnightJob;
  CHECK_FALSE(AdoptionLooksPossible(facts));
}

TEST_CASE("Adoption : l'alliance compte portée, pas rangée dans le sac", "[sim][adoption]") {
  ro::sim::Inventory bag;
  bag.Put(Item(2, ro::sim::kBrideWeddingRingItemId, 0));
  bag.Put(Item(3, kClip, kAccessorySlot));
  CHECK_FALSE(ro::sim::WearsWeddingRing(bag));
  bag.Put(Item(4, ro::sim::kGroomWeddingRingItemId, kAccessorySlot));
  CHECK(ro::sim::WearsWeddingRing(bag));
}

TEST_CASE("Adoption : l'enfant se reconnaît à la compétence qu'il donne", "[sim][adoption]") {
  ro::sim::Inventory bag;
  bag.Put(Item(2, ro::sim::kGroomWeddingRingItemId, kAccessorySlot));
  ro::sim::SkillBook skills;
  const AdoptionFacts childless =
      ro::sim::AdoptionFactsOf(ro::sim::kAdoptionMinBaseLevel, bag, skills, true, kSwordmanJob);
  CHECK(childless.wears_wedding_ring);
  CHECK_FALSE(childless.has_child);
  CHECK(AdoptionLooksPossible(childless));

  ro::sim::KnownSkill call_baby;
  call_baby.id = ro::sim::kCallBabySkillId;
  call_baby.level = 1;
  skills.Learn(call_baby);
  CHECK_FALSE(AdoptionLooksPossible(
      ro::sim::AdoptionFactsOf(ro::sim::kAdoptionMinBaseLevel, bag, skills, true, kSwordmanJob)));
}

TEST_CASE("Adoption : la dernière demande reçue remplace la précédente", "[sim][adoption]") {
  AdoptionDesk desk;
  CHECK_FALSE(desk.invitation().has_value());
  CHECK(desk.invitation_seq() == 0);
  desk.TellInvitation(AdoptionInvitation{.parent_account_id = 1, .spouse_account_id = 2});
  desk.TellInvitation(
      AdoptionInvitation{.parent_account_id = 3, .spouse_account_id = 4, .parent_name = "Ana"});
  REQUIRE(desk.invitation().has_value());
  CHECK(desk.invitation()->parent_account_id == 3);
  CHECK(desk.invitation_seq() == 2);
  desk.ForgetInvitation();
  CHECK_FALSE(desk.invitation().has_value());

  desk.TellRefusal(AdoptionRefusal::kParentLevel);
  CHECK(desk.TakeRefusals() == std::vector<AdoptionRefusal>{AdoptionRefusal::kParentLevel});
  CHECK(desk.TakeRefusals().empty());
}
