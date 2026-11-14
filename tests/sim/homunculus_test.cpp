// L'HOMONCULE DANS LE MONDE : paliers d'intimité, vitesse d'attaque, alerte de
// faim, compétences, et ce qui le fait apparaître ou disparaître.

#include "ro/sim/homunculus.h"

#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/skill_book.h"

using ro::sim::GradeOfIntimacy;
using ro::sim::Homunculus;
using ro::sim::HomunculusIntimacyGrade;
using ro::sim::HomunculusSheet;
using ro::sim::HomunculusSkill;

namespace {

[[nodiscard]] HomunculusSheet Hungry(int hunger) {
  return HomunculusSheet{.alive = true, .hunger = hunger};
}

}  // namespace

TEST_CASE("Homoncule : les paliers d'intimité suivent les bornes du serveur", "[sim][homunculus]") {
  CHECK(GradeOfIntimacy(0) == HomunculusIntimacyGrade::kHateWithPassion);
  CHECK(GradeOfIntimacy(3) == HomunculusIntimacyGrade::kHateWithPassion);
  CHECK(GradeOfIntimacy(4) == HomunculusIntimacyGrade::kHate);
  CHECK(GradeOfIntimacy(10) == HomunculusIntimacyGrade::kHate);
  CHECK(GradeOfIntimacy(11) == HomunculusIntimacyGrade::kAwkward);
  CHECK(GradeOfIntimacy(100) == HomunculusIntimacyGrade::kAwkward);
  CHECK(GradeOfIntimacy(246) == HomunculusIntimacyGrade::kShy);
  CHECK(GradeOfIntimacy(251) == HomunculusIntimacyGrade::kNeutral);
  CHECK(GradeOfIntimacy(750) == HomunculusIntimacyGrade::kNeutral);
  CHECK(GradeOfIntimacy(910) == HomunculusIntimacyGrade::kCordial);
  CHECK(GradeOfIntimacy(1000) == HomunculusIntimacyGrade::kLoyal);
  CHECK(GradeOfIntimacy(1001) == HomunculusIntimacyGrade::kUnknown);
}

TEST_CASE("Homoncule : la vitesse d'attaque se déduit du délai", "[sim][homunculus]") {
  CHECK(ro::sim::AspdFromAmotion(635) == 136);
  CHECK(ro::sim::AspdFromAmotion(0) == 200);
}

TEST_CASE("Homoncule : les compétences d'homoncule vont de 8001 à 8043", "[sim][homunculus]") {
  CHECK_FALSE(ro::sim::IsHomunculusSkill(8000));
  CHECK(ro::sim::IsHomunculusSkill(8001));
  CHECK(ro::sim::IsHomunculusSkill(8043));
  CHECK_FALSE(ro::sim::IsHomunculusSkill(8044));
}

TEST_CASE("Homoncule : il n'est là qu'une fois accusé", "[sim][homunculus]") {
  Homunculus homunculus;
  CHECK_FALSE(homunculus.present());
  static_cast<void>(homunculus.TakeSheet(Hungry(50)));
  CHECK_FALSE(homunculus.present());
  homunculus.Acknowledge(77);
  CHECK(homunculus.present());
  CHECK(homunculus.actor_id() == 77);
  CHECK(homunculus.active());
}

TEST_CASE("Homoncule : au repos ou sans PV, il n'est plus actif", "[sim][homunculus]") {
  Homunculus homunculus;
  homunculus.Acknowledge(77);
  static_cast<void>(homunculus.TakeSheet(HomunculusSheet{.resting = true, .alive = true}));
  CHECK(homunculus.present());
  CHECK_FALSE(homunculus.active());
  static_cast<void>(homunculus.TakeSheet(HomunculusSheet{.alive = false}));
  CHECK_FALSE(homunculus.active());
}

TEST_CASE("Homoncule : l'alerte de faim s'écrit une fois par palier", "[sim][homunculus]") {
  Homunculus homunculus;
  CHECK_FALSE(homunculus.TakeSheet(Hungry(11)));
  CHECK(homunculus.TakeSheet(Hungry(10)));
  // La même satiété redite ne se répète pas.
  CHECK_FALSE(homunculus.TakeSheet(Hungry(10)));
  CHECK_FALSE(homunculus.TakeSheet(Hungry(7)));
  CHECK(homunculus.TakeSheet(Hungry(5)));
  CHECK(homunculus.TakeSheet(Hungry(1)));
  CHECK(homunculus.TakeSheet(Hungry(10)));
}

TEST_CASE("Homoncule : une compétence se met à jour sans perdre sa visée", "[sim][homunculus]") {
  Homunculus homunculus;
  homunculus.TakeSkills({HomunculusSkill{.skill_id = 8005,
                                         .targeting = 4,
                                         .level = 1,
                                         .upgradable = true,
                                         .server_name = "HAMI_CASTLE"}});
  homunculus.UpdateSkill(HomunculusSkill{.skill_id = 8005, .level = 2, .sp_cost = 10});
  const HomunculusSkill* castle = homunculus.FindSkill(8005);
  REQUIRE(castle != nullptr);
  CHECK(castle->level == 2);
  CHECK(castle->sp_cost == 10);
  CHECK(castle->targeting == 4);
  CHECK(castle->server_name == "HAMI_CASTLE");
  CHECK_FALSE(castle->upgradable);
  // Une compétence inconnue de la liste s'y ajoute.
  homunculus.UpdateSkill(HomunculusSkill{.skill_id = 8006, .level = 1});
  CHECK(homunculus.skills().size() == 2);
}

TEST_CASE("Homoncule : abandonné, il disparaît ; un autre personnage efface tout",
          "[sim][homunculus]") {
  Homunculus homunculus;
  homunculus.Acknowledge(77);
  homunculus.SetAutoFeed(true);
  homunculus.Forget();
  CHECK_FALSE(homunculus.present());
  CHECK(homunculus.actor_id() == 0);
  homunculus.Reset();
  CHECK_FALSE(homunculus.auto_feed());
  CHECK_FALSE(homunculus.sheet_known());
}

TEST_CASE("Homoncule : le repos que le joueur lance le rend absent jusqu'au prochain appel",
          "[sim][homunculus]") {
  constexpr std::uint16_t kRest = 244;
  constexpr std::uint16_t kCall = 243;
  Homunculus homunculus;
  homunculus.Acknowledge(77);
  // Une autre compétence ne change rien.
  CHECK_FALSE(homunculus.NoticeOwnSkill(kCall, 100));
  CHECK(homunculus.present());
  CHECK(homunculus.NoticeOwnSkill(kRest, 200));
  CHECK_FALSE(homunculus.present());
  // Rappelé, il est accusé de nouveau ; la même annonce relue ne compte plus.
  homunculus.Acknowledge(78);
  CHECK_FALSE(homunculus.NoticeOwnSkill(kRest, 200));
  CHECK(homunculus.present());
  // Un nouveau repos, oui.
  CHECK(homunculus.NoticeOwnSkill(kRest, 300));
  CHECK_FALSE(homunculus.present());
}

TEST_CASE("Compagnon : seule une compétence sur soi part de la fiche ou d'une barre",
          "[sim][homunculus]") {
  using ro::sim::CompanionCast;
  using ro::sim::CompanionCastOf;
  CHECK(CompanionCastOf(ro::sim::kSkillOnSelf, 1) == CompanionCast::kNow);
  CHECK(CompanionCastOf(ro::sim::kSkillPassive, 3) == CompanionCast::kPassive);
  CHECK(CompanionCastOf(ro::sim::kSkillOnSelf, 0) == CompanionCast::kNotLearned);
  // Une cible ou une case demanderait une visée mesurée depuis le compagnon.
  CHECK(CompanionCastOf(ro::sim::kSkillOnEnemy, 5) == CompanionCast::kNeedsAim);
  CHECK(CompanionCastOf(ro::sim::kSkillOnFriend, 5) == CompanionCast::kNeedsAim);
  CHECK(CompanionCastOf(ro::sim::kSkillOnGround | ro::sim::kSkillOnSelf, 5) ==
        CompanionCast::kNeedsAim);
}
