// LE BUDGET DES EFFETS DE L'ARÈNE : au-delà du plafond des lancers d'une image,
// ou de celui des effets vivants dans le champ, les suivants sont comptés
// « sautés » ; une nouvelle se lit une fois, et un visuel sauté ne revient pas.
// Au plafond, le héros passe devant les impacts, et l'impact répété part le
// dernier ; ce qui naîtrait hors du cadre vu ne part pas et ne coûte rien.
// Le cône part du héros en éventail sur son ouverture.

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/survivors_news.h"
#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_run.h"

#include "survivors/arena_effect_plan.h"

namespace {

constexpr std::uint16_t kFireBoltSkill = 19;
constexpr std::uint16_t kSightSkill = 10;
constexpr std::uint16_t kFireBallSkill = 17;
constexpr std::size_t kManyHits = 30;

[[nodiscard]] ro::sim::SurvivorsNews HitsNews(std::size_t count) {
  ro::sim::SurvivorsNews news;
  for (std::size_t index = 0; index < count; ++index) {
    news.hits.push_back(ro::sim::SurvivorsHit{.foe_serial = static_cast<std::uint32_t>(index + 1),
                                              .amount = 10,
                                              .skill_id = kFireBoltSkill});
  }
  return news;
}

}  // namespace

TEST_CASE("Budget des effets : au-delà du plafond des lancers, les suivants sont sautés",
          "[app][survivors]") {
  const moonclient::ArenaSkillLooks looks(ro::sim::ShippedSurvivorsRules());
  const ro::sim::SurvivorsField field;
  moonclient::ArenaEffectBudget budget;
  std::vector<moonclient::ArenaLaunch> plan;

  budget.BeginFrame(0);
  moonclient::PlanArenaLaunches(HitsNews(kManyHits), field, looks, {}, {}, budget, plan);
  CHECK(plan.size() == moonclient::kMostArenaLaunchesPerFrame);
  CHECK(budget.launched() == moonclient::kMostArenaLaunchesPerFrame);
  CHECK(budget.skipped() == kManyHits - moonclient::kMostArenaLaunchesPerFrame);

  // L'IMAGE SUIVANTE NE RELIT RIEN : ses nouvelles sont vides, rien ne part, et
  // les sautés de la précédente sont perdus.
  budget.BeginFrame(plan.size());
  moonclient::PlanArenaLaunches(ro::sim::SurvivorsNews{}, field, looks, {}, {}, budget, plan);
  CHECK(plan.empty());
  CHECK(budget.skipped() == 0);
  CHECK(budget.skipped_total() == kManyHits - moonclient::kMostArenaLaunchesPerFrame);
}

TEST_CASE("Budget des effets : au plafond des vivants, plus rien ne part", "[app][survivors]") {
  const moonclient::ArenaSkillLooks looks(ro::sim::ShippedSurvivorsRules());
  const ro::sim::SurvivorsField field;
  moonclient::ArenaEffectBudget budget;
  std::vector<moonclient::ArenaLaunch> plan;

  // SOUS « EFFETS RÉDUITS », AUCUNE RÉSERVE : les impacts ont tout le plafond.
  budget.set_reserves_held(false);
  budget.BeginFrame(moonclient::kMostArenaLivingEffects - 2);
  moonclient::PlanArenaLaunches(HitsNews(5), field, looks, {.reduced = true}, {}, budget, plan);
  CHECK(plan.size() == 2);
  CHECK(budget.skipped() == 3);
  CHECK(budget.ranked_out_total() == 0);

  // Le héros lui-même ne passe pas le plafond : c'est un garde-fou.
  moonclient::ArenaEffectBudget full;
  full.BeginFrame(moonclient::kMostArenaLivingEffects);
  CHECK_FALSE(full.Admit(moonclient::ArenaEffectRank::kHero));
  CHECK(full.ranked_out_total() == 0);
}

TEST_CASE("Budget des effets : un tic de poison n'a pas d'impact", "[app][survivors]") {
  const moonclient::ArenaSkillLooks looks(ro::sim::ShippedSurvivorsRules());
  const ro::sim::SurvivorsField field;
  ro::sim::SurvivorsNews news = HitsNews(2);
  news.hits.back().poison_tick = true;
  moonclient::ArenaEffectBudget budget;
  std::vector<moonclient::ArenaLaunch> plan;
  budget.BeginFrame(0);
  moonclient::PlanArenaLaunches(news, field, looks, {}, {}, budget, plan);
  CHECK(plan.size() == 1);
}

TEST_CASE("Budget des effets : « Effets réduits » ne garde que l'étoile de coup",
          "[app][survivors]") {
  const moonclient::ArenaSkillLooks looks(ro::sim::ShippedSurvivorsRules());
  const ro::sim::SurvivorsField field;
  ro::sim::SurvivorsNews news = HitsNews(1);
  news.casts.push_back(ro::sim::SurvivorsCast{.skill_id = kFireBoltSkill});
  news.level_ups = 1;
  moonclient::ArenaEffectBudget budget;
  std::vector<moonclient::ArenaLaunch> plan;

  budget.BeginFrame(0);
  moonclient::PlanArenaLaunches(news, field, looks, {}, {}, budget, plan);
  CHECK(plan.size() == 3);

  budget.BeginFrame(0);
  moonclient::PlanArenaLaunches(news, field, looks, {.reduced = true}, {}, budget, plan);
  REQUIRE(plan.size() == 1);
  // L'ÉTOILE DE COUP, et non l'éclat propre au skill.
  CHECK(plan.front().effect_id == moonclient::kPlainHitEffect);
  CHECK(looks.Find(kFireBoltSkill)->hit_effect_id != moonclient::kPlainHitEffect);

  // Éteints, aucun effet ne part.
  budget.BeginFrame(0);
  moonclient::PlanArenaLaunches(news, field, looks, {.shown = false}, {}, budget, plan);
  CHECK(plan.empty());
}

TEST_CASE("Budget des effets : les orbes se lisent sur l'état, hors budget, une par orbe",
          "[app][survivors]") {
  const moonclient::ArenaSkillLooks looks(ro::sim::ShippedSurvivorsRules());
  const ro::sim::SurvivorsField field;
  // L'ALLUMAGE D'UNE ORBITE NE PREND RIEN AU BUDGET : les impacts partent jusqu'au plafond,
  // et l'orbite n'en a ôté aucun.
  ro::sim::SurvivorsNews news = HitsNews(kManyHits);
  news.casts.insert(
      news.casts.begin(),
      ro::sim::SurvivorsCast{.skill_id = kSightSkill, .kind = ro::sim::CastKind::kOrbitOn});
  moonclient::ArenaEffectBudget budget;
  std::vector<moonclient::ArenaLaunch> plan;
  budget.BeginFrame(0);
  moonclient::PlanArenaLaunches(news, field, looks, {}, {}, budget, plan);
  CHECK(plan.size() == moonclient::kMostArenaLaunchesPerFrame);

  const std::vector<ro::sim::SurvivorsOrb> orbs = {
      {.skill_id = kSightSkill, .position = {.x = 1000, .y = 2000}},
      {.skill_id = kSightSkill, .position = {.x = 3000, .y = 4000}}};
  std::vector<moonclient::ArenaOrbLook> shown;
  moonclient::PlanArenaOrbs(orbs, looks, {}, shown);
  REQUIRE(shown.size() == orbs.size());
  CHECK(shown[1].at == orbs[1].position);
  CHECK(shown[0].effect_id == looks.Find(kSightSkill)->effect_id);

  // « EFFETS RÉDUITS » COCHÉ EN PLEINE ORBITE : plus aucune orbe à peindre,
  // et `ArenaEffects` éteint celles qu'il portait.
  moonclient::PlanArenaOrbs(orbs, looks, {.reduced = true}, shown);
  CHECK(shown.empty());
  moonclient::PlanArenaOrbs(orbs, looks, {.shown = false}, shown);
  CHECK(shown.empty());
}

TEST_CASE("Vies des effets : une horloge qui revient à zéro ouvre une partie neuve",
          "[app][survivors]") {
  // Une partie finie à 15:00, son plafond d'effets vivants atteint.
  constexpr std::uint32_t kRunEndMs = 900000;
  constexpr std::uint32_t kEffectLifeMs = 500;
  constexpr std::uint32_t kFirstFrameMs = 20;
  moonclient::ArenaEffectLives lives;
  for (std::size_t index = 0; index < moonclient::kMostArenaLivingEffects; ++index) {
    lives.Add(kRunEndMs + kEffectLifeMs);
  }
  lives.Expire(kRunEndMs);
  REQUIRE(lives.size() == moonclient::kMostArenaLivingEffects);
  moonclient::ArenaEffectBudget full;
  full.BeginFrame(lives.size());
  CHECK_FALSE(full.Admit());

  // « Rejouer » : la présentation repart de zéro, et rien ne vit plus.
  lives.Expire(0);
  CHECK(lives.size() == 0);
  moonclient::ArenaEffectBudget fresh;
  fresh.BeginFrame(lives.size());
  CHECK(fresh.Admit());

  // Sur la même horloge, les vies s'éteignent à leur instant.
  lives.Add(kFirstFrameMs + kEffectLifeMs);
  lives.Expire(kFirstFrameMs);
  CHECK(lives.size() == 1);
  lives.Expire(kFirstFrameMs + kEffectLifeMs);
  CHECK(lives.size() == 0);
  lives.Add(kEffectLifeMs);
  lives.Clear();
  CHECK(lives.size() == 0);
}

TEST_CASE("Budget des effets : au plafond, le sort du héros passe devant les impacts",
          "[app][survivors]") {
  const moonclient::ArenaSkillLooks looks(ro::sim::ShippedSurvivorsRules());
  const ro::sim::SurvivorsField field;
  moonclient::ArenaEffectBudget budget;
  std::vector<moonclient::ArenaLaunch> plan;

  // LA RÉSERVE DU HÉROS EST ATTEINTE : les impacts n'ont plus de place, le sort
  // lancé dans la même image, rangé derrière eux dans les nouvelles, en trouve.
  ro::sim::SurvivorsNews news = HitsNews(3);
  news.casts.push_back(ro::sim::SurvivorsCast{.skill_id = kFireBoltSkill});
  budget.BeginFrame(moonclient::kMostArenaLivingEffects - moonclient::kArenaHeroLivingReserve);
  moonclient::PlanArenaLaunches(news, field, looks, {}, {}, budget, plan);
  REQUIRE(plan.size() == 1U);
  CHECK(plan.front().rank == moonclient::ArenaEffectRank::kHero);
  CHECK(budget.skipped() == 3);
  CHECK(budget.ranked_out_total() == 3);
}

TEST_CASE("Budget des effets : l'impact répété sur un même monstre part le dernier",
          "[app][survivors]") {
  const moonclient::ArenaSkillLooks looks(ro::sim::ShippedSurvivorsRules());
  const ro::sim::SurvivorsField field;
  constexpr std::size_t kTwoLaunches = 2;
  moonclient::ArenaEffectBudget budget(
      moonclient::ArenaEffectCaps{.launches_per_frame = kTwoLaunches});
  std::vector<moonclient::ArenaLaunch> plan;

  // Trois coups sur le monstre 1, puis un sur le monstre 2 : ce dernier passe
  // avant les deux répétitions.
  constexpr std::uint32_t kStruck = 1;
  constexpr std::uint32_t kOther = 2;
  ro::sim::SurvivorsNews news;
  for (const std::uint32_t serial : {kStruck, kStruck, kStruck, kOther}) {
    news.hits.push_back(
        ro::sim::SurvivorsHit{.foe_serial = serial, .amount = 10, .skill_id = kFireBoltSkill});
  }
  budget.BeginFrame(0);
  moonclient::PlanArenaLaunches(news, field, looks, {}, {}, budget, plan);
  REQUIRE(plan.size() == kTwoLaunches);
  CHECK(plan[0].rank == moonclient::ArenaEffectRank::kNearHit);
  CHECK(plan[1].rank == moonclient::ArenaEffectRank::kNearHit);
  CHECK(budget.skipped() == 2);
}

TEST_CASE("Budget des effets : hors du cadre vu, rien ne part et rien ne coûte",
          "[app][survivors]") {
  const moonclient::ArenaSkillLooks looks(ro::sim::ShippedSurvivorsRules());
  const ro::sim::SurvivorsField field;
  constexpr std::int32_t kHalfSightUnits = 10 * ro::sim::kSubunitsPerCell;
  const moonclient::ArenaSight sight{.bounded = true,
                                     .west_x = -kHalfSightUnits,
                                     .east_x = kHalfSightUnits,
                                     .south_y = -kHalfSightUnits,
                                     .north_y = kHalfSightUnits};
  const ro::sim::FieldPoint far{.x = 4 * kHalfSightUnits, .y = 0};
  const ro::sim::FieldPoint near{.x = kHalfSightUnits / 2, .y = 0};

  ro::sim::SurvivorsNews news;
  news.hits.push_back(
      ro::sim::SurvivorsHit{.foe_serial = 1, .at = far, .amount = 10, .skill_id = kFireBoltSkill});
  news.hits.push_back(
      ro::sim::SurvivorsHit{.foe_serial = 2, .at = near, .amount = 10, .skill_id = kFireBoltSkill});
  moonclient::ArenaEffectBudget budget;
  std::vector<moonclient::ArenaLaunch> plan;
  budget.BeginFrame(0);
  moonclient::PlanArenaLaunches(news, field, looks, {}, sight, budget, plan);
  REQUIRE(plan.size() == 1U);
  CHECK(plan.front().at == near);
  CHECK(budget.skipped() == 0);
  CHECK(budget.unseen_total() == 1);

  // UN EFFET QUI VIT HORS DU CADRE NE COMPTE PAS AU PLAFOND ; celui que porte
  // le héros compte toujours.
  constexpr std::uint32_t kEndsMs = 1000;
  moonclient::ArenaEffectLives lives;
  lives.Add(kEndsMs, far);
  lives.Add(kEndsMs, near);
  lives.Add(kEndsMs);
  CHECK(lives.size() == 3);
  CHECK(lives.CountSeen(sight) == 2);
  // Sans bornes, le cadre voit tout.
  CHECK(lives.CountSeen(moonclient::ArenaSight{}) == 3);
}

TEST_CASE("Effets de l'arène : le cône part du héros en éventail, chaque boule visée",
          "[app][survivors]") {
  const moonclient::ArenaSkillLooks looks(ro::sim::ShippedSurvivorsRules());
  const ro::sim::SurvivorsField field;
  const ro::sim::FieldPoint hero{.x = 1000, .y = 2000};
  constexpr std::uint8_t kEast = 48;
  constexpr std::uint8_t kSpread = 6;
  const std::int32_t length = ro::sim::Cells(4);
  ro::sim::SurvivorsNews news;
  news.casts.push_back(ro::sim::SurvivorsCast{.skill_id = kFireBallSkill,
                                              .kind = ro::sim::CastKind::kConeOn,
                                              .from = hero,
                                              .to = hero + ro::sim::TurnVector(kEast, length),
                                              .spread_steps = kSpread});
  moonclient::ArenaEffectBudget budget;
  std::vector<moonclient::ArenaLaunch> plan;
  budget.BeginFrame(0);
  moonclient::PlanArenaLaunches(news, field, looks, {}, {}, budget, plan);

  // Du bord gauche au bord droit, l'axe au milieu ; sans cible, la boule
  // resterait droite.
  REQUIRE(plan.size() == 3U);
  const std::array<std::uint8_t, 3> headings = {kEast - kSpread, kEast, kEast + kSpread};
  for (std::size_t index = 0; index < plan.size(); ++index) {
    INFO("boule " << index);
    CHECK(plan[index].kind == moonclient::ArenaLaunchKind::kOnHero);
    CHECK(plan[index].at == hero);
    REQUIRE(plan[index].target.has_value());
    CHECK(*plan[index].target == hero + ro::sim::TurnVector(headings[index], length));
  }
}
