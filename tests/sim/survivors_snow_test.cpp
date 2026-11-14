// LES NEIGES DE LUTIE ET LE GEL : l'arène 4, son terrain et son contenu ; le
// séisme qui gèle le héros qu'il blesse, sans jamais le figer.

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string_view>
#include <utility>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/survivors_content.h"
#include "ro/sim/survivors_geometry.h"
#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_run.h"
#include "ro/sim/survivors_state.h"
#include "ro/sim/survivors_terrain.h"

#include "support/survivors_fixture.h"

namespace {

using ro::sim::SurvivorsRun;

constexpr std::uint16_t kSnowArenaId = 4;
constexpr std::uint16_t kSnowTerrainId = 3;
constexpr std::uint16_t kMageClassId = 1;
constexpr std::uint16_t kFireBoltId = 19;
constexpr std::uint32_t kSeed = 2026;
constexpr std::uint16_t kHatiiId = 1252;
constexpr std::uint16_t kStormyKnightId = 1251;
constexpr std::size_t kSnowBossCount = 6;
constexpr std::size_t kSnowEventCount = 7;
constexpr std::int32_t kSlowPct = 40;
constexpr std::uint32_t kSlowMs = 2000;
constexpr std::int32_t kWeakerSlowPct = 20;
constexpr std::int32_t kPercent = 100;
constexpr std::int32_t kBlowDamage = 5;
constexpr std::int32_t kHazardRadiusUnits = ro::sim::Cells(1);
constexpr std::uint32_t kLongSteps = 1000;
// Le cap 0 : plein nord.
constexpr std::uint8_t kNorth = 0;

[[nodiscard]] const ro::sim::SurvivorsRules& Rules() { return ro::sim::ShippedSurvivorsRules(); }

// Un héros sans skill ni monstre, sur la neige, qu'aucun coup n'esquive.
[[nodiscard]] SurvivorsRun BareHero() {
  auto run = SurvivorsRun::Start(Rules(), ro::sim::SurvivorsStart{.seed = kSeed,
                                                                  .class_id = kMageClassId,
                                                                  .arena_id = kSnowArenaId,
                                                                  .first_skill_id = kFireBoltId});
  REQUIRE(run.has_value());
  ro::sim::SurvivorsField& field = run->field_for_tests();
  field.hero.skills.clear();
  field.foes.clear();
  field.hero.stats.values[static_cast<std::size_t>(ro::sim::SurvivorsStat::kDodgePct)] = 0;
  return std::move(*run);
}

// Un cercle gelant armé sous le héros, qui frappe au prochain pas.
[[nodiscard]] ro::sim::SurvivorsHazard FrostUnder(const ro::sim::SurvivorsField& field,
                                                  std::int32_t slow_pct) {
  return ro::sim::SurvivorsHazard{.kind = ro::sim::HazardKind::kQuake,
                                  .center = field.hero.position,
                                  .radius_units = kHazardRadiusUnits,
                                  .damage = kBlowDamage,
                                  .armed_step = field.step,
                                  .until_step = field.step + kLongSteps,
                                  .slow_pct = slow_pct,
                                  .slow_ms = kSlowMs};
}

// Le pas que fait le héros vers le nord, en sous-unités.
[[nodiscard]] std::int32_t StrideNorth(SurvivorsRun& run) {
  ro::sim::SurvivorsField& field = run.field_for_tests();
  const ro::sim::FieldPoint before = field.hero.position;
  run.Step(ro::test_support::Heading(kNorth));
  return std::abs(field.hero.position.y - before.y);
}

}  // namespace

TEST_CASE("Neige survivors : l'arène 4 joue son terrain et son contenu", "[sim][survivors]") {
  const ro::sim::ArenaRule* arena = ro::sim::FindArena(Rules(), kSnowArenaId);
  REQUIRE(arena != nullptr);
  CHECK(std::string_view{arena->key} == "lutie_snowfield");
  CHECK(arena->terrain_id == kSnowTerrainId);
  // Classée, comme toute arène livrée.
  CHECK(arena->ranked);
  const ro::sim::TerrainRule* terrain = ro::sim::FindTerrain(Rules(), kSnowTerrainId);
  REQUIRE(terrain != nullptr);
  CHECK(std::string_view{terrain->key} == "lutie_snowfield");

  REQUIRE(arena->bosses.size() == kSnowBossCount);
  CHECK(arena->events.size() == kSnowEventCount);
  CHECK(arena->bosses.back().final);
  CHECK(arena->bosses.back().foe_id == kHatiiId);
  for (const ro::sim::WaveRule& wave : arena->waves) {
    for (const std::uint16_t foe_id : wave.foe_ids) CHECK(ro::sim::FindFoe(Rules(), foe_id));
  }
  // Stormy Knight et Hatii gèlent ; aucun autre boss ne le fait.
  for (const ro::sim::BossRule& boss : arena->bosses) {
    bool freezes = false;
    for (const ro::sim::BossMoveRule& move : boss.moves) freezes = freezes || move.slow_pct > 0;
    CHECK(freezes == (boss.foe_id == kStormyKnightId || boss.foe_id == kHatiiId));
  }
}

TEST_CASE("Neige survivors : le gel ralentit le héros que le cercle blesse", "[sim][survivors]") {
  SurvivorsRun run = BareHero();
  ro::sim::SurvivorsField& field = run.field_for_tests();
  const std::int32_t free_stride = StrideNorth(run);
  REQUIRE(free_stride > 0);

  field.hazards.push_back(FrostUnder(field, kSlowPct));
  run.Step(ro::sim::SurvivorsInput{});
  CHECK(field.hero.frost_slow_pct == kSlowPct);
  REQUIRE(field.hero.frost_until_step > field.step);

  SECTION("gelé, il marche moins vite ; dégelé, il retrouve son pas") {
    const std::int32_t frozen_stride = StrideNorth(run);
    CHECK(frozen_stride == free_stride * (kPercent - kSlowPct) / kPercent);
    CHECK(frozen_stride > 0);
    while (field.step < field.hero.frost_until_step) run.Step(ro::sim::SurvivorsInput{});
    CHECK(StrideNorth(run) == free_stride);
  }

  SECTION("un gel plus faible ne l'adoucit pas") {
    const std::uint32_t until = field.hero.frost_until_step;
    field.hazards.clear();
    field.hero.invulnerable_until_step = 0;
    field.hazards.push_back(FrostUnder(field, kWeakerSlowPct));
    run.Step(ro::sim::SurvivorsInput{});
    CHECK(field.hero.frost_slow_pct == kSlowPct);
    CHECK(field.hero.frost_until_step >= until);
  }
}

TEST_CASE("Neige survivors : un coup esquivé ne gèle pas", "[sim][survivors]") {
  SurvivorsRun run = BareHero();
  ro::sim::SurvivorsField& field = run.field_for_tests();
  field.hero.stats.values[static_cast<std::size_t>(ro::sim::SurvivorsStat::kDodgePct)] =
      ro::sim::kWholeChancePct;
  field.hazards.push_back(FrostUnder(field, kSlowPct));
  run.Step(ro::sim::SurvivorsInput{});
  CHECK(field.hero.frost_until_step <= field.step);
}
