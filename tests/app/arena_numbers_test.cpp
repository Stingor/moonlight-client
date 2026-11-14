// LES CHIFFRES DE L'ARÈNE : la fusion des coups rapprochés sur un même monstre,
// le plafond des nombres vivants et l'éviction du plus petit, et la table des
// couleurs (U14) à ses bornes.

#include "survivors/arena_numbers.h"

#include <algorithm>
#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/survivors_run.h"

#include "support/survivors_fixture.h"

namespace {

constexpr std::uint32_t kFoe = 7;
constexpr std::uint32_t kOtherFoe = 8;
constexpr std::uint64_t kStartMs = 10'000;
constexpr std::int32_t kPlainPct = 100;
constexpr ro::sim::FieldPoint kSomewhere{.x = 4096, .y = 4096};

}  // namespace

TEST_CASE("Chiffres de l'arène : deux coups à 199 ms fusionnent, à 201 ms non",
          "[app][survivors]") {
  moonclient::ArenaNumberPool merged;
  merged.Add(kFoe, false, kSomewhere, 10, kPlainPct, kStartMs);
  merged.Add(kFoe, false, kSomewhere, 15, kPlainPct, kStartMs + 199);
  REQUIRE(merged.live().size() == 1);
  CHECK(merged.live().front().amount == 25);

  moonclient::ArenaNumberPool apart;
  apart.Add(kFoe, false, kSomewhere, 10, kPlainPct, kStartMs);
  apart.Add(kFoe, false, kSomewhere, 15, kPlainPct, kStartMs + 201);
  CHECK(apart.live().size() == 2);

  // Un autre monstre, ou le héros, ne fusionne jamais.
  moonclient::ArenaNumberPool others;
  others.Add(kFoe, false, kSomewhere, 10, kPlainPct, kStartMs);
  others.Add(kOtherFoe, false, kSomewhere, 10, kPlainPct, kStartMs);
  others.Add(0, true, kSomewhere, 10, kPlainPct, kStartMs);
  CHECK(others.live().size() == 3);
}

TEST_CASE("Chiffres de l'arène : un critique ne se fond qu'avec un critique", "[app][survivors]") {
  moonclient::ArenaNumberPool pool;
  pool.Add(kFoe, false, kSomewhere, 10, kPlainPct, kStartMs);
  pool.Add(kFoe, false, kSomewhere, 20, kPlainPct, kStartMs + 10, true);
  REQUIRE(pool.live().size() == 2);
  CHECK_FALSE(pool.live().front().critical);
  CHECK(pool.live().back().critical);
  pool.Add(kFoe, false, kSomewhere, 30, kPlainPct, kStartMs + 20, true);
  REQUIRE(pool.live().size() == 2);
  CHECK(pool.live().front().amount == 10);
  CHECK(pool.live().back().amount == 50);
}

TEST_CASE("Chiffres de l'arène : une esquive montre « Miss », sans se fondre avec un coup",
          "[app][survivors]") {
  auto run = ro::sim::SurvivorsRun::Start(ro::test_support::QuietRules(),
                                          ro::test_support::PillarArena(48),
                                          ro::test_support::TinyStart(7));
  REQUIRE(run.has_value());
  constexpr std::int32_t kHurt = 12;
  ro::sim::SurvivorsNews news;
  news.dodged_by_hero[0] = 2U;
  moonclient::ArenaNumbers numbers;
  numbers.Notice(news, run->field(), kStartMs);
  REQUIRE(numbers.pool().live().size() == 1);
  const moonclient::ArenaNumber& miss = numbers.pool().live().front();
  CHECK(miss.hero_hurt);
  CHECK(miss.amount == 0);
  CHECK(moonclient::ArenaNumberStyleOf(miss.amount, miss.efficiency_pct, miss.hero_hurt).miss);

  // Un coup encaissé juste après garde son chiffre, et le « Miss » reste.
  ro::sim::SurvivorsNews hurt;
  hurt.hurt_by_hero[0] = kHurt;
  numbers.Notice(hurt, run->field(), kStartMs + 1U);
  REQUIRE(numbers.pool().live().size() == 2);
  CHECK(numbers.pool().live().front().amount == 0);
  CHECK(numbers.pool().live().back().amount == kHurt);
}

TEST_CASE("Chiffres de l'arène : la nouvelle d'un critique le dit, et d'un coup ordinaire non",
          "[app][survivors]") {
  auto run = ro::sim::SurvivorsRun::Start(ro::test_support::QuietRules(),
                                          ro::test_support::PillarArena(48),
                                          ro::test_support::TinyStart(7));
  REQUIRE(run.has_value());
  ro::sim::SurvivorsNews news;
  news.hits.push_back(ro::sim::SurvivorsHit{.foe_serial = kFoe, .amount = 10});
  news.hits.push_back(
      ro::sim::SurvivorsHit{.foe_serial = kOtherFoe, .amount = 20, .critical = true});
  moonclient::ArenaNumbers numbers;
  numbers.Notice(news, run->field(), kStartMs);
  REQUIRE(numbers.pool().live().size() == 2);
  CHECK_FALSE(numbers.pool().live().front().critical);
  CHECK(numbers.pool().live().back().critical);
}

TEST_CASE("Chiffres de l'arène : la fusion garde la plus forte efficacité", "[app][survivors]") {
  moonclient::ArenaNumberPool pool;
  pool.Add(kFoe, false, kSomewhere, 10, 40, kStartMs);
  pool.Add(kFoe, false, kSomewhere, 10, 160, kStartMs + 50);
  REQUIRE(pool.live().size() == 1);
  CHECK(pool.live().front().efficiency_pct == 160);
}

TEST_CASE("Chiffres de l'arène : 48 vivants, puis le plus petit cède", "[app][survivors]") {
  moonclient::ArenaNumberPool pool;
  for (std::uint32_t serial = 1; serial <= moonclient::kMostArenaNumbers; ++serial) {
    pool.Add(serial, false, kSomewhere, static_cast<std::int32_t>(serial) + 10, kPlainPct,
             kStartMs);
  }
  REQUIRE(pool.live().size() == moonclient::kMostArenaNumbers);
  // Le plus petit, 11, cède devant 100.
  pool.Add(1000, false, kSomewhere, 100, kPlainPct, kStartMs);
  CHECK(pool.live().size() == moonclient::kMostArenaNumbers);
  const auto amount_of = [&pool](std::int32_t amount) {
    return std::ranges::count(pool.live(), amount, &moonclient::ArenaNumber::amount);
  };
  CHECK(amount_of(11) == 0);
  CHECK(amount_of(100) == 1);
  // Un coup plus faible que tous ceux qui montent ne paraît pas.
  pool.Add(2000, false, kSomewhere, 1, kPlainPct, kStartMs);
  CHECK(amount_of(1) == 0);
  CHECK(pool.live().size() == moonclient::kMostArenaNumbers);
}

TEST_CASE("Chiffres de l'arène : un coup encaissé paraît même sous 48 nombres plus forts",
          "[app][survivors]") {
  moonclient::ArenaNumberPool pool;
  for (std::uint32_t serial = 1; serial <= moonclient::kMostArenaNumbers; ++serial) {
    pool.Add(serial, false, kSomewhere, static_cast<std::int32_t>(serial) + 300, kPlainPct,
             kStartMs);
  }
  // Le héros encaisse 25 : le plus petit nombre de monstre, 301, cède.
  pool.Add(0, true, kSomewhere, 25, kPlainPct, kStartMs);
  CHECK(pool.live().size() == moonclient::kMostArenaNumbers);
  const auto hurt = std::ranges::find_if(
      pool.live(), [](const moonclient::ArenaNumber& number) { return number.hero_hurt; });
  REQUIRE(hurt != pool.live().end());
  CHECK(hurt->amount == 25);
  CHECK(std::ranges::count(pool.live(), 301, &moonclient::ArenaNumber::amount) == 0);
  // Et un coup porté plus fort ne le chasse pas : un autre monstre cède.
  pool.Add(5000, false, kSomewhere, 9000, kPlainPct, kStartMs);
  CHECK(std::ranges::count_if(pool.live(), [](const moonclient::ArenaNumber& number) {
          return number.hero_hurt;
        }) == 1);
}

TEST_CASE("Chiffres de l'arène : un nombre s'éteint au terme de sa vie", "[app][survivors]") {
  moonclient::ArenaNumberPool pool;
  pool.Add(kFoe, false, kSomewhere, 10, kPlainPct, kStartMs);
  pool.Expire(kStartMs + 100);
  CHECK(pool.live().size() == 1);
  pool.Expire(kStartMs + 10'000);
  CHECK(pool.live().empty());
}

TEST_CASE("Chiffres de l'arène : la table des couleurs aux bornes", "[app][survivors]") {
  using moonclient::ArenaNumberStyleOf;
  const moonclient::ArenaNumberStyle strong{.color = moonclient::kStrongHitColor,
                                            .scale = moonclient::kStrongHitScale};
  const moonclient::ArenaNumberStyle plain{.color = moonclient::kPlainHitColor,
                                           .scale = moonclient::kPlainHitScale};
  const moonclient::ArenaNumberStyle weak{.color = moonclient::kWeakHitColor,
                                          .scale = moonclient::kWeakHitScale};
  CHECK(ArenaNumberStyleOf(10, 150, false) == strong);
  CHECK(ArenaNumberStyleOf(10, 149, false) == plain);
  CHECK(ArenaNumberStyleOf(10, 51, false) == plain);
  CHECK(ArenaNumberStyleOf(10, 50, false) == weak);
  CHECK(ArenaNumberStyleOf(10, 0, false) == weak);
  // Le coup encaissé est rouge, quelle que soit l'efficacité.
  CHECK(ArenaNumberStyleOf(10, 200, true).color == moonclient::kHeroHurtColor);
  // Zéro dégât : « Miss ».
  CHECK(ArenaNumberStyleOf(0, 150, false).miss);
  CHECK(ArenaNumberStyleOf(0, 100, true).miss);
  CHECK_FALSE(ArenaNumberStyleOf(1, 100, false).miss);
}
