// La relève d'une vie de réserve se voit : l'effet de Resurrection sur le héros
// qui se relève, aux pieds d'un allié, et même en effets réduits.

#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/survivors_content.h"
#include "ro/sim/survivors_news.h"
#include "ro/sim/survivors_state.h"

#include "survivors/arena_effect_plan.h"
#include "survivors/survivors_looks.h"

namespace {

[[nodiscard]] ro::sim::SurvivorsNews RevivedAt(ro::sim::FieldPoint at) {
  ro::sim::SurvivorsNews news;
  news.revives.push_back(ro::sim::SurvivorsReviveNews{.at = at, .lives_left = 0});
  return news;
}

[[nodiscard]] std::vector<moonclient::ArenaLaunch> PlanOf(
    const ro::sim::SurvivorsNews& news, const ro::sim::SurvivorsField& field,
    moonclient::ArenaEffectSwitches switches) {
  const moonclient::ArenaSkillLooks looks(ro::sim::ShippedSurvivorsRules());
  moonclient::ArenaEffectBudget budget;
  std::vector<moonclient::ArenaLaunch> plan;
  budget.BeginFrame(0);
  moonclient::PlanArenaLaunches(news, field, looks, switches, {}, budget, plan);
  return plan;
}

}  // namespace

TEST_CASE("Relève : l'effet de Resurrection sur le héros, même en effets réduits",
          "[app][survivors]") {
  ro::sim::SurvivorsField field;
  field.hero.position = ro::sim::FieldPoint{4'000, 7'000};
  for (const moonclient::ArenaEffectSwitches switches :
       {moonclient::ArenaEffectSwitches{}, moonclient::ArenaEffectSwitches{.reduced = true}}) {
    const std::vector<moonclient::ArenaLaunch> plan =
        PlanOf(RevivedAt(field.hero.position), field, switches);
    REQUIRE(plan.size() == 1U);
    CHECK(plan[0].effect_id == moonclient::kReviveEffect);
    CHECK(plan[0].kind == moonclient::ArenaLaunchKind::kOnHero);
  }
  // Sans aucun effet, rien.
  CHECK(PlanOf(RevivedAt(field.hero.position), field, {.shown = false}).empty());
}

TEST_CASE("Relève : celle d'un allié naît à ses pieds", "[app][survivors]") {
  ro::sim::SurvivorsField field;
  field.hero.position = ro::sim::FieldPoint{4'000, 7'000};
  const ro::sim::FieldPoint ally_at{9'000, 2'000};
  const std::vector<moonclient::ArenaLaunch> plan = PlanOf(RevivedAt(ally_at), field, {});
  REQUIRE(plan.size() == 1U);
  CHECK(plan[0].effect_id == moonclient::kReviveEffect);
  CHECK(plan[0].kind == moonclient::ArenaLaunchKind::kOnGround);
  CHECK(plan[0].at == ally_at);
}
