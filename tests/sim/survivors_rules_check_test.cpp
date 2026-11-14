#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

#include "ro/core/error.h"
#include "ro/sim/survivors_geometry.h"
#include "ro/sim/survivors_rules.h"

#include "support/survivors_fixture.h"

using ro::sim::CardRule;
using ro::sim::Cells;
using ro::sim::CheckSurvivorsRules;
using ro::sim::ClassRule;
using ro::sim::kMostArenaSideCells;
using ro::sim::kMostStatBonus;
using ro::sim::SkillLevel;
using ro::sim::SurvivorsRules;
using ro::sim::SurvivorsSkillRule;
using ro::sim::SurvivorsTuning;
using ro::sim::WaveRule;

namespace {

// LES BORNES DU CATALOGUE : ce que CheckSurvivorsRules refuse pour qu'aucune
// partie ne fige, ne déborde ni ne tire dans le vide. Chaque essai part des
// règles minuscules, en retouche une valeur, et attend un refus.

[[nodiscard]] bool Refused(const SurvivorsRules& rules) {
  const ro::core::Result<void> checked = CheckSurvivorsRules(rules);
  return !checked && checked.error().code == ro::core::ErrorCode::kInvalidArgument &&
         !checked.error().detail.empty();
}

[[nodiscard]] bool Accepted(const SurvivorsRules& rules) {
  const ro::core::Result<void> checked = CheckSurvivorsRules(rules);
  INFO((checked ? std::string_view{} : std::string_view{checked.error().detail}));
  return checked.has_value();
}

constexpr std::int32_t kLargestMeasure = Cells(kMostArenaSideCells);
constexpr std::size_t kStrikeIndex = 0;
constexpr std::size_t kHomingIndex = 1;
constexpr std::size_t kOrbitIndex = 2;

// Les règles minuscules, dont un essai retouche les niveaux, la classe, la
// carte ou la vague. Les spans pointent dans l'objet : il ne se copie pas.
struct Variant {
  std::array<SkillLevel, 2> strike = ro::test_support::kTinyStrikeLevels;
  std::array<SkillLevel, 2> homing = ro::test_support::kTinyHomingLevels;
  std::array<SkillLevel, 2> orbit = ro::test_support::kTinyOrbitLevels;
  std::array<SurvivorsSkillRule, 3> skills = ro::test_support::kTinySkills;
  std::array<ClassRule, 1> classes = ro::test_support::kTinyClasses;
  std::array<std::int32_t, 2> card_levels = ro::test_support::kTinyCardLevels;
  std::array<CardRule, 1> cards = ro::test_support::kTinyCards;
  std::array<WaveRule, 1> waves = ro::test_support::kTinyWaves;
  SurvivorsRules rules = ro::test_support::TinyRules();

  Variant() {
    skills[kStrikeIndex].levels = strike;
    skills[kHomingIndex].levels = homing;
    skills[kOrbitIndex].levels = orbit;
    cards[0].per_level = card_levels;
    rules.skills = skills;
    rules.classes = classes;
    rules.cards = cards;
    rules.waves = waves;
  }
  Variant(const Variant&) = delete;
  Variant& operator=(const Variant&) = delete;
};

// Refusé après `change`, sur une variante fraîche.
[[nodiscard]] bool RefusedAfter(const std::function<void(Variant&)>& change) {
  Variant variant;
  REQUIRE(Accepted(variant.rules));
  change(variant);
  return Refused(variant.rules);
}

}  // namespace

TEST_CASE("Règles survivors : une classe qui ne joue pas est refusée", "[sim][survivors]") {
  CHECK(RefusedAfter([](Variant& v) { v.classes[0].max_hp = 0; }));
  // Une marche qui ne fait pas une sous-unité par pas fige le héros.
  CHECK(RefusedAfter([](Variant& v) { v.classes[0].walk_ms_per_cell = 0; }));
  CHECK(ro::sim::UnitsPerStep(30000) == 0);
  CHECK(RefusedAfter([](Variant& v) { v.classes[0].walk_ms_per_cell = 30000; }));
  CHECK(RefusedAfter([](Variant& v) { v.classes[0].magnet_units = -1; }));
  CHECK(RefusedAfter([](Variant& v) { v.classes[0].magnet_units = kLargestMeasure + 1; }));
  CHECK(RefusedAfter([](Variant& v) { v.classes[0].damage_pct_cap = kMostStatBonus + 1; }));
  CHECK(RefusedAfter([](Variant& v) { v.classes[0].damage_pct_per_level = -1; }));
  // Aux bornes, elle se joue.
  Variant variant;
  variant.classes[0].magnet_units = kLargestMeasure;
  variant.classes[0].damage_pct_cap = kMostStatBonus;
  CHECK(Accepted(variant.rules));
}

TEST_CASE("Règles survivors : la majoration du boss final reste dans ses bornes",
          "[sim][survivors]") {
  CHECK(RefusedAfter([](Variant& v) { v.classes[0].final_boss_hp_bonus_pct = -1; }));
  CHECK(RefusedAfter(
      [](Variant& v) { v.classes[0].final_boss_hp_bonus_pct = ro::sim::kMostStatBonus + 1; }));
  Variant variant;
  variant.classes[0].final_boss_hp_bonus_pct = ro::sim::kMostStatBonus;
  CHECK(Accepted(variant.rules));
}

TEST_CASE("Règles survivors : un niveau qui ne tire rien est refusé", "[sim][survivors]") {
  // Une frappe ou un tir sans portée ne trouve jamais de monstre.
  CHECK(RefusedAfter([](Variant& v) { v.strike[0].reach_units = 0; }));
  CHECK(RefusedAfter([](Variant& v) { v.homing[0].reach_units = 0; }));
  // 40 sous-unités par seconde : 0 par pas de 20 ms, le tir ne part jamais.
  CHECK(RefusedAfter([](Variant& v) { v.homing[0].speed_units_per_s = 40; }));
  CHECK(RefusedAfter([](Variant& v) { v.homing[0].speed_units_per_s = -50; }));
  // Une salve au-delà de kMostVolley.
  CHECK(RefusedAfter([](Variant& v) {
    v.strike[1].amount = static_cast<std::uint8_t>(ro::sim::kMostVolley + 1);
  }));
  // Portée, rayon et vitesse au-delà de la plus grande arène.
  CHECK(RefusedAfter([](Variant& v) { v.strike[0].reach_units = kLargestMeasure + 1; }));
  CHECK(RefusedAfter([](Variant& v) { v.orbit[0].reach_units = -1; }));
  CHECK(RefusedAfter([](Variant& v) { v.homing[0].hit_radius_units = kLargestMeasure + 1; }));
  CHECK(RefusedAfter([](Variant& v) {
    v.homing[0].speed_units_per_s =
        kLargestMeasure * static_cast<std::int32_t>(ro::sim::kStepsPerSecond) + 1;
  }));

  // Aux bornes : une sous-unité par pas, une salve pleine, une orbite sans
  // portée (ses orbes tournent sur le héros).
  Variant variant;
  variant.homing[0].speed_units_per_s = static_cast<std::int32_t>(ro::sim::kStepsPerSecond);
  CHECK(ro::sim::ShotUnitsPerStep(variant.homing[0]) == 1);
  variant.strike[1].amount = static_cast<std::uint8_t>(ro::sim::kMostVolley);
  variant.orbit[0].reach_units = 0;
  variant.strike[0].reach_units = kLargestMeasure;
  CHECK(Accepted(variant.rules));
}

TEST_CASE("Règles survivors : une carte ou une vague hors bornes est refusée", "[sim][survivors]") {
  CHECK(RefusedAfter([](Variant& v) { v.card_levels[1] = kMostStatBonus + 1; }));
  CHECK(RefusedAfter([](Variant& v) { v.card_levels[0] = -kMostStatBonus - 1; }));
  // Un lot sans intervalle naîtrait à chaque pas.
  CHECK(RefusedAfter([](Variant& v) { v.waves[0].interval_ms = 0; }));
  Variant variant;
  variant.card_levels[0] = -kMostStatBonus;
  variant.card_levels[1] = kMostStatBonus;
  // Sans lot, l'intervalle nul ne fait naître personne : valide.
  variant.waves[0].batch = 0;
  variant.waves[0].interval_ms = 0;
  CHECK(Accepted(variant.rules));
}

TEST_CASE("Règles survivors : des réglages hors bornes sont refusés", "[sim][survivors]") {
  // Sans rayon, une gemme tirée sur le héros n'est jamais ramassée.
  CHECK(RefusedAfter([](Variant& v) { v.rules.tuning.hero_radius_units = 0; }));
  // Sans essai d'apparition, rien ne naît, pas même un boss.
  CHECK(RefusedAfter([](Variant& v) { v.rules.tuning.spawn_tries_per_foe = 0; }));
  // Chaque mesure en sous-unités dans [0, Cells(kMostArenaSideCells)].
  using Measure = std::int32_t SurvivorsTuning::*;
  constexpr std::array<Measure, 8> kMeasures{
      &SurvivorsTuning::hero_radius_units,         &SurvivorsTuning::touch_reach_units,
      &SurvivorsTuning::direct_chase_units,        &SurvivorsTuning::recall_units,
      &SurvivorsTuning::separation_max_push_units, &SurvivorsTuning::gem_pull_start_units,
      &SurvivorsTuning::gem_pull_accel_units,      &SurvivorsTuning::flow_radius_cells,
  };
  for (const Measure measure : kMeasures) {
    const std::int32_t most =
        measure == &SurvivorsTuning::flow_radius_cells ? kMostArenaSideCells : kLargestMeasure;
    CHECK(RefusedAfter([measure, most](Variant& v) { v.rules.tuning.*measure = most + 1; }));
    CHECK(RefusedAfter([measure](Variant& v) { v.rules.tuning.*measure = -1; }));
    Variant variant;
    variant.rules.tuning.*measure = most;
    CHECK(Accepted(variant.rules));
  }
  for (std::size_t size = 0; size < ro::sim::kFoeSizeCount; ++size) {
    CHECK(RefusedAfter(
        [size](Variant& v) { v.rules.tuning.foe_radius_units[size] = kLargestMeasure + 1; }));
    CHECK(RefusedAfter([size](Variant& v) { v.rules.tuning.foe_radius_units[size] = -1; }));
  }
  CHECK(
      RefusedAfter([](Variant& v) { v.rules.tuning.spawn_search_ring = kMostArenaSideCells + 1; }));
  CHECK(RefusedAfter([](Variant& v) { v.rules.tuning.spawn_search_ring = -1; }));
  // La croissance des PV dans [0, kMostStatBonus] : le calcul d'une naissance
  // tient alors en 64 bits.
  CHECK(RefusedAfter([](Variant& v) { v.rules.tuning.foe_hp_growth_pct_per_minute = -1; }));
  CHECK(RefusedAfter(
      [](Variant& v) { v.rules.tuning.foe_hp_growth_pct_per_minute = kMostStatBonus + 1; }));
  Variant variant;
  variant.rules.tuning.foe_hp_growth_pct_per_minute = kMostStatBonus;
  CHECK(Accepted(variant.rules));
}

TEST_CASE("Règles survivors : les réglages livrés, champ par champ", "[sim][survivors]") {
  // Un équilibrage qui change ces valeurs monte kSurvivorsRulesetVersion, que
  // la partie en or épingle avec son bilan. Les réglages du contenu d'une
  // arène complète : survivors_catalogue_test.cpp.
  const SurvivorsTuning& tuning = ro::sim::ShippedSurvivorsRules().tuning;
  CHECK(tuning.foe_walk_scale_pct == 180);
  CHECK(tuning.foe_hp_growth_pct_per_minute == 4);
  CHECK(tuning.chill_slow_pct == 30);
  CHECK(tuning.boss_chill_share_pct == 50);
  CHECK(tuning.hero_radius_units == 409);
  CHECK(tuning.foe_radius_units == std::array<std::int32_t, 3>{358, 460, 716});
  CHECK(tuning.touch_reach_units == 128);
  CHECK(tuning.direct_chase_units == Cells(3));
  CHECK(tuning.recall_units == Cells(48));
  CHECK(tuning.flow_radius_cells == 44);
  CHECK(tuning.flow_refresh_steps == 5U);
  CHECK(tuning.separation_max_push_units == 128);
  CHECK(tuning.hero_shove_pct == 20);
  CHECK(tuning.gem_pull_start_units == 64);
  CHECK(tuning.gem_pull_accel_units == 16);
  CHECK(tuning.offer_weight_owned == 100U);
  CHECK(tuning.offer_weight_fresh == 25U);
  CHECK(tuning.heal_offer_hp == 30);
  CHECK(tuning.zeny_offer == 25U);
  CHECK(tuning.most_floor_spawns_per_step == 10);
  CHECK(tuning.spawn_tries_per_foe == 4);
  CHECK(tuning.spawn_search_ring == 4);
  CHECK(tuning.most_cooldown_pct == 60);
  CHECK(tuning.stance_least_rest_ms == 2000U);
  CHECK(tuning.plant_kills_per_minute == 10U);
  CHECK(tuning.plant_start_count == 5);
  CHECK(tuning.plant_start_cells == 12);
  CHECK(tuning.most_live_plants == 0);
  CHECK(tuning.plant_forget_units == Cells(48));
}
