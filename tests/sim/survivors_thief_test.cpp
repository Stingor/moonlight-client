// LE VOLEUR : la troisième classe du catalogue. Il frappe au contact, laisse le
// poison finir le travail, double certains coups, aveugle, repousse ce qui le
// talonne sans bouger, et se dérobe quand il est blessé. Rien de ce qu'il apporte
// ne tire quoi que ce soit pour une classe qui ne l'a pas.

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/core/pcg32.h"
#include "ro/sim/survivors_bot.h"
#include "ro/sim/survivors_geometry.h"
#include "ro/sim/survivors_match.h"
#include "ro/sim/survivors_news.h"
#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_run.h"
#include "ro/sim/survivors_state.h"
#include "ro/sim/survivors_team_play.h"

#include "support/survivors_fixture.h"

namespace {

using ro::sim::SurvivorsRun;

constexpr std::uint16_t kMageClassId = 1;
constexpr std::uint16_t kKnightClassId = 2;
constexpr std::uint16_t kThiefClassId = 3;
constexpr std::uint16_t kPlainsArenaId = 2;  // la prairie générée
constexpr std::uint64_t kSeed = 20261107;

constexpr std::uint16_t kFireBoltId = 19;
constexpr std::uint16_t kBashId = 5;
constexpr std::uint16_t kSpearStabId = 58;
constexpr std::uint16_t kDoubleAttackId = 48;
constexpr std::uint16_t kImproveDodgeId = 49;
constexpr std::uint16_t kHidingId = 51;
constexpr std::uint16_t kEnvenomId = 52;
constexpr std::uint16_t kCloakingId = 135;
constexpr std::uint16_t kVenomDustId = 140;
constexpr std::uint16_t kSprinkleSandId = 149;
constexpr std::uint16_t kBackSlideId = 150;
constexpr std::uint16_t kThrowVenomKnifeId = 1004;

constexpr std::uint16_t kSmokieCardId = 4044;

constexpr std::uint16_t kSmokieId = 1056;
constexpr std::uint16_t kMasteringId = 1090;  // un mini-boss
constexpr std::uint16_t kPoringId = 1002;     // un monstre qu'un recul déplace
// Un monstre de pierre, lent et neutre : le poison et le coup le touchent à plein.
constexpr std::uint16_t kSteadyFoeId = 1040;
constexpr std::int32_t kSteadyFoeHp = 1000000;
constexpr std::int32_t kNearCells = 2;
constexpr std::int32_t kHalfTurn = ro::sim::kHeadingSteps / 2;

// Assez de pas pour qu'un skill prêt tire et que son coup tombe, avant sa recharge.
constexpr std::uint32_t kFewSteps = 5;
// Assez pour qu'un nuage ou un couteau atteigne son monstre.
constexpr std::uint32_t kFlightSteps = 40;
// Un skill qui ne tire jamais pendant l'essai.
constexpr std::uint32_t kNever = std::numeric_limits<std::uint32_t>::max();

[[nodiscard]] ro::sim::SurvivorsStart ThiefStart() {
  return ro::sim::SurvivorsStart{.seed = kSeed,
                                 .class_id = kThiefClassId,
                                 .arena_id = kPlainsArenaId,
                                 .first_skill_id = kDoubleAttackId};
}

[[nodiscard]] ro::sim::SurvivorsStart MageStart() {
  return ro::sim::SurvivorsStart{.seed = kSeed,
                                 .class_id = kMageClassId,
                                 .arena_id = kPlainsArenaId,
                                 .first_skill_id = kFireBoltId};
}

// Un Voleur sans skill ni monstre, à l'heure où un coffre fait évoluer.
[[nodiscard]] SurvivorsRun BareThief(const ro::sim::SurvivorsRules& rules) {
  auto run = SurvivorsRun::Start(rules, ThiefStart());
  REQUIRE(run.has_value());
  ro::sim::SurvivorsField& field = run->field_for_tests();
  field.hero.skills.clear();
  field.foes.clear();
  field.step = rules.chests.evolution_from_ms / ro::sim::kSurvivorsStepMs;
  return std::move(*run);
}

void Hold(ro::sim::SurvivorsField& field, std::uint16_t skill_id, std::uint8_t level = 1) {
  field.hero.skills.push_back(
      ro::sim::OwnedSkill{.skill_id = skill_id, .level = level, .ready_step = field.step});
}

[[nodiscard]] ro::sim::OwnedSkill* Owned(ro::sim::SurvivorsField& field, std::uint16_t skill_id) {
  const auto found = std::ranges::find(field.hero.skills, skill_id, &ro::sim::OwnedSkill::skill_id);
  return found != field.hero.skills.end() ? &*found : nullptr;
}

// Le héros ne monte ni ne tombe pendant ces pas, et ses PV ne bougent que par les coups.
void StepKeepingLevel(SurvivorsRun& run, std::uint32_t steps, ro::sim::SurvivorsInput input = {}) {
  ro::sim::SurvivorsField& field = run.field_for_tests();
  for (std::uint32_t done = 0; done < steps; ++done) {
    field.hero.exp = 0;
    field.gems.clear();
    run.Step(input);
  }
}

// Comme StepKeepingLevel, les PV du héros refaits : aucun réflexe ne part.
void StepHealed(SurvivorsRun& run, std::uint32_t steps) {
  ro::sim::SurvivorsField& field = run.field_for_tests();
  for (std::uint32_t done = 0; done < steps; ++done) {
    field.hero.hp = field.hero.max_hp;
    StepKeepingLevel(run, 1);
  }
}

// Un monstre robuste et immobile, à `offset` du héros. Rend sa place dans la liste
// par son numéro : `FoeOf` le retrouve.
[[nodiscard]] std::uint32_t PlaceSteadyFoe(ro::sim::SurvivorsField& field,
                                           ro::sim::FieldPoint offset) {
  const std::uint32_t serial =
      ro::test_support::PlaceFoe(field, kSteadyFoeId, field.hero.position + offset);
  REQUIRE(serial != 0U);
  ro::sim::SurvivorsFoe& foe = field.foes.back();
  foe.units_per_step = 0;
  foe.hp = kSteadyFoeHp;
  foe.max_hp = kSteadyFoeHp;
  return serial;
}

[[nodiscard]] ro::sim::SurvivorsFoe* FoeOf(ro::sim::SurvivorsField& field, std::uint32_t serial) {
  const auto found = std::ranges::find(field.foes, serial, &ro::sim::SurvivorsFoe::serial);
  return found != field.foes.end() ? &*found : nullptr;
}

// Un monstre `foe_id` tué sur place, loin du héros.
void Kill(SurvivorsRun& run, std::uint16_t foe_id) {
  constexpr std::int32_t kKillCells = 6;
  ro::sim::SurvivorsField& field = run.field_for_tests();
  const ro::sim::FieldPoint at =
      field.hero.position + ro::sim::FieldPoint{ro::sim::Cells(kKillCells), 0};
  REQUIRE(ro::test_support::PlaceFoe(field, foe_id, at) != 0U);
  field.foes.back().hp = 0;
  field.foes.back().units_per_step = 0;
  StepKeepingLevel(run, 1);
}

[[nodiscard]] std::size_t CardsOnGround(const ro::sim::SurvivorsField& field) {
  return static_cast<std::size_t>(
      std::ranges::count_if(field.pickups, [](const ro::sim::SurvivorsPickup& pickup) {
        return pickup.kind == ro::sim::PickupKind::kCard;
      }));
}

[[nodiscard]] std::uint32_t HitsOn(const ro::sim::SurvivorsNews& news, std::uint32_t foe_serial,
                                   std::uint16_t skill_id) {
  return static_cast<std::uint32_t>(
      std::ranges::count_if(news.hits, [&](const ro::sim::SurvivorsHit& hit) {
        return hit.foe_serial == foe_serial && hit.skill_id == skill_id;
      }));
}

// Le héros encaisse un coup au pas suivant, sous la moitié de ses PV : un monstre
// fort sur ses pieds, immédiatement retiré. Rend les nouvelles du pas.
[[nodiscard]] ro::sim::SurvivorsNews TakeWoundingBlow(SurvivorsRun& run) {
  ro::sim::SurvivorsField& field = run.field_for_tests();
  field.hero.hp = field.hero.max_hp / 2;
  const std::uint32_t serial = PlaceSteadyFoe(field, ro::sim::FieldPoint{});
  (void)run.TakeNews();
  StepKeepingLevel(run, 1);
  std::erase_if(field.foes, [&](const ro::sim::SurvivorsFoe& foe) { return foe.serial == serial; });
  return run.TakeNews();
}

}  // namespace

TEST_CASE("Voleur survivors : sa classe, ses corps et sa place au catalogue", "[sim][survivors]") {
  const ro::sim::SurvivorsRules& rules = ro::sim::ShippedSurvivorsRules();
  // La partie classée et le défi prennent la première classe.
  CHECK(rules.classes.front().class_id == kMageClassId);
  const ro::sim::ClassRule* thief = ro::sim::FindClass(rules, kThiefClassId);
  const ro::sim::ClassRule* knight = ro::sim::FindClass(rules, kKnightClassId);
  REQUIRE(thief != nullptr);
  REQUIRE(knight != nullptr);
  CHECK(std::string_view(thief->key) == "thief");
  CHECK(thief->attack_gesture);
  CHECK(thief->mount_card_item_id == 0);
  CHECK(thief->reborn_level == knight->reborn_level);
  // Le plus rapide ; plus de PV que le Magicien, moins que le Chevalier.
  CHECK(thief->walk_ms_per_cell < rules.classes.front().walk_ms_per_cell);
  CHECK(thief->max_hp > rules.classes.front().max_hp);
  CHECK(thief->max_hp < knight->max_hp);

  ro::sim::SurvivorsHero hero;
  CHECK(ro::sim::HeroJobOf(*thief, hero) == 12);
  hero.evolutions = 1;
  CHECK(ro::sim::HeroJobOf(*thief, hero) == 12);
  hero.level = thief->reborn_level;
  CHECK(ro::sim::HeroJobOf(*thief, hero) == 4013);

  // Sa réserve de cartes : celle du Magicien, plus Whisper (survivors_dodge_test).
  for (const std::uint16_t card_item_id : rules.classes.front().card_pool) {
    CHECK(std::ranges::find(thief->card_pool, card_item_id) != thief->card_pool.end());
  }
  CHECK(std::ranges::find(thief->card_pool, kSmokieCardId) != thief->card_pool.end());
  CHECK(thief->card_pool.size() == rules.classes.front().card_pool.size() + 1);
}

TEST_CASE("Voleur survivors : chaque évolution remplace ses skills par l'évolué",
          "[sim][survivors]") {
  const ro::sim::SurvivorsRules& rules = ro::sim::ShippedSurvivorsRules();
  const ro::sim::ClassRule* thief = ro::sim::FindClass(rules, kThiefClassId);
  REQUIRE(thief != nullptr);
  std::size_t evolutions = 0;
  for (const ro::sim::EvolutionRule& evolution : rules.evolutions) {
    if (std::ranges::find(thief->skill_pool, evolution.skill_id) == thief->skill_pool.end()) {
      continue;
    }
    ++evolutions;
    INFO("évolué " << evolution.evolved_skill_id);
    SurvivorsRun run = BareThief(rules);
    ro::sim::SurvivorsField& field = run.field_for_tests();
    const auto hold_at_top = [&](std::uint16_t skill_id) {
      const ro::sim::SurvivorsSkillRule* rule = ro::sim::FindSkill(rules, skill_id);
      REQUIRE(rule != nullptr);
      Hold(field, skill_id, static_cast<std::uint8_t>(rule->levels.size()));
      field.hero.skills.back().ready_step = kNever;
    };
    hold_at_top(evolution.skill_id);
    if (evolution.partner_skill_id != 0) {
      hold_at_top(evolution.partner_skill_id);
    } else {
      field.hero.cards.push_back(
          ro::sim::OwnedCard{.card_item_id = evolution.card_item_id, .level = 1});
    }
    field.pickups.push_back(ro::sim::SurvivorsPickup{.serial = field.next_serial++,
                                                     .kind = ro::sim::PickupKind::kChest,
                                                     .item_id = rules.chests.item_id,
                                                     .position = field.hero.position});
    StepHealed(run, 1);
    CHECK(field.hero.evolutions == 1U);
    CHECK(Owned(field, evolution.evolved_skill_id) != nullptr);
    CHECK(Owned(field, evolution.skill_id) == nullptr);
    CHECK(ro::sim::HeroJobOf(*thief, field.hero) == 12);
    CHECK(std::ranges::find(thief->skill_pool, evolution.evolved_skill_id) ==
          thief->skill_pool.end());
  }
  CHECK(evolutions == 7U);
}

TEST_CASE("Poison survivors : chaque skill qui en porte empoisonne", "[sim][survivors]") {
  const ro::sim::SurvivorsRules& rules = ro::sim::ShippedSurvivorsRules();
  for (const std::uint16_t skill_id : {kEnvenomId, kVenomDustId, kThrowVenomKnifeId}) {
    INFO("skill " << skill_id);
    SurvivorsRun run = BareThief(rules);
    ro::sim::SurvivorsField& field = run.field_for_tests();
    const std::uint32_t serial =
        PlaceSteadyFoe(field, ro::sim::FieldPoint{ro::sim::Cells(kNearCells), 0});
    Hold(field, skill_id);
    StepHealed(run, kFlightSteps);
    const ro::sim::SurvivorsFoe* foe = FoeOf(field, serial);
    REQUIRE(foe != nullptr);
    CHECK(foe->poison_until_step > 0U);
    CHECK(foe->poison_skill_id == skill_id);
  }
}

TEST_CASE("Poison survivors : ses tics sont comptés, et sans tirage", "[sim][survivors]") {
  const ro::sim::SurvivorsRules& rules = ro::sim::ShippedSurvivorsRules();
  const ro::sim::SurvivorsSkillRule* envenom = ro::sim::FindSkill(rules, kEnvenomId);
  REQUIRE(envenom != nullptr);
  const ro::sim::SkillLevel& level = envenom->levels.front();
  REQUIRE(level.poison_ms > 0U);

  SurvivorsRun run = BareThief(rules);
  ro::sim::SurvivorsField& field = run.field_for_tests();
  const std::uint32_t serial =
      PlaceSteadyFoe(field, ro::sim::FieldPoint{ro::sim::Cells(kNearCells), 0});
  Hold(field, kEnvenomId);
  const ro::core::Pcg32 combat_before = field.combat_random;
  StepHealed(run, kFewSteps);
  REQUIRE(FoeOf(field, serial)->poison_until_step > 0U);
  Owned(field, kEnvenomId)->ready_step = kNever;
  field.hero.skills.front().ready_step = kNever;

  // Le poison couvre sa durée : une fois par période, tic compris à son terme.
  std::uint32_t ticks = 0;
  const std::uint32_t lasting_steps = ro::sim::StepsOf(level.poison_ms) + kFewSteps;
  (void)run.TakeNews();
  for (std::uint32_t done = 0; done < lasting_steps; ++done) {
    StepHealed(run, 1);
    for (const ro::sim::SurvivorsHit& hit : run.TakeNews().hits) {
      if (hit.foe_serial != serial) continue;
      CHECK(hit.poison_tick);
      CHECK_FALSE(hit.critical);
      CHECK(hit.skill_id == kEnvenomId);
      ++ticks;
    }
  }
  const std::uint32_t tick_steps = ro::sim::StepsOf(rules.tuning.poison_tick_ms);
  CHECK(ticks == ro::sim::StepsOf(level.poison_ms) / tick_steps);
  CHECK(FoeOf(field, serial)->poison_until_step == 0U);
  CHECK(field.combat_random == combat_before);
}

TEST_CASE("Poison survivors : il tue, et sa mort est créditée", "[sim][survivors]") {
  const ro::sim::SurvivorsRules& rules = ro::sim::ShippedSurvivorsRules();

  SECTION("en solo") {
    SurvivorsRun run = BareThief(rules);
    ro::sim::SurvivorsField& field = run.field_for_tests();
    const std::uint32_t serial =
        PlaceSteadyFoe(field, ro::sim::FieldPoint{ro::sim::Cells(kNearCells), 0});
    Hold(field, kEnvenomId);
    StepHealed(run, kFewSteps);
    Owned(field, kEnvenomId)->ready_step = kNever;
    FoeOf(field, serial)->hp = 1;
    const std::uint32_t kills_before = field.tally.kills;
    (void)run.TakeNews();
    bool fell = false;
    for (std::uint32_t done = 0; done < kFlightSteps * kFewSteps && !fell; ++done) {
      StepHealed(run, 1);
      const ro::sim::SurvivorsNews news = run.TakeNews();
      fell = std::ranges::any_of(news.falls, [&](const ro::sim::SurvivorsFall& fall) {
        return fall.foe_serial == serial;
      });
    }
    CHECK(fell);
    CHECK(field.tally.kills == kills_before + 1);
  }

  SECTION("en coop, au héros qui l'a posé") {
    ro::sim::SurvivorsStart start = MageStart();
    start.allies.push_back(
        ro::sim::SurvivorsAllyStart{.class_id = kThiefClassId, .first_skill_id = kEnvenomId});
    auto run = SurvivorsRun::Start(rules, start);
    REQUIRE(run.has_value());
    ro::sim::SurvivorsField& field = run->field_for_tests();
    field.hero.skills.clear();
    field.foes.clear();
    ro::sim::SurvivorsHero& ally = field.allies.front().hero;
    const std::uint32_t serial =
        PlaceSteadyFoe(field, ally.position - field.hero.position +
                                  ro::sim::FieldPoint{ro::sim::Cells(kNearCells), 0});
    for (std::uint32_t done = 0; done < kFewSteps; ++done) {
      field.hero.exp = 0;
      run->Step(ro::sim::SurvivorsInput{});
    }
    ro::sim::SurvivorsFoe* foe = FoeOf(field, serial);
    REQUIRE(foe != nullptr);
    REQUIRE(foe->poison_until_step > 0U);
    CHECK(foe->poison_hero == 1U);
    ally.skills.front().ready_step = kNever;
    foe->hp = 1;
    (void)run->TakeNews();
    bool fell = false;
    bool ticked_by_ally = false;
    for (std::uint32_t done = 0; done < kFlightSteps * kFewSteps && !fell; ++done) {
      field.hero.exp = 0;
      field.gems.clear();
      run->Step(ro::sim::SurvivorsInput{});
      const ro::sim::SurvivorsNews news = run->TakeNews();
      for (const ro::sim::SurvivorsHit& hit : news.hits) {
        if (hit.foe_serial == serial) ticked_by_ally = hit.poison_tick && hit.hero_index == 1U;
      }
      for (const ro::sim::SurvivorsFall& fall : news.falls) {
        if (fall.foe_serial != serial) continue;
        fell = true;
        CHECK(fall.hero_index == 1U);
      }
    }
    CHECK(fell);
    CHECK(ticked_by_ally);
  }
}

TEST_CASE("Poison survivors : ni les boss, et l'empreinte le compte", "[sim][survivors]") {
  const ro::sim::SurvivorsRules& rules = ro::sim::ShippedSurvivorsRules();
  SurvivorsRun run = BareThief(rules);
  ro::sim::SurvivorsField& field = run.field_for_tests();
  const std::uint32_t serial = ro::test_support::PlaceFoe(
      field, kMasteringId,
      field.hero.position + ro::sim::FieldPoint{ro::sim::Cells(kNearCells), 0});
  REQUIRE(serial != 0U);
  field.foes.back().units_per_step = 0;
  Hold(field, kEnvenomId);
  StepHealed(run, kFewSteps);
  ro::sim::SurvivorsFoe* boss = FoeOf(field, serial);
  REQUIRE(boss != nullptr);
  CHECK(boss->poison_until_step == 0U);

  // L'empreinte voit un poison posé, et la période de ses tics entre dans celle des règles.
  const std::uint64_t bare = ro::sim::SurvivorsDigest(field);
  boss->poison_until_step = field.step + kFewSteps;
  CHECK(ro::sim::SurvivorsDigest(field) != bare);
  ro::sim::SurvivorsRules slower = rules;
  slower.tuning.poison_tick_ms += ro::sim::kSurvivorsStepMs;
  CHECK(ro::sim::SurvivorsRulesDigest(slower) != ro::sim::SurvivorsRulesDigest(rules));
}

TEST_CASE("Critique survivors : il double le coup, pour un héros qui a une chance",
          "[sim][survivors]") {
  const ro::sim::SurvivorsRules& rules = ro::sim::ShippedSurvivorsRules();
  // Le coup d'une même frappe, avec ou sans critique.
  const auto first_hit = [&](std::int32_t crit_pct) {
    SurvivorsRun run = BareThief(rules);
    ro::sim::SurvivorsField& field = run.field_for_tests();
    field.hero.stats.values[static_cast<std::size_t>(ro::sim::SurvivorsStat::kCritPct)] = crit_pct;
    const std::uint32_t serial =
        PlaceSteadyFoe(field, ro::sim::FieldPoint{ro::sim::Cells(kNearCells), 0});
    Hold(field, kDoubleAttackId);
    const ro::core::Pcg32 before = field.combat_random;
    StepHealed(run, kFewSteps);
    const ro::sim::SurvivorsNews news = run.TakeNews();
    REQUIRE(HitsOn(news, serial, kDoubleAttackId) > 0U);
    const bool drew = field.combat_random != before;
    return std::pair{*std::ranges::find(news.hits, serial, &ro::sim::SurvivorsHit::foe_serial),
                     drew};
  };
  constexpr std::int32_t kAlways = 100;
  const auto [plain, plain_drew] = first_hit(0);
  const auto [doubled, doubled_drew] = first_hit(kAlways);
  CHECK_FALSE(plain.critical);
  CHECK_FALSE(plain_drew);
  CHECK(doubled.critical);
  CHECK(doubled_drew);
  CHECK(doubled.amount == 2 * plain.amount);
}

TEST_CASE("Critique survivors : le Magicien et le Chevalier ne tirent rien", "[sim][survivors]") {
  const ro::sim::SurvivorsRules& rules = ro::sim::ShippedSurvivorsRules();
  struct ClassCase {
    std::uint16_t class_id;
    std::uint16_t skill_id;
  };
  for (const ClassCase& each :
       {ClassCase{kMageClassId, kFireBoltId}, ClassCase{kKnightClassId, kBashId}}) {
    INFO("classe " << each.class_id);
    auto run = SurvivorsRun::Start(rules, ro::sim::SurvivorsStart{.seed = kSeed,
                                                                  .class_id = each.class_id,
                                                                  .arena_id = kPlainsArenaId,
                                                                  .first_skill_id = each.skill_id});
    REQUIRE(run.has_value());
    ro::sim::SurvivorsField& field = run->field_for_tests();
    const std::uint32_t serial =
        PlaceSteadyFoe(field, ro::sim::FieldPoint{ro::sim::Cells(kNearCells), 0});
    const ro::core::Pcg32 before = field.combat_random;
    StepHealed(*run, kFewSteps);
    const ro::sim::SurvivorsNews news = run->TakeNews();
    CHECK(HitsOn(news, serial, each.skill_id) > 0U);
    CHECK(field.combat_random == before);
    CHECK(std::ranges::none_of(news.hits,
                               [](const ro::sim::SurvivorsHit& hit) { return hit.critical; }));
  }
}

TEST_CASE("Smokie Card : elle porte le critique, et tombe pour une équipe qui la tient",
          "[sim][survivors]") {
  // Une cote de un : chaque tirage fait tomber la carte.
  ro::sim::SurvivorsRules rules = ro::sim::ShippedSurvivorsRules();
  rules.card_drops.common_odds = 1;
  const ro::sim::FoeRule* smokie = ro::sim::FindFoe(rules, kSmokieId);
  REQUIRE(smokie != nullptr);
  CHECK(smokie->card_item_id == kSmokieCardId);
  const ro::sim::CardRule* card = ro::sim::FindCard(rules, kSmokieCardId);
  REQUIRE(card != nullptr);
  CHECK(card->stat == ro::sim::SurvivorsStat::kCritPct);

  SECTION("le Voleur la voit tomber, et elle lui donne sa chance") {
    SurvivorsRun run = BareThief(rules);
    ro::sim::SurvivorsField& field = run.field_for_tests();
    const ro::core::Pcg32 before = field.card_random;
    Kill(run, kSmokieId);
    CHECK(field.card_random != before);
    CHECK(CardsOnGround(field) == 1U);
    // Un allumage de posture refait les statistiques.
    field.hero.cards.push_back(ro::sim::OwnedCard{.card_item_id = kSmokieCardId, .level = 1});
    Hold(field, kImproveDodgeId);
    StepHealed(run, 1);
    CHECK(field.hero.stats.Of(ro::sim::SurvivorsStat::kCritPct) == card->per_level.front());
  }

  SECTION("commune à toutes les réserves, le Magicien la voit tomber aussi") {
    auto run = SurvivorsRun::Start(rules, MageStart());
    REQUIRE(run.has_value());
    ro::sim::SurvivorsField& field = run->field_for_tests();
    field.foes.clear();
    const ro::core::Pcg32 before = field.card_random;
    Kill(*run, kSmokieId);
    CHECK(field.card_random != before);
    CHECK(CardsOnGround(field) == 1U);
  }

  SECTION("un Voleur allié la fait tomber pour le Magicien") {
    ro::sim::SurvivorsStart start = MageStart();
    start.allies.push_back(
        ro::sim::SurvivorsAllyStart{.class_id = kThiefClassId, .first_skill_id = kEnvenomId});
    auto run = SurvivorsRun::Start(rules, start);
    REQUIRE(run.has_value());
    ro::sim::SurvivorsField& field = run->field_for_tests();
    field.foes.clear();
    Kill(*run, kSmokieId);
    CHECK(CardsOnGround(field) == 1U);
  }
}

TEST_CASE("Critique survivors : un loadout ne le porte pas, et les journaux n'en savent rien",
          "[sim][survivors]") {
  ro::sim::SurvivorsLoadout loadout;
  CHECK(ro::sim::LoadoutWithinBounds(loadout));
  loadout.bonus.values[static_cast<std::size_t>(ro::sim::SurvivorsStat::kCritPct)] = 1;
  CHECK_FALSE(ro::sim::LoadoutWithinBounds(loadout));
}

TEST_CASE("Sprinkle Sand : son cône aveugle ce qu'il touche, et lui seul", "[sim][survivors]") {
  const ro::sim::SurvivorsRules& rules = ro::sim::ShippedSurvivorsRules();
  const ro::sim::SurvivorsSkillRule* sand = ro::sim::FindSkill(rules, kSprinkleSandId);
  REQUIRE(sand != nullptr);
  CHECK(sand->archetype == ro::sim::SkillArchetype::kCone);
  REQUIRE(sand->levels.front().blind_ms > 0U);

  SurvivorsRun run = BareThief(rules);
  ro::sim::SurvivorsField& field = run.field_for_tests();
  const std::uint8_t facing = field.hero.facing;
  const ro::sim::FieldPoint ahead = ro::sim::TurnVector(facing, ro::sim::Cells(kNearCells));
  const std::uint32_t in_cone = PlaceSteadyFoe(field, ahead);
  const std::uint32_t behind = PlaceSteadyFoe(field, ro::sim::FieldPoint{-ahead.x, -ahead.y});
  Hold(field, kSprinkleSandId);
  StepHealed(run, 1);
  const ro::sim::SurvivorsNews news = run.TakeNews();
  CHECK(HitsOn(news, in_cone, kSprinkleSandId) == 1U);
  CHECK(FoeOf(field, in_cone)->blind_until_step > field.step);
  CHECK(FoeOf(field, behind)->blind_until_step == 0U);
  CHECK(HitsOn(news, behind, kSprinkleSandId) == 0U);

  // Un cône sans aveuglement n'aveugle pas.
  SurvivorsRun other = BareThief(rules);
  ro::sim::SurvivorsField& other_field = other.field_for_tests();
  const std::uint32_t stabbed = PlaceSteadyFoe(other_field, ahead);
  Hold(other_field, kSpearStabId);
  StepHealed(other, 1);
  CHECK(HitsOn(other.TakeNews(), stabbed, kSpearStabId) == 1U);
  CHECK(FoeOf(other_field, stabbed)->blind_until_step == 0U);
}

TEST_CASE("Back Slide : il frappe derrière lui, puis bondit", "[sim][survivors]") {
  const ro::sim::SurvivorsRules& rules = ro::sim::ShippedSurvivorsRules();
  const ro::sim::SurvivorsSkillRule* slide = ro::sim::FindSkill(rules, kBackSlideId);
  REQUIRE(slide != nullptr);
  CHECK(slide->archetype == ro::sim::SkillArchetype::kCone);
  CHECK(slide->cone_behind);

  SurvivorsRun run = BareThief(rules);
  ro::sim::SurvivorsField& field = run.field_for_tests();
  const std::uint8_t facing = field.hero.facing;
  const ro::sim::FieldPoint ahead = ro::sim::TurnVector(facing, ro::sim::Cells(kNearCells));
  const std::uint32_t in_front = PlaceSteadyFoe(field, ahead);
  const std::uint32_t behind = ro::test_support::PlaceFoe(
      field, kPoringId, field.hero.position + ro::sim::FieldPoint{-ahead.x, -ahead.y});
  REQUIRE(behind != 0U);
  field.foes.back().units_per_step = 0;
  field.foes.back().hp = kSteadyFoeHp;
  const ro::sim::FieldPoint stood_at = field.hero.position;
  Hold(field, kBackSlideId);
  StepHealed(run, 1);
  const ro::sim::SurvivorsNews news = run.TakeNews();

  CHECK(HitsOn(news, behind, kBackSlideId) == 1U);
  CHECK(HitsOn(news, in_front, kBackSlideId) == 0U);
  CHECK(FoeOf(field, behind)->knock_left_units > 0);
  CHECK(FoeOf(field, in_front)->knock_left_units == 0);
  CHECK(field.hero.position == stood_at);
  // Il regarde ce qu'il a frappé ; son bond l'emporte dans le sens de sa marche.
  const auto rear_heading =
      static_cast<std::uint8_t>((facing + kHalfTurn) % ro::sim::kHeadingSteps);
  CHECK((field.hero.facing == rear_heading && field.hero.leap_heading == facing &&
         field.hero.leap_steps_left == ro::sim::kBackSlideLeapSteps));

  // Le lancer porte le geste : le héros, puis le point derrière lui, dans le dos.
  const auto cast = std::ranges::find(news.casts, kBackSlideId, &ro::sim::SurvivorsCast::skill_id);
  REQUIRE(cast != news.casts.end());
  CHECK(cast->kind == ro::sim::CastKind::kBackSlide);
  CHECK(cast->to ==
        stood_at + ro::sim::TurnVector(rear_heading, slide->levels.front().reach_units));
  // Il part à sa recharge, sans monstre il attend.
  CHECK(Owned(field, kBackSlideId)->ready_step ==
        field.step - 1 + ro::sim::StepsOf(slide->levels.front().cooldown_ms));
}

TEST_CASE("Hiding : blessé, le héros se dérobe et ne lance plus de skill, mais il marche",
          "[sim][survivors]") {
  const ro::sim::SurvivorsRules& rules = ro::sim::ShippedSurvivorsRules();
  const ro::sim::SurvivorsSkillRule* hiding = ro::sim::FindSkill(rules, kHidingId);
  REQUIRE(hiding != nullptr);
  CHECK(hiding->archetype == ro::sim::SkillArchetype::kReflex);
  const ro::sim::SkillLevel& level = hiding->levels.front();
  constexpr std::uint32_t kHidingMs = 2000;
  constexpr std::uint32_t kHidingCooldownMs = 30000;
  CHECK(level.life_ms == kHidingMs);
  CHECK(level.cooldown_ms == kHidingCooldownMs);

  SurvivorsRun run = BareThief(rules);
  ro::sim::SurvivorsField& field = run.field_for_tests();
  Hold(field, kHidingId);

  SECTION("un coup reçu au-dessus du seuil ne le déclenche pas") {
    const std::uint32_t serial = PlaceSteadyFoe(field, ro::sim::FieldPoint{});
    field.hero.hp = field.hero.max_hp;
    (void)run.TakeNews();
    StepKeepingLevel(run, 1);
    std::erase_if(field.foes,
                  [&](const ro::sim::SurvivorsFoe& foe) { return foe.serial == serial; });
    CHECK(field.hero.hp < field.hero.max_hp);
    CHECK(field.hero.veil_until_step == 0U);
  }

  SECTION("un coup reçu sous le seuil le déclenche, et le dit") {
    const ro::sim::SurvivorsNews news = TakeWoundingBlow(run);
    CHECK(std::ranges::any_of(news.casts, [](const ro::sim::SurvivorsCast& cast) {
      return cast.kind == ro::sim::CastKind::kStanceOn && cast.skill_id == kHidingId;
    }));
    REQUIRE(field.hero.veil_until_step > field.step);
    CHECK(field.hero.veil_sheathes);
    CHECK_FALSE(field.hero.critical_due);
    const ro::sim::FieldPoint walked_from = field.hero.position;
    StepKeepingLevel(run, kFewSteps, ro::sim::SurvivorsInput{.heading = 0});
    CHECK(field.step < field.hero.veil_until_step);
    CHECK(field.hero.position != walked_from);

    // Il ne prend plus de coup et ne lance plus de skill, mais il marche, et la horde vient.
    const std::int32_t hp = field.hero.hp;
    const std::uint32_t stuck = PlaceSteadyFoe(field, ro::sim::FieldPoint{});
    StepKeepingLevel(run, kFewSteps);
    CHECK(field.hero.hp == hp);
    std::erase_if(field.foes,
                  [&](const ro::sim::SurvivorsFoe& foe) { return foe.serial == stuck; });
    const std::uint32_t target =
        PlaceSteadyFoe(field, ro::sim::FieldPoint{ro::sim::Cells(kNearCells), 0});
    constexpr std::int32_t kWalkerCells = 10;
    const std::uint32_t walker = ro::test_support::PlaceFoe(
        field, kSteadyFoeId,
        field.hero.position + ro::sim::FieldPoint{ro::sim::Cells(kWalkerCells), 0});
    REQUIRE(walker != 0U);
    const std::int64_t far_squared =
        ro::sim::DistanceSquared(FoeOf(field, walker)->position, field.hero.position);
    Hold(field, kDoubleAttackId);
    const ro::sim::FieldPoint stood_at = field.hero.position;
    const std::uint32_t left_steps = field.hero.veil_until_step - field.step;
    StepKeepingLevel(run, left_steps);
    const ro::sim::SurvivorsNews during = run.TakeNews();
    CHECK(field.hero.position == stood_at);
    CHECK(HitsOn(during, target, kDoubleAttackId) == 0U);
    CHECK(ro::sim::DistanceSquared(FoeOf(field, walker)->position, field.hero.position) <
          far_squared);
    std::erase_if(field.foes,
                  [&](const ro::sim::SurvivorsFoe& foe) { return foe.serial == walker; });

    // Il frappe de nouveau à son terme.
    field.hero.hp = field.hero.max_hp;
    StepKeepingLevel(run, kFewSteps);
    CHECK(HitsOn(run.TakeNews(), target, kDoubleAttackId) > 0U);
  }

  SECTION("sa recharge l'empêche de repartir tout de suite") {
    (void)TakeWoundingBlow(run);
    REQUIRE(field.hero.veil_until_step > field.step);
    const std::uint32_t first_until = field.hero.veil_until_step;
    CHECK(Owned(field, kHidingId)->ready_step ==
          field.step - 1 + ro::sim::StepsOf(level.cooldown_ms));
    field.step = first_until;
    (void)TakeWoundingBlow(run);
    CHECK(field.hero.veil_until_step == first_until);
    field.step = Owned(field, kHidingId)->ready_step;
    (void)TakeWoundingBlow(run);
    CHECK(field.hero.veil_until_step > first_until);
  }
}

TEST_CASE("Cloaking : il garde ses coups et sa marche, et son premier coup en sortant est critique",
          "[sim][survivors]") {
  const ro::sim::SurvivorsRules& rules = ro::sim::ShippedSurvivorsRules();
  const ro::sim::SurvivorsSkillRule* cloaking = ro::sim::FindSkill(rules, kCloakingId);
  REQUIRE(cloaking != nullptr);
  CHECK(cloaking->archetype == ro::sim::SkillArchetype::kReflex);
  constexpr std::uint32_t kCloakingMs = 4000;
  constexpr std::uint32_t kCloakingCooldownMs = 20000;
  CHECK(cloaking->levels.front().life_ms == kCloakingMs);
  CHECK(cloaking->levels.front().cooldown_ms == kCloakingCooldownMs);

  SurvivorsRun run = BareThief(rules);
  ro::sim::SurvivorsField& field = run.field_for_tests();
  Hold(field, kCloakingId);
  (void)TakeWoundingBlow(run);
  REQUIRE(field.hero.veil_until_step > field.step);
  CHECK_FALSE(field.hero.veil_sheathes);
  CHECK(field.hero.critical_due);

  // Tant qu'il dure : intouchable, il marche et frappe sans critique.
  const std::int32_t hp = field.hero.hp;
  const std::uint32_t stuck = PlaceSteadyFoe(field, ro::sim::FieldPoint{});
  StepKeepingLevel(run, kFewSteps);
  CHECK(field.hero.hp == hp);
  std::erase_if(field.foes, [&](const ro::sim::SurvivorsFoe& foe) { return foe.serial == stuck; });

  const std::uint32_t target =
      PlaceSteadyFoe(field, ro::sim::FieldPoint{ro::sim::Cells(kNearCells), 0});
  Hold(field, kDoubleAttackId);
  const ro::core::Pcg32 combat_before = field.combat_random;
  const ro::sim::FieldPoint stood_at = field.hero.position;
  StepKeepingLevel(run, 1, ro::sim::SurvivorsInput{.heading = 0});
  CHECK(field.hero.position != stood_at);
  const std::uint32_t left_steps = field.hero.veil_until_step - field.step;
  StepKeepingLevel(run, left_steps - 1);
  const ro::sim::SurvivorsNews during = run.TakeNews();
  CHECK(field.hero.hp == hp);
  CHECK(HitsOn(during, target, kDoubleAttackId) > 0U);
  CHECK(std::ranges::none_of(during.hits,
                             [](const ro::sim::SurvivorsHit& hit) { return hit.critical; }));
  CHECK(field.hero.critical_due);

  // Le premier coup en sortant est critique, et le suivant ne l'est plus : sans
  // chance de critique, rien n'a été tiré.
  std::uint32_t criticals = 0;
  std::uint32_t after_hits = 0;
  constexpr std::uint32_t kAfterSteps = 200;
  for (std::uint32_t done = 0; done < kAfterSteps; ++done) {
    StepHealed(run, 1);
    for (const ro::sim::SurvivorsHit& hit : run.TakeNews().hits) {
      if (hit.skill_id != kDoubleAttackId) continue;
      ++after_hits;
      if (hit.critical) ++criticals;
    }
  }
  CHECK(after_hits > 1U);
  CHECK(criticals == 1U);
  CHECK_FALSE(field.hero.critical_due);
  CHECK(field.combat_random == combat_before);
}

TEST_CASE("Voleur survivors : le robot le joue, et refuse une clé inconnue", "[sim][survivors]") {
  CHECK(ro::sim::BotProfileOf("mage") == &ro::sim::MageBotProfile());
  CHECK(ro::sim::BotProfileOf("knight") == &ro::sim::KnightBotProfile());
  CHECK(ro::sim::BotProfileOf("thief") == &ro::sim::ThiefBotProfile());
  CHECK(ro::sim::BotProfileOf("") == nullptr);
  CHECK(ro::sim::BotProfileOf("assassin") == nullptr);

  const ro::sim::SurvivorsRules& rules = ro::sim::ShippedSurvivorsRules();
  const ro::sim::ClassRule* thief = ro::sim::FindClass(rules, kThiefClassId);
  REQUIRE(thief != nullptr);
  const ro::sim::SurvivorsBotProfile& profile = ro::sim::ThiefBotProfile();
  // Toute sa réserve, et rien d'autre.
  CHECK(profile.skill_order.size() == thief->skill_pool.size());
  for (const std::uint16_t skill_id : thief->skill_pool) {
    CHECK(std::ranges::find(profile.skill_order, skill_id) != profile.skill_order.end());
  }
  CHECK(profile.card_order.size() == thief->card_pool.size());
  for (const std::uint16_t card_item_id : thief->card_pool) {
    CHECK(std::ranges::find(profile.card_order, card_item_id) != profile.card_order.end());
  }
  // Les deux skills de l'union tiennent dans ses premiers emplacements, et les
  // cartes de ses évolutions dans ses premières cartes.
  const auto first_skills = profile.skill_order.first(rules.caps.skill_slots);
  CHECK(std::ranges::find(first_skills, kEnvenomId) != first_skills.end());
  CHECK(std::ranges::find(first_skills, kSprinkleSandId) != first_skills.end());
  const auto first_cards = profile.card_order.first(rules.caps.card_slots);
  for (const ro::sim::EvolutionRule& evolution : rules.evolutions) {
    if (evolution.card_item_id == 0 ||
        std::ranges::find(first_skills, evolution.skill_id) == first_skills.end()) {
      continue;
    }
    INFO("évolution de " << evolution.skill_id);
    CHECK(std::ranges::find(first_cards, evolution.card_item_id) != first_cards.end());
  }

  // Deux parties de même graine, jouées par le robot, finissent à l'identique.
  constexpr std::uint32_t kMinuteSteps = 60 * ro::sim::kStepsPerSecond;
  const auto played = [&] {
    auto run = SurvivorsRun::Start(rules, ThiefStart());
    REQUIRE(run.has_value());
    ro::sim::SurvivorsBot bot(profile);
    for (std::uint32_t step = 0; step < kMinuteSteps; ++step) {
      if (run->field().phase == ro::sim::SurvivorsPhase::kOver) break;
      if (run->field().phase == ro::sim::SurvivorsPhase::kChoosing) {
        REQUIRE(run->Answer(bot.Choose(run->field())));
        continue;
      }
      run->Step(bot.Steer(run->field()));
    }
    CHECK(run->field().tally.kills > 0U);
    return ro::sim::SurvivorsDigest(run->field());
  };
  CHECK(played() == played());
}
