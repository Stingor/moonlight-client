// CE QUE LE CONTRÔLE ET L'EMPREINTE GARDENT DU VOLEUR : le robot qui reprend un
// joueur parti, le réflexe qui ne devient pas permanent, et le poison reposé qui
// reste au héros et au skill de son tic le plus fort.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/core/error.h"
#include "ro/sim/survivors_bot.h"
#include "ro/sim/survivors_geometry.h"
#include "ro/sim/survivors_match.h"
#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_run.h"
#include "ro/sim/survivors_state.h"

#include "support/survivors_fixture.h"

namespace {

using ro::sim::SurvivorsRun;

constexpr std::uint16_t kThiefClassId = 3;
constexpr std::uint16_t kPlainsArenaId = 2;  // la prairie générée
constexpr std::uint64_t kSeed = 20261107;

constexpr std::uint16_t kDoubleAttackId = 48;
constexpr std::uint16_t kHidingId = 51;
constexpr std::uint16_t kEnvenomId = 52;
constexpr std::uint16_t kCloakingId = 135;
constexpr std::uint16_t kVenomDustId = 140;
constexpr std::uint8_t kTopEnvenomLevel = 8;

// Un monstre de pierre, lent et neutre : le poison le touche à plein.
constexpr std::uint16_t kSteadyFoeId = 1040;
constexpr std::int32_t kSteadyFoeHp = 1000000;
constexpr std::int32_t kNearCells = 2;

// Assez de pas pour qu'un skill prêt tire et que son coup tombe.
constexpr std::uint32_t kFewSteps = 5;
// Assez pour qu'un nuage atteigne son monstre.
constexpr std::uint32_t kFlightSteps = 40;
constexpr std::uint32_t kNever = std::numeric_limits<std::uint32_t>::max();
constexpr std::int64_t kPercent = 100;

[[nodiscard]] ro::sim::SurvivorsStart ThiefStart() {
  return ro::sim::SurvivorsStart{.seed = kSeed,
                                 .class_id = kThiefClassId,
                                 .arena_id = kPlainsArenaId,
                                 .first_skill_id = kDoubleAttackId};
}

// Les PV refaits et la montée de niveau retenue : aucun réflexe, aucun choix.
void StepHealed(SurvivorsRun& run, std::uint32_t steps) {
  ro::sim::SurvivorsField& field = run.field_for_tests();
  for (std::uint32_t done = 0; done < steps; ++done) {
    field.hero.hp = field.hero.max_hp;
    for (ro::sim::SurvivorsKit& ally : field.allies) ally.hero.hp = ally.hero.max_hp;
    field.hero.exp = 0;
    field.gems.clear();
    run.Step(ro::sim::SurvivorsInput{});
  }
}

// Le skill unique `skill_id` du héros `hero`, au niveau `level`, prêt ou jamais prêt.
void HoldOnly(ro::sim::SurvivorsHero& hero, std::uint16_t skill_id, std::uint8_t level,
              std::uint32_t ready_step) {
  hero.skills.clear();
  hero.skills.push_back(
      ro::sim::OwnedSkill{.skill_id = skill_id, .level = level, .ready_step = ready_step});
}

// Un monstre robuste et immobile, à deux cases à l'est de `near`.
[[nodiscard]] std::uint32_t PlaceSteadyFoe(ro::sim::SurvivorsField& field,
                                           ro::sim::FieldPoint near) {
  const std::uint32_t serial = ro::test_support::PlaceFoe(
      field, kSteadyFoeId, near + ro::sim::FieldPoint{ro::sim::Cells(kNearCells), 0});
  REQUIRE(serial != 0U);
  ro::sim::SurvivorsFoe& foe = field.foes.back();
  foe.units_per_step = 0;
  foe.hp = kSteadyFoeHp;
  foe.max_hp = kSteadyFoeHp;
  return serial;
}

[[nodiscard]] const ro::sim::SurvivorsFoe& FoeOf(const ro::sim::SurvivorsField& field,
                                                 std::uint32_t serial) {
  const auto found = std::ranges::find(field.foes, serial, &ro::sim::SurvivorsFoe::serial);
  REQUIRE(found != field.foes.end());
  return *found;
}

// Les règles livrées, le niveau `level_index` du réflexe `skill_id` rechargé en
// `cooldown_ms`.
struct ReflexVariant {
  std::vector<ro::sim::SurvivorsSkillRule> skills;
  std::vector<ro::sim::SkillLevel> levels;
  ro::sim::SurvivorsRules rules;
};

[[nodiscard]] ReflexVariant WithReflexCooldown(std::uint16_t skill_id, std::size_t level_index,
                                               std::uint32_t cooldown_ms) {
  const ro::sim::SurvivorsRules& shipped = ro::sim::ShippedSurvivorsRules();
  ReflexVariant variant{
      .skills = {shipped.skills.begin(), shipped.skills.end()}, .levels = {}, .rules = shipped};
  const auto reflex =
      std::ranges::find(variant.skills, skill_id, &ro::sim::SurvivorsSkillRule::skill_id);
  REQUIRE(reflex != variant.skills.end());
  REQUIRE(reflex->archetype == ro::sim::SkillArchetype::kReflex);
  variant.levels.assign(reflex->levels.begin(), reflex->levels.end());
  REQUIRE(level_index < variant.levels.size());
  variant.levels[level_index].cooldown_ms = cooldown_ms;
  reflex->levels = variant.levels;
  variant.rules.skills = variant.skills;
  return variant;
}

}  // namespace

TEST_CASE("Robot survivors : son repli entre dans l'empreinte des règles", "[sim][survivors]") {
  const ro::sim::SurvivorsBotProfile& thief = ro::sim::ThiefBotProfile();
  REQUIRE(thief.retreat_below_pct > 0);
  const std::uint64_t shipped = ro::sim::SurvivorsBotDigest(thief);
  ro::sim::SurvivorsBotProfile later = thief;
  later.retreat_below_pct += 1;
  CHECK(ro::sim::SurvivorsBotDigest(later) != shipped);
  ro::sim::SurvivorsBotProfile farther = thief;
  farther.retreat_radius_units += 1;
  CHECK(ro::sim::SurvivorsBotDigest(farther) != shipped);
}

TEST_CASE("Réflexe survivors : sa recharge au plus réduit dépasse sa durée", "[sim][survivors]") {
  const ro::sim::SurvivorsRules& shipped = ro::sim::ShippedSurvivorsRules();
  REQUIRE(ro::sim::CheckSurvivorsRules(shipped).has_value());
  for (const std::uint16_t skill_id : {kHidingId, kCloakingId}) {
    INFO("skill " << skill_id);
    const ro::sim::SurvivorsSkillRule* reflex = ro::sim::FindSkill(shipped, skill_id);
    REQUIRE(reflex != nullptr);
    const std::size_t top = reflex->levels.size() - 1;
    const std::uint32_t life_ms = reflex->levels[top].life_ms;
    // Au plus réduit, la recharge rendrait exactement la durée : le réflexe repartirait
    // à l'instant où il s'éteint.
    const auto endless_ms = static_cast<std::uint32_t>(
        std::int64_t{life_ms} * kPercent / (kPercent - shipped.tuning.most_cooldown_pct));
    const ReflexVariant endless = WithReflexCooldown(skill_id, top, endless_ms);
    CHECK_FALSE(ro::sim::CheckSurvivorsRules(endless.rules).has_value());
    // Plus courte que la durée, même sans réduction.
    const ReflexVariant shorter = WithReflexCooldown(skill_id, 0, life_ms);
    CHECK_FALSE(ro::sim::CheckSurvivorsRules(shorter.rules).has_value());
  }
}

TEST_CASE("Poison survivors : reposé plus faible, il garde son skill", "[sim][survivors]") {
  const ro::sim::SurvivorsRules& rules = ro::sim::ShippedSurvivorsRules();
  auto started = SurvivorsRun::Start(rules, ThiefStart());
  REQUIRE(started.has_value());
  SurvivorsRun run = std::move(*started);
  ro::sim::SurvivorsField& field = run.field_for_tests();
  field.foes.clear();
  const std::uint32_t serial = PlaceSteadyFoe(field, field.hero.position);
  const std::int32_t dust_tick =
      ro::sim::FindSkill(rules, kVenomDustId)->levels.front().poison_tick_damage;

  SECTION("plus faible, le nuage le prolonge sans le reprendre") {
    HoldOnly(field.hero, kEnvenomId, kTopEnvenomLevel, field.step);
    StepHealed(run, kFewSteps);
    const std::int32_t strong_tick = FoeOf(field, serial).poison_tick_damage;
    REQUIRE(FoeOf(field, serial).poison_skill_id == kEnvenomId);
    REQUIRE(dust_tick < strong_tick);
    const std::uint32_t until_step = FoeOf(field, serial).poison_until_step;
    HoldOnly(field.hero, kVenomDustId, 1, field.step);
    StepHealed(run, kFlightSteps);
    CHECK(FoeOf(field, serial).poison_until_step >= until_step);
    CHECK(FoeOf(field, serial).poison_tick_damage == strong_tick);
    CHECK(FoeOf(field, serial).poison_skill_id == kEnvenomId);
  }

  SECTION("aussi fort, il passe au skill qui le repose") {
    HoldOnly(field.hero, kEnvenomId, 1, field.step);
    StepHealed(run, kFewSteps);
    REQUIRE(FoeOf(field, serial).poison_skill_id == kEnvenomId);
    REQUIRE(FoeOf(field, serial).poison_tick_damage == dust_tick);
    HoldOnly(field.hero, kVenomDustId, 1, field.step);
    StepHealed(run, kFlightSteps);
    CHECK(FoeOf(field, serial).poison_skill_id == kVenomDustId);
  }
}

TEST_CASE("Poison survivors : en coop, reposé plus faible, il reste à son héros",
          "[sim][survivors]") {
  const ro::sim::SurvivorsRules& rules = ro::sim::ShippedSurvivorsRules();
  ro::sim::SurvivorsStart start = ThiefStart();
  start.allies.push_back(
      ro::sim::SurvivorsAllyStart{.class_id = kThiefClassId, .first_skill_id = kEnvenomId});
  auto started = SurvivorsRun::Start(rules, start);
  REQUIRE(started.has_value());
  SurvivorsRun run = std::move(*started);
  ro::sim::SurvivorsField& field = run.field_for_tests();
  field.foes.clear();
  ro::sim::SurvivorsHero& ally = field.allies.front().hero;
  const std::uint32_t serial = PlaceSteadyFoe(field, ally.position);
  field.hero.position = ally.position;

  // Le héros 0 pose le tic le plus fort ; le héros 1, plus faible, ne le reprend pas.
  HoldOnly(field.hero, kEnvenomId, kTopEnvenomLevel, field.step);
  HoldOnly(ally, kEnvenomId, 1, kNever);
  StepHealed(run, kFewSteps);
  REQUIRE(FoeOf(field, serial).poison_until_step > 0U);
  REQUIRE(FoeOf(field, serial).poison_hero == 0U);
  field.hero.skills.front().ready_step = kNever;
  ally.skills.front().ready_step = field.step;
  StepHealed(run, kFewSteps);
  CHECK(FoeOf(field, serial).poison_hero == 0U);

  // Aussi fort, il passe au héros qui le repose.
  ally.skills.front().level = kTopEnvenomLevel;
  ally.skills.front().ready_step = field.step;
  StepHealed(run, kFewSteps);
  CHECK(FoeOf(field, serial).poison_hero == 1U);
}
