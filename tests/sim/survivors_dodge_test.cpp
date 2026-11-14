// L'ESQUIVE : un coup reçu peut manquer un héros qui en a la chance. Il n'a alors pas eu
// lieu pour ses PV, son réflexe ni sa relève, mais le monstre a bien frappé. Un héros sans
// esquive ne tire rien.

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <tuple>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/core/pcg32.h"
#include "ro/sim/survivors_geometry.h"
#include "ro/sim/survivors_news.h"
#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_run.h"
#include "ro/sim/survivors_state.h"
#include "ro/sim/survivors_team_play.h"

#include "support/survivors_fixture.h"

namespace {

using ro::sim::SurvivorsRun;
using ro::sim::SurvivorsStat;

constexpr std::uint32_t kSeed = 2026;
constexpr std::uint16_t kMageClassId = 1;
constexpr std::uint16_t kKnightClassId = 2;
constexpr std::uint16_t kThiefClassId = 3;
constexpr std::uint16_t kPlainsArenaId = 2;  // la prairie générée
constexpr std::uint16_t kDoubleAttackId = 48;
constexpr std::uint16_t kHidingId = 51;
constexpr std::uint16_t kImproveDodgeId = 49;
constexpr std::uint16_t kFireBoltId = 19;
constexpr std::uint16_t kWhisperCardId = 4102;
constexpr std::uint16_t kWhisperId = 1179;
// Un monstre de pierre, lent : il touche sans mourir.
constexpr std::uint16_t kSteadyFoeId = 1040;
constexpr std::int32_t kSteadyFoeHp = 1000000;
constexpr std::int32_t kWholeChancePct = ro::sim::kWholeChancePct;
constexpr std::int32_t kFarCells = 40;
constexpr std::int32_t kBlowDamage = 5;
constexpr std::int32_t kThreatRadiusUnits = ro::sim::Cells(1);
constexpr std::uint32_t kLongSteps = 1000;
constexpr std::uint32_t kHazardPeriodSteps = 25;
// Assez de coups pour que des esquives et des blessures se mêlent.
constexpr std::uint32_t kManyBlowSteps = 600;
constexpr std::int32_t kPartialChancePct = 30;
constexpr std::int32_t kOneLevelPct = 7;
constexpr std::int32_t kTopLevelPct = 35;

[[nodiscard]] ro::sim::SurvivorsStart StartOf(std::uint16_t class_id,
                                              std::uint16_t first_skill_id) {
  return ro::sim::SurvivorsStart{.seed = kSeed,
                                 .class_id = class_id,
                                 .arena_id = kPlainsArenaId,
                                 .first_skill_id = first_skill_id};
}

void SetDodge(ro::sim::SurvivorsHero& hero, std::int32_t chance_pct) {
  hero.stats.values[static_cast<std::size_t>(SurvivorsStat::kDodgePct)] = chance_pct;
}

// Un héros sans skill ni monstre, avec la chance d'esquive voulue.
[[nodiscard]] SurvivorsRun BareHero(const ro::sim::SurvivorsRules& rules,
                                    const ro::sim::SurvivorsStart& start, std::int32_t chance_pct) {
  auto run = SurvivorsRun::Start(rules, start);
  REQUIRE(run.has_value());
  ro::sim::SurvivorsField& field = run->field_for_tests();
  field.hero.skills.clear();
  field.foes.clear();
  SetDodge(field.hero, chance_pct);
  return std::move(*run);
}

[[nodiscard]] SurvivorsRun BareThief(const ro::sim::SurvivorsRules& rules,
                                     std::int32_t chance_pct) {
  return BareHero(rules, StartOf(kThiefClassId, kDoubleAttackId), chance_pct);
}

// Un monstre robuste et immobile, à `at`. Rend son rang dans la liste.
[[nodiscard]] std::size_t PlaceSteadyFoe(ro::sim::SurvivorsField& field, ro::sim::FieldPoint at) {
  REQUIRE(ro::test_support::PlaceFoe(field, kSteadyFoeId, at) != 0U);
  ro::sim::SurvivorsFoe& foe = field.foes.back();
  foe.units_per_step = 0;
  foe.hp = kSteadyFoeHp;
  foe.max_hp = kSteadyFoeHp;
  return field.foes.size() - 1U;
}

// Le héros ne monte pas de niveau pendant ce pas.
[[nodiscard]] ro::sim::SurvivorsNews StepOnce(SurvivorsRun& run) {
  ro::sim::SurvivorsField& field = run.field_for_tests();
  for (std::size_t index = 0; index < ro::sim::HeroCount(field); ++index) {
    run.PutHeroOnStage(index);
    field.hero.exp = 0;
  }
  run.PutHeroOnStage(0);
  field.gems.clear();
  (void)run.TakeNews();
  run.Step(ro::sim::SurvivorsInput{});
  return run.TakeNews();
}

// Le coup d'un monstre posé sur le héros, à ce pas : rend les nouvelles du pas.
[[nodiscard]] ro::sim::SurvivorsNews TakeBlow(SurvivorsRun& run) {
  ro::sim::SurvivorsField& field = run.field_for_tests();
  (void)PlaceSteadyFoe(field, field.hero.position);
  return StepOnce(run);
}

}  // namespace

TEST_CASE(
    "Esquive survivors : à 100 %, le coup de contact ne coûte aucun PV et compte pour la "
    "recharge",
    "[sim][survivors]") {
  const ro::sim::SurvivorsRules& rules = ro::sim::ShippedSurvivorsRules();
  SurvivorsRun run = BareThief(rules, kWholeChancePct);
  ro::sim::SurvivorsField& field = run.field_for_tests();
  const std::int32_t full_hp = field.hero.hp;
  const std::size_t rank = PlaceSteadyFoe(field, field.hero.position);
  const ro::core::Pcg32 before = field.combat_random;
  const ro::sim::SurvivorsNews news = StepOnce(run);

  CHECK(field.hero.hp == full_hp);
  CHECK(news.hero_hurt == 0);
  CHECK(news.hurt_by_hero[0] == 0);
  CHECK(news.dodged_by_hero[0] == 1U);
  CHECK(field.combat_random != before);

  // LA RECHARGE : le monstre a frappé, et il attend comme s'il avait touché.
  const ro::sim::FoeRule* rule = ro::sim::FindFoe(rules, kSteadyFoeId);
  REQUIRE(rule != nullptr);
  const std::uint32_t ready_step = field.foes[rank].touch_ready_step;
  CHECK(ready_step > field.step);
  CHECK(ready_step <= field.step + ro::sim::StepsOf(rule->touch_interval_ms));
  const ro::core::Pcg32 after_blow = field.combat_random;
  const ro::sim::SurvivorsNews next = StepOnce(run);
  CHECK(next.dodged_by_hero[0] == 0U);
  CHECK(field.combat_random == after_blow);
}

TEST_CASE("Esquive survivors : un coup esquivé ne déclenche ni réflexe ni relève",
          "[sim][survivors]") {
  const ro::sim::SurvivorsRules& rules = ro::sim::ShippedSurvivorsRules();

  SECTION("le même coup, sous la moitié des PV, déclenche le réflexe s'il touche") {
    SurvivorsRun run = BareThief(rules, 0);
    ro::sim::SurvivorsField& field = run.field_for_tests();
    field.hero.skills.push_back(
        ro::sim::OwnedSkill{.skill_id = kHidingId, .level = 1, .ready_step = field.step});
    field.hero.hp = field.hero.max_hp / 2;
    (void)TakeBlow(run);
    CHECK(field.hero.veil_until_step > field.step);
  }

  SECTION("esquivé, il ne le déclenche pas") {
    SurvivorsRun run = BareThief(rules, kWholeChancePct);
    ro::sim::SurvivorsField& field = run.field_for_tests();
    field.hero.skills.push_back(
        ro::sim::OwnedSkill{.skill_id = kHidingId, .level = 1, .ready_step = field.step});
    const std::uint32_t ready_step = field.hero.skills.front().ready_step;
    field.hero.hp = field.hero.max_hp / 2;
    const ro::sim::SurvivorsNews news = TakeBlow(run);
    CHECK(field.hero.veil_until_step == 0U);
    CHECK(field.hero.skills.front().ready_step == ready_step);
    CHECK(news.casts.empty());
  }

  SECTION("esquivé au dernier PV, il ne provoque pas de relève") {
    SurvivorsRun run = BareThief(rules, kWholeChancePct);
    ro::sim::SurvivorsField& field = run.field_for_tests();
    field.lives_left = 1;
    field.hero.hp = 1;
    const ro::sim::SurvivorsNews news = TakeBlow(run);
    CHECK(field.hero.hp == 1);
    CHECK(field.lives_left == 1);
    CHECK(news.revives.empty());
    CHECK(field.phase == ro::sim::SurvivorsPhase::kRunning);
  }
}

TEST_CASE("Esquive survivors : elle vaut pour les tirs et les menaces au sol", "[sim][survivors]") {
  const ro::sim::SurvivorsRules& rules = ro::sim::ShippedSurvivorsRules();
  SurvivorsRun run = BareThief(rules, kWholeChancePct);
  ro::sim::SurvivorsField& field = run.field_for_tests();
  const std::int32_t full_hp = field.hero.hp;

  SECTION("un tir esquivé s'éteint") {
    field.foe_shots.push_back(ro::sim::SurvivorsFoeShot{.position = field.hero.position,
                                                        .previous_position = field.hero.position,
                                                        .damage = kBlowDamage,
                                                        .radius_units = kThreatRadiusUnits,
                                                        .expires_step = field.step + kLongSteps});
    const ro::sim::SurvivorsNews news = StepOnce(run);
    CHECK(field.hero.hp == full_hp);
    CHECK(news.dodged_by_hero[0] == 1U);
    CHECK(field.foe_shots.empty());
  }

  SECTION("une menace esquivée attend sa période") {
    field.hazards.push_back(ro::sim::SurvivorsHazard{.center = field.hero.position,
                                                     .radius_units = kThreatRadiusUnits,
                                                     .damage = kBlowDamage,
                                                     .armed_step = field.step,
                                                     .until_step = field.step + kLongSteps,
                                                     .period_steps = kHazardPeriodSteps});
    const std::uint32_t struck_step = field.step;
    const ro::sim::SurvivorsNews news = StepOnce(run);
    CHECK(field.hero.hp == full_hp);
    CHECK(news.dodged_by_hero[0] == 1U);
    REQUIRE(field.hazards.size() == 1U);
    CHECK(field.hazards.front().next_hurt_step >= struck_step + kHazardPeriodSteps);
  }
}

TEST_CASE("Esquive survivors : sans chance, aucun tirage", "[sim][survivors]") {
  const ro::sim::SurvivorsRules& rules = ro::sim::ShippedSurvivorsRules();
  const auto check_no_draw = [&](ro::sim::SurvivorsStart start) {
    SurvivorsRun run = BareHero(rules, start, 0);
    ro::sim::SurvivorsField& field = run.field_for_tests();
    const std::int32_t full_hp = field.hero.hp;
    const ro::core::Pcg32 before = field.combat_random;
    const ro::sim::SurvivorsNews news = TakeBlow(run);
    CHECK(field.hero.hp < full_hp);
    CHECK(news.hurt_by_hero[0] > 0);
    CHECK(news.dodged_by_hero[0] == 0U);
    CHECK(field.combat_random == before);
  };
  check_no_draw(StartOf(kThiefClassId, kDoubleAttackId));
  check_no_draw(StartOf(kMageClassId, kFireBoltId));
}

TEST_CASE("Esquive survivors : un loadout ne la porte pas", "[sim][survivors]") {
  ro::sim::SurvivorsLoadout loadout;
  CHECK(ro::sim::LoadoutWithinBounds(loadout));
  loadout.bonus.values[static_cast<std::size_t>(SurvivorsStat::kDodgePct)] = 1;
  CHECK_FALSE(ro::sim::LoadoutWithinBounds(loadout));
}

TEST_CASE("Carte Whisper : dans la seule réserve du Voleur, 7 % par niveau, au plus 35 %",
          "[sim][survivors]") {
  // Une cote de un : chaque tirage fait tomber la carte.
  ro::sim::SurvivorsRules rules = ro::sim::ShippedSurvivorsRules();
  rules.card_drops.common_odds = 1;
  const ro::sim::CardRule* card = ro::sim::FindCard(rules, kWhisperCardId);
  REQUIRE(card != nullptr);
  CHECK(card->stat == SurvivorsStat::kDodgePct);
  REQUIRE(card->per_level.size() == ro::sim::kMostCardLevels);
  for (std::size_t level = 0; level < card->per_level.size(); ++level) {
    CHECK(card->per_level[level] == kOneLevelPct * static_cast<std::int32_t>(level + 1U));
  }
  CHECK(card->per_level.back() == kTopLevelPct);

  const ro::sim::FoeRule* whisper = ro::sim::FindFoe(rules, kWhisperId);
  REQUIRE(whisper != nullptr);
  CHECK(whisper->card_item_id == kWhisperCardId);

  const auto holds = [&](std::uint16_t class_id) {
    const ro::sim::ClassRule* hero_class = ro::sim::FindClass(rules, class_id);
    REQUIRE(hero_class != nullptr);
    return std::ranges::find(hero_class->card_pool, kWhisperCardId) != hero_class->card_pool.end();
  };
  CHECK(holds(kThiefClassId));
  CHECK_FALSE(holds(kMageClassId));
  CHECK_FALSE(holds(kKnightClassId));

  // Elle n'ouvre aucune évolution.
  CHECK(std::ranges::find(rules.evolutions, kWhisperCardId,
                          &ro::sim::EvolutionRule::card_item_id) == rules.evolutions.end());

  const auto kill_whisper = [](SurvivorsRun& run) {
    ro::sim::SurvivorsField& field = run.field_for_tests();
    const ro::sim::FieldPoint at =
        field.hero.position + ro::sim::FieldPoint{ro::sim::Cells(kFarCells), 0};
    REQUIRE(ro::test_support::PlaceFoe(field, kWhisperId, at) != 0U);
    field.foes.back().hp = 0;
    field.foes.back().units_per_step = 0;
    (void)StepOnce(run);
  };
  const auto cards_on_ground = [](const ro::sim::SurvivorsField& field) {
    return std::ranges::count_if(field.pickups, [](const ro::sim::SurvivorsPickup& pickup) {
      return pickup.kind == ro::sim::PickupKind::kCard;
    });
  };

  SECTION("le Voleur la voit tomber, et elle donne sa chance") {
    SurvivorsRun run = BareThief(rules, 0);
    ro::sim::SurvivorsField& field = run.field_for_tests();
    kill_whisper(run);
    CHECK(cards_on_ground(field) == 1);
    // Un allumage de posture refait les statistiques.
    field.hero.cards.push_back(
        ro::sim::OwnedCard{.card_item_id = kWhisperCardId, .level = ro::sim::kMostCardLevels});
    field.hero.skills.push_back(
        ro::sim::OwnedSkill{.skill_id = kImproveDodgeId, .level = 1, .ready_step = field.step});
    (void)StepOnce(run);
    CHECK(field.hero.stats.Of(SurvivorsStat::kDodgePct) == kTopLevelPct);
  }

  SECTION("le Magicien ne tire rien et ne voit rien tomber") {
    SurvivorsRun run = BareHero(rules, StartOf(kMageClassId, kFireBoltId), 0);
    ro::sim::SurvivorsField& field = run.field_for_tests();
    const ro::core::Pcg32 before = field.card_random;
    kill_whisper(run);
    CHECK(field.card_random == before);
    CHECK(cards_on_ground(field) == 0);
  }
}

TEST_CASE("Esquive survivors : le contrôle des règles la borne", "[sim][survivors]") {
  const ro::sim::SurvivorsRules& shipped = ro::sim::ShippedSurvivorsRules();
  REQUIRE(ro::sim::CheckSurvivorsRules(shipped).has_value());
  std::vector<ro::sim::CardRule> cards(shipped.cards.begin(), shipped.cards.end());
  const auto whisper = std::ranges::find(cards, kWhisperCardId, &ro::sim::CardRule::card_item_id);
  REQUIRE(whisper != cards.end());
  ro::sim::SurvivorsRules rules = shipped;
  rules.cards = cards;

  SECTION("une carte qui rendrait intouchable") {
    constexpr std::array<std::int32_t, 1> kImmune{kWholeChancePct};
    whisper->per_level = kImmune;
    CHECK_FALSE(ro::sim::CheckSurvivorsRules(rules).has_value());
  }

  SECTION("une chance négative") {
    constexpr std::array<std::int32_t, 1> kNegative{-1};
    whisper->per_level = kNegative;
    CHECK_FALSE(ro::sim::CheckSurvivorsRules(rules).has_value());
  }

  SECTION("plusieurs cartes qui, ensemble, rendraient intouchable") {
    constexpr std::array<std::int32_t, 1> kMost{kWholeChancePct - 1};
    whisper->per_level = kMost;
    REQUIRE(ro::sim::CheckSurvivorsRules(rules).has_value());
    const auto smokie = std::ranges::find_if(
        cards, [](const ro::sim::CardRule& card) { return card.stat == SurvivorsStat::kCritPct; });
    REQUIRE(smokie != cards.end());
    smokie->stat = SurvivorsStat::kDodgePct;
    CHECK_FALSE(ro::sim::CheckSurvivorsRules(rules).has_value());
  }

  SECTION("une posture qui la majorerait") {
    std::vector<ro::sim::SurvivorsSkillRule> skills(shipped.skills.begin(), shipped.skills.end());
    const auto stance =
        std::ranges::find(skills, kImproveDodgeId, &ro::sim::SurvivorsSkillRule::skill_id);
    REQUIRE(stance != skills.end());
    stance->stance_stat = SurvivorsStat::kDodgePct;
    rules.skills = skills;
    CHECK_FALSE(ro::sim::CheckSurvivorsRules(rules).has_value());
  }
}

TEST_CASE("Esquive survivors : deux parties de même graine sont identiques", "[sim][survivors]") {
  const ro::sim::SurvivorsRules& rules = ro::sim::ShippedSurvivorsRules();
  const auto play = [&]() {
    SurvivorsRun run = BareThief(rules, kPartialChancePct);
    ro::sim::SurvivorsField& field = run.field_for_tests();
    (void)PlaceSteadyFoe(field, field.hero.position);
    std::uint32_t dodged = 0;
    std::int32_t hurt = 0;
    for (std::uint32_t step = 0; step < kManyBlowSteps; ++step) {
      field.hero.hp = field.hero.max_hp;
      const ro::sim::SurvivorsNews news = StepOnce(run);
      dodged += news.dodged_by_hero[0];
      hurt += news.hurt_by_hero[0];
    }
    return std::tuple{ro::sim::SurvivorsDigest(field), dodged, hurt};
  };
  const auto first = play();
  CHECK(first == play());
  // L'essai a du sens : des coups ont touché, d'autres ont manqué.
  CHECK(std::get<1>(first) > 0U);
  CHECK(std::get<2>(first) > 0);
}

TEST_CASE("Esquive survivors : en coop, chacun tire avec sa propre chance", "[sim][survivors]") {
  const ro::sim::SurvivorsRules& rules = ro::sim::ShippedSurvivorsRules();
  // Le Magicien en tête, un Voleur allié : deux monstres, un sur chacun, loin l'un de l'autre.
  const auto blow_on_each = [&](std::int32_t mage_pct, std::int32_t thief_pct) {
    ro::sim::SurvivorsStart start = StartOf(kMageClassId, kFireBoltId);
    start.allies.push_back(
        ro::sim::SurvivorsAllyStart{.class_id = kThiefClassId, .first_skill_id = kDoubleAttackId});
    auto started = SurvivorsRun::Start(rules, start);
    REQUIRE(started.has_value());
    SurvivorsRun run = std::move(*started);
    ro::sim::SurvivorsField& field = run.field_for_tests();
    field.foes.clear();
    const ro::sim::FieldPoint mage_at = field.hero.position;
    field.hero.skills.clear();
    SetDodge(field.hero, mage_pct);
    run.PutHeroOnStage(1);
    field.hero.skills.clear();
    field.hero.position = mage_at + ro::sim::FieldPoint{ro::sim::Cells(kFarCells), 0};
    field.hero.previous_position = field.hero.position;
    SetDodge(field.hero, thief_pct);
    (void)PlaceSteadyFoe(field, field.hero.position);
    run.PutHeroOnStage(0);
    (void)PlaceSteadyFoe(field, mage_at);
    const ro::core::Pcg32 before = field.combat_random;
    ro::sim::SurvivorsNews news = StepOnce(run);
    return std::pair{std::move(news), field.combat_random != before};
  };

  SECTION("le Voleur esquive, le Magicien encaisse") {
    const auto [news, drew] = blow_on_each(0, kWholeChancePct);
    CHECK(news.dodged_by_hero[0] == 0U);
    CHECK(news.hurt_by_hero[0] > 0);
    CHECK(news.dodged_by_hero[1] == 1U);
    CHECK(news.hurt_by_hero[1] == 0);
    CHECK(drew);
  }

  SECTION("sa chance ne vaut pas pour l'autre") {
    const auto [news, drew] = blow_on_each(kWholeChancePct, 0);
    CHECK(news.dodged_by_hero[0] == 1U);
    CHECK(news.hurt_by_hero[0] == 0);
    CHECK(news.dodged_by_hero[1] == 0U);
    CHECK(news.hurt_by_hero[1] > 0);
    CHECK(drew);
  }

  SECTION("sans chance, personne ne tire") {
    const auto [news, drew] = blow_on_each(0, 0);
    CHECK(news.hurt_by_hero[0] > 0);
    CHECK(news.hurt_by_hero[1] > 0);
    CHECK_FALSE(drew);
  }
}
