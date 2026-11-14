// L'ORAGE ET SES STATUTS : une aire posée sur la foule la plus dense, ou sur
// le héros, qui frappe tout son disque par vagues ; elle gèle à partir d'une
// vague, aveugle et maudit. Les boss ne subissent ni l'un ni l'autre.

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>

#include <catch2/catch_test_macros.hpp>

#include "ro/core/error.h"
#include "ro/sim/survivors_geometry.h"
#include "ro/sim/survivors_news.h"
#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_run.h"
#include "ro/sim/survivors_state.h"

#include "support/survivors_content_fixture.h"
#include "support/survivors_fixture.h"

using ro::sim::CastKind;
using ro::sim::Cells;
using ro::sim::FieldPoint;
using ro::sim::SkillArchetype;
using ro::sim::SurvivorsElement;
using ro::sim::SurvivorsField;
using ro::sim::SurvivorsFoe;
using ro::sim::SurvivorsRun;
using ro::sim::TargetRule;
using ro::test_support::ContentTables;
using ro::test_support::kContentBaphometId;
using ro::test_support::kContentNovaId;
using ro::test_support::kContentNovaIndex;
using ro::test_support::kTinyLunaticId;
using ro::test_support::PillarArena;
using ro::test_support::PlaceFoe;
using ro::test_support::StepStill;
using ro::test_support::TinyStart;

namespace {

constexpr std::int32_t kArenaSideCells = 64;
constexpr std::int32_t kSturdyHp = 1000;
constexpr std::int32_t kBossHp = 50000;
constexpr std::size_t kBaphometIndex = 11;
constexpr std::int32_t kStormDamage = 10;
constexpr std::uint32_t kWaveMs = 500;
constexpr std::uint32_t kWaveSteps = kWaveMs / ro::sim::kSurvivorsStepMs;
constexpr std::uint32_t kStatusMs = 3000;
constexpr std::int64_t kWholePct = 100;
// La majoration de dégâts d'un héros de niveau 1 des règles minuscules.
constexpr std::int64_t kLevelOneBonusPct = 1;

// Les dégâts qu'un héros de niveau 1 inflige pour `base`, à 100 %.
[[nodiscard]] std::int32_t Expected(std::int64_t base) {
  return static_cast<std::int32_t>(base * (kWholePct + kLevelOneBonusPct) / kWholePct);
}

// Les règles de contenu, silencieuses, où le skill de nova devient un orage.
struct StormContent : ContentTables {
  StormContent(TargetRule target, ro::sim::SkillLevel storm) {
    rules.waves = ro::test_support::kQuietWaves;
    rules.events = {};
    rules.bosses = {};
    rules.elites = {};
    rules.plants = {};
    // Les réglages des statuts du ruleset livré.
    const ro::sim::SurvivorsTuning& shipped = ro::sim::ShippedSurvivorsRules().tuning;
    rules.tuning.curse_slow_pct = shipped.curse_slow_pct;
    rules.tuning.curse_hurt_pct = shipped.curse_hurt_pct;
    rules.tuning.blind_turn_ms = shipped.blind_turn_ms;
    // Le boss sans aucun trait : son rang seul le protège.
    foes[kBaphometIndex].behaviour.traits = 0;
    nova = storm;
    skills[kContentNovaIndex].archetype = SkillArchetype::kStorm;
    skills[kContentNovaIndex].target = target;
    skills[kContentNovaIndex].element = SurvivorsElement::kWater;
  }
};

[[nodiscard]] SurvivorsRun StartRun(const ro::sim::SurvivorsRules& rules) {
  auto run = SurvivorsRun::Start(rules, PillarArena(kArenaSideCells), TinyStart(1));
  REQUIRE(run.has_value());
  return std::move(*run);
}

void GiveStorm(SurvivorsField& field) {
  field.hero.skills = {ro::sim::OwnedSkill{
      .skill_id = kContentNovaId, .level = 1, .ready_step = field.step, .phase_step = field.step}};
}

std::uint32_t PlaceStill(SurvivorsField& field, std::uint16_t foe_id, FieldPoint offset,
                         std::int32_t hp = kSturdyHp) {
  const std::uint32_t serial = PlaceFoe(field, foe_id, field.hero.position + offset);
  SurvivorsFoe& foe = field.foes.back();
  foe.units_per_step = 0;
  foe.hp = hp;
  foe.max_hp = hp;
  return serial;
}

[[nodiscard]] const SurvivorsFoe& FoeOf(const SurvivorsField& field, std::uint32_t serial) {
  const auto found = std::find_if(field.foes.begin(), field.foes.end(),
                                  [&](const SurvivorsFoe& foe) { return foe.serial == serial; });
  REQUIRE(found != field.foes.end());
  return *found;
}

[[nodiscard]] std::size_t CountCasts(const ro::sim::SurvivorsNews& news, CastKind kind) {
  return static_cast<std::size_t>(
      std::count_if(news.casts.begin(), news.casts.end(),
                    [&](const ro::sim::SurvivorsCast& cast) { return cast.kind == kind; }));
}

// Quatre vagues sur le plus dense, qui gèlent à partir de la troisième.
constexpr ro::sim::SkillLevel kGustLevel{.damage = kStormDamage,
                                         .cooldown_ms = 10000,
                                         .amount = 4,
                                         .interval_ms = kWaveMs,
                                         .reach_units = Cells(9),
                                         .area_radius_units = Cells(2),
                                         .freeze_ms = 1000,
                                         .freeze_from_wave = 3};

// Deux vagues sur le héros, qui aveuglent et maudissent.
constexpr ro::sim::SkillLevel kVermilionLevel{.damage = kStormDamage,
                                              .cooldown_ms = 10000,
                                              .amount = 2,
                                              .interval_ms = kWaveMs,
                                              .area_radius_units = Cells(3),
                                              .blind_ms = kStatusMs,
                                              .curse_ms = kStatusMs};

}  // namespace

TEST_CASE("Orage survivors : sur la foule la plus dense, il gèle à sa troisième vague",
          "[sim][survivors]") {
  StormContent tables(TargetRule::kDensest, kGustLevel);
  SurvivorsRun run = StartRun(tables.rules);
  SurvivorsField& field = run.field_for_tests();
  GiveStorm(field);
  const FieldPoint crowd{Cells(5), 0};
  const std::uint32_t center = PlaceStill(field, kTinyLunaticId, crowd);
  const std::uint32_t above = PlaceStill(field, kTinyLunaticId, crowd + FieldPoint{0, Cells(1)});
  const std::uint32_t below = PlaceStill(field, kTinyLunaticId, crowd - FieldPoint{0, Cells(1)});
  const std::uint32_t lone = PlaceStill(field, kTinyLunaticId, FieldPoint{-Cells(3), 0});

  // Posé au premier pas, sa première vague tombe au suivant.
  StepStill(run, 2);
  const ro::sim::SurvivorsNews news = run.TakeNews();
  CHECK(CountCasts(news, CastKind::kStormOn) == 1);
  REQUIRE(field.zones.size() == 1);
  CHECK(field.zones[0].from == field.hero.position + crowd);
  for (const std::uint32_t serial : {center, above, below}) {
    CHECK(FoeOf(field, serial).hp == kSturdyHp - Expected(kStormDamage));
    CHECK(FoeOf(field, serial).freeze_until_step == 0U);
  }
  CHECK(FoeOf(field, lone).hp == kSturdyHp);

  // La deuxième ne gèle pas encore ; la troisième gèle.
  StepStill(run, kWaveSteps);
  CHECK(FoeOf(field, center).hp == kSturdyHp - 2 * Expected(kStormDamage));
  CHECK(FoeOf(field, center).freeze_until_step == 0U);
  StepStill(run, kWaveSteps);
  CHECK(FoeOf(field, center).freeze_until_step > field.step);
  CHECK(FoeOf(field, lone).freeze_until_step == 0U);

  // La quatrième, la dernière, l'éteint.
  StepStill(run, kWaveSteps);
  CHECK(field.zones.empty());
  CHECK(CountCasts(run.TakeNews(), CastKind::kZoneOff) == 1);
}

TEST_CASE("Orage survivors : sur le héros, il aveugle et maudit, mais pas le boss",
          "[sim][survivors]") {
  StormContent tables(TargetRule::kHero, kVermilionLevel);
  SurvivorsRun run = StartRun(tables.rules);
  SurvivorsField& field = run.field_for_tests();
  GiveStorm(field);
  const std::uint32_t common = PlaceStill(field, kTinyLunaticId, FieldPoint{Cells(2), 0});
  const std::uint32_t boss =
      PlaceStill(field, kContentBaphometId, FieldPoint{0, Cells(2)}, kBossHp);

  StepStill(run, 2);
  REQUIRE(field.zones.size() == 1);
  CHECK(field.zones[0].from == field.hero.position);
  CHECK(FoeOf(field, common).blind_until_step > field.step);
  CHECK(FoeOf(field, common).curse_until_step > field.step);
  CHECK(FoeOf(field, boss).blind_until_step == 0U);
  CHECK(FoeOf(field, boss).curse_until_step == 0U);
  CHECK(FoeOf(field, common).hp == kSturdyHp - Expected(kStormDamage));

  // LA SECONDE VAGUE TROUVE UN MAUDIT, qui prend sa part de plus.
  StepStill(run, kWaveSteps);
  const std::int64_t cursed_hurt_pct = tables.rules.tuning.curse_hurt_pct;
  const std::int32_t cursed = static_cast<std::int32_t>(std::int64_t{Expected(kStormDamage)} *
                                                        (kWholePct + cursed_hurt_pct) / kWholePct);
  CHECK(cursed > Expected(kStormDamage));
  CHECK(FoeOf(field, common).hp == kSturdyHp - Expected(kStormDamage) - cursed);
}

TEST_CASE("Orage survivors : un aveuglé erre au lieu de poursuivre", "[sim][survivors]") {
  // Le même monstre, aveuglé ou non : sa marche diverge dès l'orage.
  constexpr std::uint32_t kWanderSteps = 20;
  std::array<FieldPoint, 2> ends{};
  for (const bool blinded : {false, true}) {
    ro::sim::SkillLevel level = kVermilionLevel;
    level.blind_ms = blinded ? kStatusMs : 0;
    level.curse_ms = 0;
    StormContent tables(TargetRule::kHero, level);
    SurvivorsRun run = StartRun(tables.rules);
    SurvivorsField& field = run.field_for_tests();
    GiveStorm(field);
    const std::uint32_t serial =
        PlaceFoe(field, kTinyLunaticId, field.hero.position + FieldPoint{Cells(3), 0});
    field.foes.back().hp = kSturdyHp;
    field.foes.back().max_hp = kSturdyHp;
    StepStill(run, kWanderSteps);
    ends[blinded ? 1 : 0] = FoeOf(field, serial).position;
  }
  CHECK(ends[0] != ends[1]);
}

TEST_CASE("Orage survivors : le contrôle des règles", "[sim][survivors]") {
  const auto refused = [](TargetRule target, ro::sim::SkillLevel level,
                          SkillArchetype archetype = SkillArchetype::kStorm) {
    StormContent tables(target, level);
    tables.skills[kContentNovaIndex].archetype = archetype;
    return !ro::sim::CheckSurvivorsRules(tables.rules).has_value();
  };
  CHECK_FALSE(refused(TargetRule::kDensest, kGustLevel));
  CHECK_FALSE(refused(TargetRule::kHero, kVermilionLevel));
  // Plusieurs vagues sans intervalle.
  ro::sim::SkillLevel no_interval = kGustLevel;
  no_interval.interval_ms = 0;
  CHECK(refused(TargetRule::kDensest, no_interval));
  // Le gel à partir d'une vague qui ne tombe jamais.
  ro::sim::SkillLevel late_freeze = kGustLevel;
  late_freeze.freeze_from_wave = 5;
  CHECK(refused(TargetRule::kDensest, late_freeze));
  // Un orage visé sans portée.
  ro::sim::SkillLevel no_reach = kGustLevel;
  no_reach.reach_units = 0;
  CHECK(refused(TargetRule::kDensest, no_reach));
  // Le héros comme cible d'autre chose qu'un orage ; un éclat tiré au hasard.
  // Un orage, lui, se tire au hasard.
  CHECK(refused(TargetRule::kHero, ro::test_support::kContentNovaLevel, SkillArchetype::kNova));
  CHECK(refused(TargetRule::kRandom, kGustLevel, SkillArchetype::kClusterBlast));
  CHECK_FALSE(refused(TargetRule::kRandom, kGustLevel));
}

TEST_CASE("Orage survivors : au hasard, il se pose à mi-distance et blesse qui y entre",
          "[sim][survivors]") {
  ro::sim::SkillLevel spikes = kGustLevel;
  spikes.freeze_ms = 0;
  spikes.freeze_from_wave = 0;
  StormContent tables(TargetRule::kRandom, spikes);
  SurvivorsRun run = StartRun(tables.rules);
  SurvivorsField& field = run.field_for_tests();
  GiveStorm(field);
  // La portée est de neuf cases : un monstre à deux cases est trop près, le
  // seul au-delà de la moitié porte l'orage.
  const std::uint32_t near = PlaceStill(field, kTinyLunaticId, FieldPoint{Cells(2), 0});
  const FieldPoint far_offset{0, Cells(7)};
  const std::uint32_t far = PlaceStill(field, kTinyLunaticId, far_offset);
  // Hors de portée : il ne peut pas porter l'orage, il y entrera.
  const std::uint32_t walker = PlaceStill(field, kTinyLunaticId, FieldPoint{-Cells(12), 0});

  StepStill(run, 2);
  REQUIRE(field.zones.size() == 1);
  CHECK(field.zones[0].from == field.hero.position + far_offset);
  CHECK(FoeOf(field, far).hp == kSturdyHp - Expected(kStormDamage));
  CHECK(FoeOf(field, near).hp == kSturdyHp);
  CHECK(FoeOf(field, walker).hp == kSturdyHp);

  // Un monstre qui entre dans le champ prend la vague suivante.
  for (SurvivorsFoe& foe : field.foes) {
    if (foe.serial != walker) continue;
    foe.position = field.hero.position + far_offset + FieldPoint{Cells(1), 0};
    foe.previous_position = foe.position;
  }
  StepStill(run, kWaveSteps);
  CHECK(FoeOf(field, walker).hp == kSturdyHp - Expected(kStormDamage));
  CHECK(FoeOf(field, far).hp == kSturdyHp - 2 * Expected(kStormDamage));
}
