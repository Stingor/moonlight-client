// LES COMMANDES DU STAFF SUR L'HOMONCULE : les listes de classes, et le texte
// qu'une intention de la fenêtre devient.

#include "game/homunculus_staff_words.h"

#include <cstdint>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/homunculus_tab.h"

using ro::ui::HomunculusStaffOrder;
using ro::ui::HomunculusTabAction;
using ro::ui::HomunculusVerb;

namespace {

constexpr std::uint16_t kLif = 6001;
constexpr std::uint16_t kVanilmirthSecond = 6008;
constexpr std::uint16_t kLifEvolved = 6009;
constexpr std::uint16_t kEira = 6048;
constexpr std::uint16_t kEleanor = 6052;
constexpr std::size_t kCreatableCount = 13;
constexpr std::size_t kMutatedCount = 5;

[[nodiscard]] HomunculusTabAction Staff(HomunculusStaffOrder order, int amount = 0) {
  HomunculusTabAction asked;
  asked.verb = HomunculusVerb::kStaff;
  asked.staff_order = order;
  asked.staff_amount = amount;
  return asked;
}

}  // namespace

TEST_CASE("Staff homoncule : les classes créables et mutables, en « classe nom »",
          "[app][homunculus_staff]") {
  const std::vector<ro::ui::StaffHomunculusChoice> creatable = moonclient::CreatableHomunculi();
  REQUIRE(creatable.size() == kCreatableCount);
  CHECK(creatable.front().job == kLif);
  CHECK(creatable.front().label == "6001 Lif");
  CHECK(creatable[7].job == kVanilmirthSecond);
  CHECK(creatable[8].job == kEira);
  CHECK(creatable[8].label == "6048 Eira");
  CHECK(creatable.back().job == kEleanor);

  const std::vector<ro::ui::StaffHomunculusChoice> mutated = moonclient::MutatedHomunculi();
  REQUIRE(mutated.size() == kMutatedCount);
  CHECK(mutated.front().job == kEira);
  CHECK(mutated.back().job == kEleanor);
}

TEST_CASE("Staff homoncule : forme évoluée, forme mutée, classe inconnue",
          "[app][homunculus_staff]") {
  CHECK(moonclient::HomunculusEvolved(kLif) == false);
  CHECK(moonclient::HomunculusEvolved(kLifEvolved) == true);
  CHECK(moonclient::HomunculusEvolved(kEira) == false);
  CHECK_FALSE(moonclient::HomunculusEvolved(0).has_value());
  CHECK(moonclient::IsMutatedHomunculus(kEleanor));
  CHECK_FALSE(moonclient::IsMutatedHomunculus(kLifEvolved));
}

TEST_CASE("Staff homoncule : chaque intention compose sa commande", "[app][homunculus_staff]") {
  CHECK(moonclient::MakeHomunculusCommand(kEira) == "@makehomun 6048");
  CHECK(moonclient::HomunculusStaffCommand(Staff(HomunculusStaffOrder::kSetHunger, 42)) ==
        "@homhungry 42");
  CHECK(moonclient::HomunculusStaffCommand(Staff(HomunculusStaffOrder::kSetIntimacy, 910)) ==
        "@homfriendly 910");
  CHECK(moonclient::HomunculusStaffCommand(Staff(HomunculusStaffOrder::kAddLevels, 3)) ==
        "@homlevel 3");
  CHECK(moonclient::HomunculusStaffCommand(Staff(HomunculusStaffOrder::kShuffle)) == "@homshuffle");
  CHECK(moonclient::HomunculusStaffCommand(Staff(HomunculusStaffOrder::kStats)) == "@homstats");
  CHECK(moonclient::HomunculusStaffCommand(Staff(HomunculusStaffOrder::kInfo)) == "@hominfo");
  CHECK(moonclient::HomunculusStaffCommand(Staff(HomunculusStaffOrder::kEvolve)) ==
        "@homevolution");

  HomunculusTabAction mutate = Staff(HomunculusStaffOrder::kMutate);
  mutate.staff_job = kEleanor;
  CHECK(moonclient::HomunculusStaffCommand(mutate) == "@hommutate 6052");

  HomunculusTabAction talk = Staff(HomunculusStaffOrder::kTalk);
  talk.staff_text = "Bonjour\nseconde ligne";
  CHECK(moonclient::HomunculusStaffCommand(talk) == "@homtalk Bonjour");
}

TEST_CASE("Staff homoncule : rien à envoyer, rien de composé", "[app][homunculus_staff]") {
  // ⚠ LA COMMANDE DE NIVEAU EST RELATIVE : zéro niveau n'envoie rien.
  CHECK(moonclient::HomunculusStaffCommand(Staff(HomunculusStaffOrder::kAddLevels, 0)).empty());
  // Sans classe, le serveur en tirerait une au sort.
  CHECK(moonclient::HomunculusStaffCommand(Staff(HomunculusStaffOrder::kMutate)).empty());
  CHECK(moonclient::HomunculusStaffCommand(Staff(HomunculusStaffOrder::kTalk)).empty());
  CHECK(moonclient::HomunculusStaffCommand(Staff(HomunculusStaffOrder::kNone)).empty());
  // Un autre verbe que celui du staff ne devient jamais une commande.
  HomunculusTabAction feed = Staff(HomunculusStaffOrder::kShuffle);
  feed.verb = HomunculusVerb::kFeed;
  CHECK(moonclient::HomunculusStaffCommand(feed).empty());
  // Les bornes du serveur, au cas où une intention les passerait.
  CHECK(moonclient::HomunculusStaffCommand(Staff(HomunculusStaffOrder::kSetHunger, 500)) ==
        "@homhungry 100");
}
