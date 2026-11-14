#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/core/pcg32.h"
#include "ro/sim/survivors_geometry.h"
#include "ro/sim/survivors_news.h"
#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_run.h"
#include "ro/sim/survivors_state.h"

#include "support/survivors_fixture.h"

using ro::sim::CastKind;
using ro::sim::Cells;
using ro::sim::FieldPoint;
using ro::sim::StepsOf;
using ro::sim::SurvivorsElement;
using ro::sim::SurvivorsField;
using ro::sim::SurvivorsRun;
using ro::sim::SurvivorsStart;
using ro::sim::SurvivorsStat;
using ro::sim::TargetRule;
using ro::sim::TurnVector;
using ro::test_support::kTinyHomingId;
using ro::test_support::kTinyLunaticId;
using ro::test_support::kTinyOrbitId;
using ro::test_support::kTinyPoringId;
using ro::test_support::kTinyStrikeId;
using ro::test_support::PillarArena;
using ro::test_support::PlaceFoe;
using ro::test_support::QuietRules;
using ro::test_support::SetBonus;
using ro::test_support::StepStill;
using ro::test_support::TinyRules;
using ro::test_support::TinyStart;

namespace {

constexpr std::int32_t kArenaSideCells = 64;
// Un monstre qui encaisse tout un essai sans tomber.
constexpr std::int32_t kSturdyHp = 1000;

// Un monstre immobile de `hp` PV, à `offset` du héros : ni la horde ni le
// contact ne le déplacent ni ne le font frapper, tant qu'il reste à plus
// d'une case du héros.
std::uint32_t PlaceStill(SurvivorsField& field, std::uint16_t foe_id, FieldPoint offset,
                         std::int32_t hp) {
  const std::uint32_t serial = PlaceFoe(field, foe_id, field.hero.position + offset);
  ro::sim::SurvivorsFoe& foe = field.foes.back();
  foe.units_per_step = 0;
  foe.hp = hp;
  foe.max_hp = hp;
  return serial;
}

[[nodiscard]] FieldPoint East(std::int32_t cells) { return FieldPoint{Cells(cells), 0}; }

// Les règles silencieuses, dont un essai retouche les niveaux des trois skills
// ou la table des éléments. Les spans pointent dans l'objet : il ne se copie
// pas, et survit à sa partie.
struct SkillVariant {
  std::array<ro::sim::SkillLevel, 2> strike = ro::test_support::kTinyStrikeLevels;
  std::array<ro::sim::SkillLevel, 2> homing = ro::test_support::kTinyHomingLevels;
  std::array<ro::sim::SkillLevel, 2> orbit = ro::test_support::kTinyOrbitLevels;
  std::array<ro::sim::SurvivorsSkillRule, 3> skills = ro::test_support::kTinySkills;
  std::array<ro::sim::ElementRow, ro::sim::kSurvivorsElementCount> elements =
      ro::test_support::kTinyElements;
  ro::sim::SurvivorsRules rules = QuietRules();

  SkillVariant() {
    skills[0].levels = strike;
    skills[1].levels = homing;
    skills[2].levels = orbit;
    rules.skills = skills;
    rules.elements = elements;
  }
  SkillVariant(const SkillVariant&) = delete;
  SkillVariant& operator=(const SkillVariant&) = delete;

  void SetElementPct(SurvivorsElement attack, SurvivorsElement defence, std::int16_t pct) {
    elements[static_cast<std::size_t>(attack)][static_cast<std::size_t>(defence)] = pct;
  }
};

[[nodiscard]] SurvivorsStart StartWith(std::uint16_t skill_id, std::uint64_t seed = 1) {
  SurvivorsStart start = TinyStart(seed);
  start.first_skill_id = skill_id;
  return start;
}

[[nodiscard]] std::vector<std::uint32_t> StruckSerials(const SurvivorsField& field) {
  std::vector<std::uint32_t> serials;
  for (const ro::sim::SurvivorsStrike& strike : field.strikes) serials.push_back(strike.foe_serial);
  return serials;
}

[[nodiscard]] std::size_t CountCasts(const ro::sim::SurvivorsNews& news, CastKind kind) {
  return static_cast<std::size_t>(
      std::count_if(news.casts.begin(), news.casts.end(),
                    [&](const ro::sim::SurvivorsCast& cast) { return cast.kind == kind; }));
}

}  // namespace

TEST_CASE("Compétences survivors : le skill de départ", "[sim][survivors]") {
  ro::sim::SurvivorsStart start = TinyStart(1);
  start.first_skill_id = kTinyOrbitId;
  const auto run = SurvivorsRun::Start(TinyRules(), PillarArena(64), start);
  REQUIRE(run.has_value());
  REQUIRE(run->field().hero.skills.size() == 1);
  CHECK(run->field().hero.skills[0].skill_id == kTinyOrbitId);
  CHECK(run->field().hero.skills[0].phase_step == 0U);
  // Les orbes se demandent dans un tampon que l'appel vide d'abord.
  // Au pas 0, l'orbite de niveau 1 est dans sa phase active, avec une orbe.
  std::vector<ro::sim::SurvivorsOrb> orbs(3);
  run->Orbs(orbs);
  CHECK(orbs.size() == 1);
}

TEST_CASE("Compétences survivors : sept frappes sur sept monstres, aucune gâchée",
          "[sim][survivors]") {
  SurvivorsStart start = TinyStart(7);
  SetBonus(start, SurvivorsStat::kAmount, 5);
  auto run = SurvivorsRun::Start(QuietRules(), PillarArena(kArenaSideCells), start);
  REQUIRE(run.has_value());
  SurvivorsField& field = run->field_for_tests();
  std::vector<std::uint32_t> placed;
  for (std::int32_t cells = 2; cells < 9; ++cells) {
    placed.push_back(PlaceStill(field, kTinyLunaticId, East(cells), 10));
  }

  StepStill(*run);
  // Chaque frappe est promise : la suivante vise le monstre suivant, du plus
  // proche au plus lointain.
  CHECK(StruckSerials(field) == placed);
  for (const ro::sim::SurvivorsFoe& foe : field.foes) CHECK(foe.pending_damage == 10);
  // Une frappe toutes les 120 ms, la première au pas suivant.
  REQUIRE(field.strikes.size() == 7);
  CHECK(field.strikes[0].land_step == 1U);
  CHECK(field.strikes[6].land_step == 1U + 6U * StepsOf(120));
  CHECK(field.hero.skills[0].ready_step == StepsOf(1000));

  StepStill(*run, 40);
  CHECK(field.foes.empty());
  CHECK(field.strikes.empty());
  CHECK(field.tally.kills == 7U);
  const ro::sim::SurvivorsNews news = run->TakeNews();
  REQUIRE(news.hits.size() == 7);
  for (std::size_t hit = 0; hit < news.hits.size(); ++hit) {
    CHECK(news.hits[hit].foe_serial == placed[hit]);
    CHECK(news.hits[hit].amount == 10);
  }
  CHECK(CountCasts(news, CastKind::kStrike) == 7);
}

TEST_CASE("Compétences survivors : les frappes surnuméraires", "[sim][survivors]") {
  SECTION("un monstre déjà couvert reprend les frappes de trop") {
    auto run = SurvivorsRun::Start(QuietRules(), PillarArena(kArenaSideCells), TinyStart(1));
    REQUIRE(run.has_value());
    SurvivorsField& field = run->field_for_tests();
    const std::uint32_t lunatic = PlaceStill(field, kTinyLunaticId, East(3), 6);
    StepStill(*run);
    CHECK(StruckSerials(field) == std::vector<std::uint32_t>{lunatic, lunatic});
    CHECK(field.foes[0].pending_damage == 20);
  }
  SECTION("un boss encaisse ce que les petits n'ont pas pris") {
    SurvivorsStart start = TinyStart(1);
    SetBonus(start, SurvivorsStat::kAmount, 2);
    auto run = SurvivorsRun::Start(QuietRules(), PillarArena(kArenaSideCells), start);
    REQUIRE(run.has_value());
    SurvivorsField& field = run->field_for_tests();
    const std::uint32_t lunatic = PlaceStill(field, kTinyLunaticId, East(2), 6);
    const std::uint32_t boss = PlaceStill(field, kTinyPoringId, East(4), kSturdyHp);
    StepStill(*run);
    CHECK(StruckSerials(field) == std::vector<std::uint32_t>{lunatic, boss, boss, boss});
    // Le feu contre l'eau : 50 %, et la promesse le sait.
    CHECK(field.foes[1].pending_damage == 15);
  }
  SECTION("une frappe arrivée sur un monstre déjà tombé se perd") {
    auto run = SurvivorsRun::Start(QuietRules(), PillarArena(kArenaSideCells), TinyStart(1));
    REQUIRE(run.has_value());
    SurvivorsField& field = run->field_for_tests();
    PlaceStill(field, kTinyLunaticId, East(3), 6);
    // Avant la recharge : la première frappe tue, la seconde arrive sur rien.
    StepStill(*run, 40);
    CHECK(field.foes.empty());
    CHECK(field.strikes.empty());
    CHECK(field.tally.kills == 1U);
    const ro::sim::SurvivorsNews news = run->TakeNews();
    CHECK(news.hits.size() == 1);
    CHECK(CountCasts(news, CastKind::kStrike) == 2);
  }
}

TEST_CASE("Compétences survivors : la visée du plus robuste et du hasard", "[sim][survivors]") {
  SECTION("kStrongest vise les PV non promis les plus hauts") {
    SkillVariant variant;
    variant.skills[0].target = TargetRule::kStrongest;
    auto run = SurvivorsRun::Start(variant.rules, PillarArena(kArenaSideCells), TinyStart(1));
    REQUIRE(run.has_value());
    SurvivorsField& field = run->field_for_tests();
    PlaceStill(field, kTinyLunaticId, East(2), 6);
    const std::uint32_t strongest = PlaceStill(field, kTinyLunaticId, East(4), 10);
    const std::uint32_t second = PlaceStill(field, kTinyLunaticId, East(6), 8);
    StepStill(*run);
    CHECK(StruckSerials(field) == std::vector<std::uint32_t>{strongest, second});
  }
  SECTION("kStrongest départage des PV égaux par la naissance, non par la distance") {
    SkillVariant variant;
    variant.skills[0].target = TargetRule::kStrongest;
    auto run = SurvivorsRun::Start(variant.rules, PillarArena(kArenaSideCells), TinyStart(1));
    REQUIRE(run.has_value());
    SurvivorsField& field = run->field_for_tests();
    const std::uint32_t elder = PlaceStill(field, kTinyLunaticId, East(6), 10);
    const std::uint32_t younger = PlaceStill(field, kTinyLunaticId, East(2), 10);
    StepStill(*run);
    CHECK(StruckSerials(field) == std::vector<std::uint32_t>{elder, younger});
  }
  SECTION("kRandom tire chaque frappe parmi les monstres non couverts") {
    SkillVariant variant;
    variant.skills[0].target = TargetRule::kRandom;
    constexpr std::uint64_t kSeed = 2026;
    auto run = SurvivorsRun::Start(variant.rules, PillarArena(kArenaSideCells), TinyStart(kSeed));
    REQUIRE(run.has_value());
    SurvivorsField& field = run->field_for_tests();
    std::vector<std::uint32_t> open;
    for (std::int32_t cells = 2; cells < 7; ++cells) {
      open.push_back(PlaceStill(field, kTinyLunaticId, East(cells), 10));
    }
    StepStill(*run);
    // Le même tirage, refait : un Below par frappe, sur les restants, dans
    // l'ordre de naissance.
    ro::core::Pcg32 combat(kSeed, ro::sim::kCombatStream);
    std::vector<std::uint32_t> expected;
    for (int strike = 0; strike < 2; ++strike) {
      const std::uint32_t drawn = combat.Below(static_cast<std::uint32_t>(open.size()));
      expected.push_back(open[drawn]);
      open.erase(open.begin() + drawn);
    }
    CHECK(StruckSerials(field) == expected);
    CHECK(field.combat_random == combat);
  }
}

TEST_CASE("Compétences survivors : un skill sans monstre à portée attend", "[sim][survivors]") {
  auto run = SurvivorsRun::Start(QuietRules(), PillarArena(kArenaSideCells), TinyStart(1));
  REQUIRE(run.has_value());
  SurvivorsField& field = run->field_for_tests();
  PlaceStill(field, kTinyLunaticId, East(10), 6);
  StepStill(*run, 3);
  CHECK(field.strikes.empty());
  CHECK(field.hero.skills[0].ready_step == 0U);
  CHECK(run->TakeNews().casts.empty());

  // Le monstre entre à portée : la salve part au pas même.
  PlaceStill(field, kTinyLunaticId, East(9), 6);
  StepStill(*run);
  CHECK(field.strikes.size() == 2);
  CHECK(field.hero.skills[0].ready_step == 3U + StepsOf(1000));
}

TEST_CASE("Compétences survivors : la recharge raccourcie, plafonnée", "[sim][survivors]") {
  // La recharge de 1 000 ms après la salve du pas 0, raccourcie de `cut_pct`.
  const auto ready_after = [](std::int32_t cut_pct) {
    SurvivorsStart start = TinyStart(1);
    SetBonus(start, SurvivorsStat::kCooldownPct, cut_pct);
    auto run = SurvivorsRun::Start(QuietRules(), PillarArena(kArenaSideCells), start);
    REQUIRE(run.has_value());
    PlaceStill(run->field_for_tests(), kTinyLunaticId, East(3), kSturdyHp);
    StepStill(*run);
    REQUIRE(run->field().strikes.size() == 2);
    return run->field().hero.skills[0].ready_step;
  };
  CHECK(ready_after(30) == StepsOf(1000 * 70 / 100));
  // Au-delà du plafond des règles (60 %), la recharge ne raccourcit plus.
  REQUIRE(QuietRules().tuning.most_cooldown_pct == 60);
  CHECK(ready_after(90) == StepsOf(1000 * 40 / 100));
  CHECK(ready_after(60) == StepsOf(1000 * 40 / 100));
}

TEST_CASE("Compétences survivors : le glaçage", "[sim][survivors]") {
  SkillVariant variant;
  variant.strike[0].chill_ms = 1500;
  SECTION("la seconde frappe prolonge le glaçage") {
    auto run = SurvivorsRun::Start(variant.rules, PillarArena(kArenaSideCells), TinyStart(1));
    REQUIRE(run.has_value());
    SurvivorsField& field = run->field_for_tests();
    PlaceStill(field, kTinyLunaticId, East(3), kSturdyHp);
    StepStill(*run, 2);
    CHECK(field.foes[0].chill_until_step == 1U + StepsOf(1500));
    StepStill(*run, 6);
    CHECK(field.foes[0].chill_until_step == 7U + StepsOf(1500));
  }
  SECTION("une frappe ne raccourcit jamais un glaçage plus long") {
    auto run = SurvivorsRun::Start(variant.rules, PillarArena(kArenaSideCells), TinyStart(1));
    REQUIRE(run.has_value());
    SurvivorsField& field = run->field_for_tests();
    PlaceStill(field, kTinyLunaticId, East(3), kSturdyHp);
    StepStill(*run, 2);
    // Une échéance au-delà de celle que posera la seconde frappe, au pas 7.
    constexpr std::uint32_t kLongChillStep = 1000;
    static_assert(kLongChillStep > 7U + StepsOf(1500));
    field.foes[0].chill_until_step = kLongChillStep;
    StepStill(*run, 6);
    REQUIRE(field.strikes.empty());
    CHECK(field.foes[0].chill_until_step == kLongChillStep);
  }
  SECTION("majorée de la durée") {
    SurvivorsStart start = TinyStart(1);
    SetBonus(start, SurvivorsStat::kDurationPct, 100);
    auto run = SurvivorsRun::Start(variant.rules, PillarArena(kArenaSideCells), start);
    REQUIRE(run.has_value());
    SurvivorsField& field = run->field_for_tests();
    PlaceStill(field, kTinyLunaticId, East(3), kSturdyHp);
    StepStill(*run, 2);
    CHECK(field.foes[0].chill_until_step == 1U + StepsOf(3000));
  }
}

TEST_CASE("Compétences survivors : le tir chercheur poursuit son monstre", "[sim][survivors]") {
  auto run =
      SurvivorsRun::Start(QuietRules(), PillarArena(kArenaSideCells), StartWith(kTinyHomingId));
  REQUIRE(run.has_value());
  SurvivorsField& field = run->field_for_tests();
  const FieldPoint hero = field.hero.position;
  const std::uint32_t target = PlaceStill(field, kTinyLunaticId, East(5), kSturdyHp);
  // Neuf cases par seconde : 184 sous-unités par pas de 20 ms.
  constexpr std::int32_t kSpeedPerStep = Cells(9) * 20 / 1000;

  StepStill(*run);
  REQUIRE(field.shots.size() == 2);
  CHECK(field.shots[0].target_serial == target);
  CHECK(field.shots[0].velocity == FieldPoint{kSpeedPerStep, 0});
  CHECK(field.shots[0].position == hero + FieldPoint{kSpeedPerStep, 0});
  CHECK(field.shots[0].expires_step == StepsOf(2000));
  CHECK(CountCasts(run->TakeNews(), CastKind::kShot) == 2);

  // Le monstre change de place : le tir se tourne vers lui.
  field.foes[0].position = hero + FieldPoint{Cells(2), Cells(4)};
  const FieldPoint before = field.shots[0].position;
  StepStill(*run);
  const FieldPoint turned = ro::sim::Toward(before, field.foes[0].position, kSpeedPerStep);
  CHECK(field.shots[0].velocity == turned);
  CHECK(field.shots[0].position == before + turned);
  CHECK(field.shots[0].previous_position == before);

  // Son monstre tombe : il reprend le plus proche à portée.
  const std::uint32_t next = PlaceStill(field, kTinyLunaticId, East(7), kSturdyHp);
  field.foes[0].hp = 0;
  StepStill(*run);
  CHECK(field.shots[0].target_serial == next);
  REQUIRE(field.foes.size() == 1);
  CHECK(field.foes[0].serial == next);
}

TEST_CASE("Compétences survivors : un tir rapide s'arrête sur son monstre", "[sim][survivors]") {
  SkillVariant variant;
  variant.homing[0].amount = 1;
  // Deux mille sous-unités par pas : plus du double de la portée du contact.
  variant.homing[0].speed_units_per_s = Cells(100);
  auto run =
      SurvivorsRun::Start(variant.rules, PillarArena(kArenaSideCells), StartWith(kTinyHomingId));
  REQUIRE(run.has_value());
  SurvivorsField& field = run->field_for_tests();
  PlaceStill(field, kTinyLunaticId, East(5), kSturdyHp);
  // Deux pas pleins, puis le troisième s'arrête sur le monstre au lieu de le
  // dépasser.
  StepStill(*run, 3);
  CHECK(run->TakeNews().hits.size() == 1);
  CHECK(field.shots.empty());
}

TEST_CASE("Compétences survivors : au plafond des tirs, le skill attend", "[sim][survivors]") {
  SkillVariant variant;
  // Des tirs lents, qui expirent avant d'atteindre le monstre.
  variant.homing[0].speed_units_per_s = Cells(1);
  variant.rules.caps.most_shots = 2;
  auto run =
      SurvivorsRun::Start(variant.rules, PillarArena(kArenaSideCells), StartWith(kTinyHomingId));
  REQUIRE(run.has_value());
  SurvivorsField& field = run->field_for_tests();
  PlaceStill(field, kTinyLunaticId, East(9), kSturdyHp);
  const std::uint32_t cooldown_steps = StepsOf(1000);
  const std::uint32_t life_steps = StepsOf(2000);

  // La recharge passée, le plafond est plein : aucune salve, et pas de recharge.
  StepStill(*run, cooldown_steps + 1);
  CHECK(field.shots.size() == 2);
  CHECK(field.hero.skills[0].ready_step == cooldown_steps);
  CHECK(CountCasts(run->TakeNews(), CastKind::kShot) == 2);

  // Les deux tirs expirent au pas life_steps : la salve part au pas suivant.
  StepStill(*run, life_steps + 1 - (cooldown_steps + 1));
  CHECK(field.shots.empty());
  StepStill(*run);
  CHECK(field.shots.size() == 2);
  CHECK(field.hero.skills[0].ready_step == life_steps + 1 + cooldown_steps);
  CHECK(CountCasts(run->TakeNews(), CastKind::kShot) == 2);
}

TEST_CASE("Compétences survivors : perçage, un coup par monstre, expiration", "[sim][survivors]") {
  SkillVariant variant;
  variant.homing[0].amount = 1;
  variant.homing[0].damage = 1;
  variant.homing[0].cooldown_ms = 100000;
  SECTION("un perçage : le tir touche deux monstres, puis s'éteint") {
    variant.homing[0].pierce = 1;
    auto run =
        SurvivorsRun::Start(variant.rules, PillarArena(kArenaSideCells), StartWith(kTinyHomingId));
    REQUIRE(run.has_value());
    SurvivorsField& field = run->field_for_tests();
    const std::uint32_t first = PlaceStill(field, kTinyLunaticId, East(3), kSturdyHp);
    const std::uint32_t second = PlaceStill(field, kTinyLunaticId, East(5), kSturdyHp);
    PlaceStill(field, kTinyLunaticId, East(7), kSturdyHp);
    StepStill(*run, 60);
    CHECK(field.shots.empty());
    const ro::sim::SurvivorsNews news = run->TakeNews();
    REQUIRE(news.hits.size() == 2);
    CHECK(news.hits[0].foe_serial == first);
    CHECK(news.hits[1].foe_serial == second);
    CHECK(news.hits[0].skill_id == kTinyHomingId);
  }
  SECTION("un seul coup par monstre traversé, puis la fin de vie") {
    variant.homing[0].pierce = static_cast<std::uint8_t>(ro::sim::kMostShotHits - 1);
    auto run =
        SurvivorsRun::Start(variant.rules, PillarArena(kArenaSideCells), StartWith(kTinyHomingId));
    REQUIRE(run.has_value());
    SurvivorsField& field = run->field_for_tests();
    PlaceStill(field, kTinyLunaticId, East(3), kSturdyHp);
    const std::uint32_t life_steps = StepsOf(2000);
    StepStill(*run, life_steps);
    REQUIRE(field.shots.size() == 1);
    CHECK(field.shots[0].expires_step == life_steps);
    CHECK(std::size_t{field.shots[0].pierce_left} == ro::sim::kMostShotHits - 2);
    CHECK(field.shots[0].hit_count == 1);
    CHECK(run->TakeNews().hits.size() == 1);
    StepStill(*run);
    CHECK(field.shots.empty());
  }
}

TEST_CASE("Compétences survivors : le coup d'un tir éclabousse ses voisins", "[sim][survivors]") {
  constexpr std::int32_t kShotDamage = 100;
  constexpr std::int32_t kSplashPct = 60;
  SkillVariant variant;
  variant.homing[0].amount = 1;
  variant.homing[0].damage = kShotDamage;
  variant.homing[0].cooldown_ms = 100000;
  variant.homing[0].pierce = 0;
  variant.homing[0].area_radius_units = Cells(2);
  variant.homing[0].splash_pct = kSplashPct;
  auto run =
      SurvivorsRun::Start(variant.rules, PillarArena(kArenaSideCells), StartWith(kTinyHomingId));
  REQUIRE(run.has_value());
  SurvivorsField& field = run->field_for_tests();
  const std::uint32_t struck = PlaceStill(field, kTinyLunaticId, East(3), kSturdyHp);
  const std::uint32_t neighbour =
      PlaceStill(field, kTinyLunaticId, East(3) + FieldPoint{0, Cells(1)}, kSturdyHp);
  const std::uint32_t far =
      PlaceStill(field, kTinyLunaticId, East(3) + FieldPoint{0, Cells(8)}, kSturdyHp);
  StepStill(*run, 60);
  CHECK(field.shots.empty());
  const auto lost = [&](std::uint32_t serial) {
    const auto foe = std::ranges::find(field.foes, serial, &ro::sim::SurvivorsFoe::serial);
    REQUIRE(foe != field.foes.end());
    return kSturdyHp - foe->hp;
  };
  // Le monstre touché prend le coup entier, une seule fois ; son voisin la part
  // de l'éclat, sous le même élément ; le monstre hors du disque, rien.
  const std::int32_t struck_lost = lost(struck);
  const std::int32_t neighbour_lost = lost(neighbour);
  CHECK(neighbour_lost > 0);
  CHECK(neighbour_lost * 100 <= struck_lost * kSplashPct);
  CHECK(lost(far) == 0);
  const ro::sim::SurvivorsNews news = run->TakeNews();
  CHECK(news.hits.size() == 2);
}

TEST_CASE("Compétences survivors : une salve chercheuse se répartit", "[sim][survivors]") {
  SkillVariant variant;
  variant.homing[0].amount = 4;
  auto run =
      SurvivorsRun::Start(variant.rules, PillarArena(kArenaSideCells), StartWith(kTinyHomingId));
  REQUIRE(run.has_value());
  SurvivorsField& field = run->field_for_tests();
  // Nés du plus loin au plus proche : l'ordre de visée n'est pas la naissance.
  const std::uint32_t farthest = PlaceStill(field, kTinyLunaticId, East(7), kSturdyHp);
  const std::uint32_t nearest = PlaceStill(field, kTinyLunaticId, East(3), kSturdyHp);
  // À la même distance que le plus proche, à l'ouest : la naissance départage.
  const std::uint32_t tied = PlaceStill(field, kTinyLunaticId, East(-3), kSturdyHp);

  StepStill(*run);
  REQUIRE(field.shots.size() == 4);
  // Le k-ième tir vise le k-ième plus proche ; le quatrième reprend au premier.
  CHECK(field.shots[0].target_serial == nearest);
  CHECK(field.shots[1].target_serial == tied);
  CHECK(field.shots[2].target_serial == farthest);
  CHECK(field.shots[3].target_serial == nearest);
  const ro::sim::SurvivorsNews news = run->TakeNews();
  REQUIRE(news.casts.size() == 4);
  for (std::size_t shot = 0; shot < news.casts.size(); ++shot) {
    CHECK(news.casts[shot].kind == CastKind::kShot);
    CHECK(news.casts[shot].target_serial == field.shots[shot].target_serial);
  }
}

TEST_CASE("Compétences survivors : un tir perçant ne retouche jamais un monstre",
          "[sim][survivors]") {
  SkillVariant variant;
  variant.homing[0].amount = 1;
  variant.homing[0].damage = 1;
  variant.homing[0].cooldown_ms = 100000;
  variant.homing[0].pierce = 2;
  REQUIRE(ro::sim::CheckSurvivorsRules(variant.rules).has_value());
  auto run =
      SurvivorsRun::Start(variant.rules, PillarArena(kArenaSideCells), StartWith(kTinyHomingId));
  REQUIRE(run.has_value());
  SurvivorsField& field = run->field_for_tests();
  // Deux monstres exactement superposés : un tir qui ne retiendrait que son
  // dernier coup passerait de l'un à l'autre jusqu'à épuiser son perçage.
  const std::uint32_t first = PlaceStill(field, kTinyLunaticId, East(3), kSturdyHp);
  const std::uint32_t second = PlaceStill(field, kTinyLunaticId, East(3), kSturdyHp);
  const std::uint32_t life_steps = StepsOf(2000);

  // Pas à pas jusqu'au premier coup : un seul monstre touché par pas, même
  // quand le tir en chevauche deux.
  std::uint32_t steps = 0;
  ro::sim::SurvivorsNews news;
  while (news.hits.empty()) {
    REQUIRE(steps < life_steps);
    StepStill(*run);
    ++steps;
    news = run->TakeNews();
  }
  REQUIRE(news.hits.size() == 1);
  CHECK(news.hits[0].foe_serial == first);
  REQUIRE(field.shots.size() == 1);
  CHECK(field.shots[0].hit_count == 1);
  CHECK(field.shots[0].pierce_left == 1);
  // Le pas suivant, l'autre, et lui seul.
  StepStill(*run);
  ++steps;
  news = run->TakeNews();
  REQUIRE(news.hits.size() == 1);
  CHECK(news.hits[0].foe_serial == second);
  REQUIRE(field.shots.size() == 1);
  CHECK(field.shots[0].hit_count == 2);
  CHECK(field.shots[0].pierce_left == 0);
  // Plus aucun coup jusqu'à l'expiration.
  StepStill(*run, life_steps - steps);
  REQUIRE(field.shots.size() == 1);
  CHECK(run->TakeNews().hits.empty());
  StepStill(*run);
  CHECK(field.shots.empty());
  CHECK(run->TakeNews().hits.empty());

  // La mémoire d'un tir borne son perçage.
  variant.homing[1].pierce = static_cast<std::uint8_t>(ro::sim::kMostShotHits - 1);
  CHECK(ro::sim::CheckSurvivorsRules(variant.rules).has_value());
  variant.homing[1].pierce = static_cast<std::uint8_t>(ro::sim::kMostShotHits);
  CHECK_FALSE(ro::sim::CheckSurvivorsRules(variant.rules).has_value());
}

TEST_CASE("Compétences survivors : les phases d'une orbite", "[sim][survivors]") {
  SkillVariant variant;
  // Une orbe assez large pour couvrir le monstre sur tout son tour : seule la
  // phase décide s'il est touché.
  variant.orbit[0].hit_radius_units = Cells(4);
  auto run =
      SurvivorsRun::Start(variant.rules, PillarArena(kArenaSideCells), StartWith(kTinyOrbitId));
  REQUIRE(run.has_value());
  SurvivorsField& field = run->field_for_tests();
  PlaceStill(field, kTinyLunaticId, East(1), kSturdyHp);
  const std::uint32_t active_steps = StepsOf(3000);
  const std::uint32_t rest_steps = StepsOf(3000);
  const std::uint32_t rehit_steps = StepsOf(500);
  std::vector<ro::sim::SurvivorsOrb> orbs;

  StepStill(*run);
  ro::sim::SurvivorsNews news = run->TakeNews();
  REQUIRE(news.casts.size() == 1);
  CHECK(news.casts[0].kind == CastKind::kOrbitOn);
  CHECK(news.casts[0].skill_id == kTinyOrbitId);
  CHECK(news.hits.size() == 1);

  // Orbs() rend le dernier pas exécuté : l'orbe reste visible jusqu'au pas qui
  // publie kOrbitOff, et disparaît avec lui. Toute la phase active frappe, une
  // fois par rehit_ms.
  StepStill(*run, active_steps - 1);
  run->Orbs(orbs);
  CHECK(orbs.size() == 1);
  news = run->TakeNews();
  CHECK(news.casts.empty());
  CHECK(news.hits.size() == active_steps / rehit_steps - 1);

  StepStill(*run);
  run->Orbs(orbs);
  CHECK(orbs.empty());
  news = run->TakeNews();
  REQUIRE(news.casts.size() == 1);
  CHECK(news.casts[0].kind == CastKind::kOrbitOff);
  CHECK(news.hits.empty());

  // La phase éteinte ne frappe JAMAIS, bien que l'emplacement soit prêt.
  const std::int32_t hp_at_rest = field.foes[0].hp;
  StepStill(*run, rest_steps - 1);
  news = run->TakeNews();
  CHECK(news.casts.empty());
  CHECK(news.hits.empty());
  CHECK(field.foes[0].hp == hp_at_rest);
  StepStill(*run);
  news = run->TakeNews();
  REQUIRE(news.casts.size() == 1);
  CHECK(news.casts[0].kind == CastKind::kOrbitOn);
  CHECK(news.hits.size() == 1);
}

TEST_CASE("Compétences survivors : une orbite retouche après rehit_ms", "[sim][survivors]") {
  SkillVariant variant;
  // Une orbe assez large pour couvrir le monstre sur tout son tour, sans repos.
  variant.orbit[0].hit_radius_units = Cells(4);
  variant.orbit[0].rest_ms = 0;
  variant.orbit[0].amount = 2;
  auto run =
      SurvivorsRun::Start(variant.rules, PillarArena(kArenaSideCells), StartWith(kTinyOrbitId));
  REQUIRE(run.has_value());
  SurvivorsField& field = run->field_for_tests();
  PlaceStill(field, kTinyLunaticId, East(1), kSturdyHp);
  const std::uint32_t rehit_steps = StepsOf(500);

  StepStill(*run, rehit_steps);
  // Deux orbes le chevauchent, mais l'emplacement ne le touche qu'une fois.
  CHECK(run->TakeNews().hits.size() == 1);
  CHECK(field.foes[0].rehit_ready_step[0] == rehit_steps);
  StepStill(*run);
  CHECK(run->TakeNews().hits.size() == 1);
  StepStill(*run, 2 * rehit_steps);
  CHECK(run->TakeNews().hits.size() == 2);
  CHECK(field.foes[0].hp == kSturdyHp - 4 * 10);
}

TEST_CASE("Compétences survivors : Orbs() rend le calcul de la simulation", "[sim][survivors]") {
  SurvivorsStart start = StartWith(kTinyOrbitId);
  SetBonus(start, SurvivorsStat::kAreaPct, 50);
  auto run = SurvivorsRun::Start(QuietRules(), PillarArena(kArenaSideCells), start);
  REQUIRE(run.has_value());
  SurvivorsField& field = run->field_for_tests();
  // Le niveau 2 : deux orbes, permanentes.
  field.hero.skills[0].level = 2;
  constexpr std::uint32_t kStepsPlayed = 37;
  StepStill(*run, kStepsPlayed);

  std::vector<ro::sim::SurvivorsOrb> orbs;
  run->Orbs(orbs);
  REQUIRE(orbs.size() == 2);
  const std::uint32_t period_steps = StepsOf(1600);
  const std::int32_t radius = Cells(2) * 150 / 100;
  // Le dernier pas exécuté est kStepsPlayed − 1 : celui des coups visibles.
  constexpr std::uint32_t kDrawnStep = kStepsPlayed - 1;
  for (std::uint32_t orb = 0; orb < 2; ++orb) {
    const std::uint32_t heading = (kDrawnStep * 64 / period_steps + orb * 64 / 2) % 64;
    CHECK(orbs[orb].skill_id == kTinyOrbitId);
    CHECK(orbs[orb].position ==
          field.hero.position + TurnVector(static_cast<std::uint8_t>(heading), radius));
  }
}

TEST_CASE("Compétences survivors : les dégâts", "[sim][survivors]") {
  // Le premier coup d'une frappe, au pas 1, contre un monstre posé à 3 cases.
  const auto first_hit = [](SurvivorsStart start, std::uint16_t foe_id, std::uint16_t level) {
    auto run = SurvivorsRun::Start(QuietRules(), PillarArena(kArenaSideCells), start);
    REQUIRE(run.has_value());
    SurvivorsField& field = run->field_for_tests();
    field.hero.level = level;
    PlaceStill(field, foe_id, East(3), kSturdyHp);
    StepStill(*run, 2);
    const ro::sim::SurvivorsNews news = run->TakeNews();
    REQUIRE(news.hits.size() == 1);
    CHECK(field.tally.damage_dealt == static_cast<std::uint64_t>(news.hits[0].amount));
    return news.hits[0];
  };
  SECTION("le niveau ajoute 1 % par niveau") {
    const ro::sim::SurvivorsHit hit = first_hit(TinyStart(1), kTinyLunaticId, 30);
    CHECK(hit.amount == 10 * 130 / 100);
    CHECK(hit.efficiency_pct == 100);
    CHECK(hit.skill_id == kTinyStrikeId);
  }
  SECTION("le bonus de niveau plafonne à 50 %, les cartes s'y ajoutent") {
    CHECK(first_hit(TinyStart(1), kTinyLunaticId, 80).amount == 15);
    SurvivorsStart start = TinyStart(1);
    SetBonus(start, SurvivorsStat::kDamagePct, 10);
    CHECK(first_hit(start, kTinyLunaticId, 80).amount == 16);
  }
  SECTION("l'élément : le feu contre l'eau") {
    const ro::sim::SurvivorsHit hit = first_hit(TinyStart(1), kTinyPoringId, 1);
    CHECK(hit.amount == 10 * 101 / 100 * 50 / 100);
    CHECK(hit.efficiency_pct == 50);
  }
  SECTION("au moins 1 tant que l'élément n'annule pas") {
    SurvivorsStart start = TinyStart(1);
    SetBonus(start, SurvivorsStat::kDamagePct, -99);
    CHECK(first_hit(start, kTinyLunaticId, 1).amount == 1);
    CHECK(first_hit(start, kTinyPoringId, 1).amount == 1);
  }
}

TEST_CASE("Compétences survivors : un élément qui annule les dégâts", "[sim][survivors]") {
  SkillVariant variant;
  // Le feu n'entame plus le neutre, et n'entame l'eau qu'à 1 %.
  variant.SetElementPct(SurvivorsElement::kFire, SurvivorsElement::kNeutral, 0);
  variant.SetElementPct(SurvivorsElement::kFire, SurvivorsElement::kWater, 1);
  SECTION("aucun dégât, et l'efficacité nulle publiée") {
    // L'orbite de feu, assez large pour couvrir le monstre dès le premier pas.
    variant.orbit[0].hit_radius_units = Cells(4);
    variant.orbit[0].rest_ms = 0;
    auto run =
        SurvivorsRun::Start(variant.rules, PillarArena(kArenaSideCells), StartWith(kTinyOrbitId));
    REQUIRE(run.has_value());
    SurvivorsField& field = run->field_for_tests();
    PlaceStill(field, kTinyLunaticId, East(1), kSturdyHp);
    StepStill(*run);
    const ro::sim::SurvivorsNews news = run->TakeNews();
    REQUIRE(news.hits.size() == 1);
    CHECK(news.hits[0].amount == 0);
    CHECK(news.hits[0].efficiency_pct == 0);
    CHECK(field.foes[0].hp == kSturdyHp);
    CHECK(field.tally.damage_dealt == 0U);
  }
  SECTION("le minimum de 1 tient pour un pour cent") {
    auto run = SurvivorsRun::Start(variant.rules, PillarArena(kArenaSideCells), TinyStart(1));
    REQUIRE(run.has_value());
    SurvivorsField& field = run->field_for_tests();
    PlaceStill(field, kTinyPoringId, East(3), kSturdyHp);
    StepStill(*run, 2);
    const ro::sim::SurvivorsNews news = run->TakeNews();
    REQUIRE(news.hits.size() == 1);
    CHECK(news.hits[0].amount == 1);
    CHECK(news.hits[0].efficiency_pct == 1);
  }
  SECTION("les frappes passent au monstre suivant") {
    auto run = SurvivorsRun::Start(variant.rules, PillarArena(kArenaSideCells), TinyStart(1));
    REQUIRE(run.has_value());
    SurvivorsField& field = run->field_for_tests();
    PlaceStill(field, kTinyLunaticId, East(2), kSturdyHp);
    const std::uint32_t poring = PlaceStill(field, kTinyPoringId, East(5), kSturdyHp);
    StepStill(*run);
    CHECK(StruckSerials(field) == std::vector<std::uint32_t>{poring, poring});
  }
  SECTION("sans autre monstre, le skill attend") {
    auto run = SurvivorsRun::Start(variant.rules, PillarArena(kArenaSideCells), TinyStart(1));
    REQUIRE(run.has_value());
    SurvivorsField& field = run->field_for_tests();
    PlaceStill(field, kTinyLunaticId, East(2), kSturdyHp);
    StepStill(*run, 3);
    CHECK(field.strikes.empty());
    CHECK(field.hero.skills[0].ready_step == 0U);
  }
}
