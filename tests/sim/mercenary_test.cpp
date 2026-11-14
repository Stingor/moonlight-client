#include "ro/sim/mercenary.h"

#include <cstdint>

#include <catch2/catch_test_macros.hpp>

namespace {

constexpr std::int64_t kNowUnix = 1'800'000'000;

ro::sim::MercenaryProfile Profile(std::uint32_t expire_unix_s) {
  ro::sim::MercenaryProfile profile;
  profile.actor_id = 110000001;
  profile.name = "Leo";
  profile.hp = 100;
  profile.max_hp = 200;
  profile.expire_unix_s = expire_unix_s;
  return profile;
}

}  // namespace

TEST_CASE("MercenarySim : la fiche le fait paraître, le contrat se décompte", "[mercenarysim]") {
  ro::sim::MercenaryBook book;
  CHECK_FALSE(book.present());
  book.Meet(Profile(static_cast<std::uint32_t>(kNowUnix + 1800)), 5000, kNowUnix);
  REQUIRE(book.present());
  CHECK(book.contract_left_s(5000) == 1800);
  CHECK(book.contract_left_s(5000 + 60'000) == 1740);
  // Une fraction de seconde restante compte encore pour une.
  CHECK(book.contract_left_s(5000 + 1'799'500) == 1);
  CHECK(book.contract_left_s(5000 + 1'800'000) == 0);
  CHECK(book.contract_left_s(5000 + 9'000'000) == 0);
}

TEST_CASE("MercenarySim : un contrat déjà passé est nul", "[mercenarysim]") {
  ro::sim::MercenaryBook book;
  book.Meet(Profile(static_cast<std::uint32_t>(kNowUnix - 10)), 0, kNowUnix);
  CHECK(book.contract_left_s(0) == 0);
}

TEST_CASE("MercenarySim : le décompte survit au rebouclage de l'horloge", "[mercenarysim]") {
  ro::sim::MercenaryBook book;
  const std::uint32_t near_wrap_ms = 0xFFFF'F000U;
  book.Meet(Profile(static_cast<std::uint32_t>(kNowUnix + 60)), near_wrap_ms, kNowUnix);
  CHECK(book.contract_left_s(near_wrap_ms + 30'000) == 30);
}

TEST_CASE("MercenarySim : une valeur ne change rien sans mercenaire", "[mercenarysim]") {
  ro::sim::MercenaryBook book;
  CHECK_FALSE(book.Change(ro::sim::MercenaryStat::kHp, 50));
  book.Meet(Profile(0), 0, kNowUnix);
  CHECK(book.Change(ro::sim::MercenaryStat::kHp, 50));
  CHECK(book.Change(ro::sim::MercenaryStat::kFaith, 12));
  CHECK(book.Change(ro::sim::MercenaryStat::kKills, 3));
  CHECK(book.profile().hp == 50);
  CHECK(book.profile().faith == 12);
  CHECK(book.profile().kills == 3);
}

TEST_CASE("MercenarySim : le départ oublie tout et garde son adieu", "[mercenarysim]") {
  ro::sim::MercenaryBook book;
  book.Meet(Profile(0), 0, kNowUnix);
  book.SetSkills({{.skill_id = 8201, .targeting = 1, .level = 5}});
  REQUIRE(book.FindSkill(8201) != nullptr);
  book.Part(ro::sim::MercenaryFarewell::kDied);
  CHECK_FALSE(book.present());
  CHECK(book.skills().empty());
  CHECK(book.FindSkill(8201) == nullptr);
  // La disparition qui suit l'annonce n'efface pas l'adieu à dire.
  book.Part(ro::sim::MercenaryFarewell::kNone);
  CHECK(book.TakeFarewell() == ro::sim::MercenaryFarewell::kDied);
  CHECK(book.TakeFarewell() == ro::sim::MercenaryFarewell::kNone);
}

TEST_CASE("MercenarySim : la guilde se lit à la classe", "[mercenarysim]") {
  CHECK(ro::sim::MercenaryGuildOfJob(6017) == ro::sim::MercenaryGuild::kArcher);
  CHECK(ro::sim::MercenaryGuildOfJob(6026) == ro::sim::MercenaryGuild::kArcher);
  CHECK(ro::sim::MercenaryGuildOfJob(6027) == ro::sim::MercenaryGuild::kSpearman);
  CHECK(ro::sim::MercenaryGuildOfJob(6046) == ro::sim::MercenaryGuild::kSwordman);
  CHECK(ro::sim::MercenaryGuildOfJob(6047) == ro::sim::MercenaryGuild::kMonster);
  CHECK(ro::sim::MercenaryGuildOfJob(1002) == ro::sim::MercenaryGuild::kMonster);
}

TEST_CASE("MercenarySim : l'ASPD se déduit du délai d'attaque", "[mercenarysim]") {
  CHECK(ro::sim::MercenaryAspd(700) == 130);
  CHECK(ro::sim::MercenaryAspd(1995) == 0);
  CHECK(ro::sim::MercenaryAspd(2000) == 0);
}

TEST_CASE("MercenarySim : la fin du contrat se rappelle à 5 puis à 1 minute, une fois chacune",
          "[mercenarysim]") {
  ro::sim::MercenaryBook book;
  book.Meet(Profile(static_cast<std::uint32_t>(kNowUnix + 600)), 0, kNowUnix);
  CHECK(book.TakeContractReminder(0) == 0);
  CHECK(book.TakeContractReminder(299'000) == 0);
  CHECK(book.TakeContractReminder(300'000) == 5);
  CHECK(book.TakeContractReminder(301'000) == 0);
  CHECK(book.TakeContractReminder(540'000) == 1);
  CHECK(book.TakeContractReminder(560'000) == 0);
}

TEST_CASE("MercenarySim : une fiche renvoyée ne refait pas sonner un rappel passé",
          "[mercenarysim]") {
  ro::sim::MercenaryBook book;
  book.Meet(Profile(static_cast<std::uint32_t>(kNowUnix + 600)), 0, kNowUnix);
  CHECK(book.TakeContractReminder(300'000) == 5);
  // Le changement de carte efface, puis le serveur renvoie la fiche : 4 min.
  book.Part(ro::sim::MercenaryFarewell::kNone);
  book.Meet(Profile(static_cast<std::uint32_t>(kNowUnix + 600)), 300'000, kNowUnix + 360);
  CHECK(book.TakeContractReminder(300'000) == 0);
  CHECK(book.TakeContractReminder(480'000) == 1);
}

TEST_CASE("MercenarySim : deux paliers franchis d'un coup ne disent que le plus proche",
          "[mercenarysim]") {
  ro::sim::MercenaryBook book;
  book.Meet(Profile(static_cast<std::uint32_t>(kNowUnix + 600)), 0, kNowUnix);
  CHECK(book.TakeContractReminder(570'000) == 1);
  CHECK(book.TakeContractReminder(580'000) == 0);
  // Sans mercenaire, rien.
  book.Part(ro::sim::MercenaryFarewell::kContractEnded);
  CHECK(book.TakeContractReminder(590'000) == 0);
}
