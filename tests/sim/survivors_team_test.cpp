#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/survivors_bot.h"
#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_run.h"

namespace ro::sim {
namespace {

constexpr std::uint64_t kSeed = 20261104;
constexpr std::uint16_t kMageClass = 1;
constexpr std::uint16_t kKnightClass = 2;
constexpr std::uint16_t kThiefClass = 3;
constexpr std::uint32_t kStepsPerSecond = 50;

[[nodiscard]] std::uint16_t FirstSkillOf(const SurvivorsRules& rules, std::uint16_t class_id) {
  const ClassRule* hero_class = FindClass(rules, class_id);
  REQUIRE(hero_class != nullptr);
  return hero_class->starter_skills.front();
}

// Un départ de `allies` alliés sur l'arène générée de la prairie, s'il y en a une.
[[nodiscard]] SurvivorsStart TeamStart(const SurvivorsRules& rules, std::size_t allies) {
  SurvivorsStart start;
  start.seed = kSeed;
  start.class_id = kMageClass;
  start.first_skill_id = FirstSkillOf(rules, kMageClass);
  for (const ArenaRule& arena : rules.arenas) {
    if (arena.terrain_id != 0) {
      start.arena_id = arena.arena_id;
      break;
    }
  }
  for (std::size_t ally = 0; ally < allies; ++ally) {
    const std::uint16_t class_id = ally % 2 == 0 ? kKnightClass : kMageClass;
    start.allies.push_back(
        SurvivorsAllyStart{.class_id = class_id, .first_skill_id = FirstSkillOf(rules, class_id)});
  }
  return start;
}

TEST_CASE("lire les orbes laisse en scène le héros qui y était, sans doublon", "[survivors]") {
  const SurvivorsRules& rules = ShippedSurvivorsRules();
  auto run = SurvivorsRun::Start(rules, TeamStart(rules, 2));
  REQUIRE(run);
  std::vector<SurvivorsOrb> from_first;
  run->Orbs(from_first);
  run->PutHeroOnStage(1);
  std::vector<SurvivorsOrb> from_second;
  run->Orbs(from_second);
  CHECK(run->field().stage == 1);
  CHECK(from_first.size() == from_second.size());
  run->PutHeroOnStage(0);
}

// Fait jouer `run` par un robot par héros, pendant `steps` pas ; chaque choix va
// à l'offre du premier robot.
void PlayByRobots(SurvivorsRun& run, std::uint32_t steps) {
  std::vector<SurvivorsBot> bots;
  for (std::size_t index = 0; index < HeroCount(run.field()); ++index) {
    const SurvivorsBotProfile* profile = BotProfileOf(ClassAt(run.field(), index)->key);
    REQUIRE(profile != nullptr);
    bots.emplace_back(*profile);
  }
  std::vector<SurvivorsInput> inputs(bots.size());
  for (std::uint32_t step = 0; step < steps && run.field().phase != SurvivorsPhase::kOver; ++step) {
    if (run.field().phase == SurvivorsPhase::kChoosing) {
      REQUIRE(run.Answer(bots.front().Choose(run.field())));
      continue;
    }
    for (std::size_t index = 0; index < bots.size(); ++index) {
      run.PutHeroOnStage(index);
      inputs[index] = bots[index].Steer(run.field());
    }
    run.PutHeroOnStage(0);
    run.StepTeam(inputs);
  }
}

TEST_CASE("une partie à quatre se rejoue à l'identique", "[survivors]") {
  const SurvivorsRules& rules = ShippedSurvivorsRules();
  const SurvivorsStart start = TeamStart(rules, 3);
  auto first = SurvivorsRun::Start(rules, start);
  auto second = SurvivorsRun::Start(rules, start);
  REQUIRE(first);
  REQUIRE(second);
  CHECK(HeroCount(first->field()) == 4);
  constexpr std::uint32_t kSteps = 60 * kStepsPerSecond;
  PlayByRobots(*first, kSteps);
  PlayByRobots(*second, kSteps);
  CHECK(first->tally() == second->tally());
  CHECK(first->field().tally.kills > 0);
  for (std::size_t index = 0; index < 4; ++index) {
    CHECK(HeroAt(first->field(), index).position == HeroAt(second->field(), index).position);
    CHECK(HeroAt(first->field(), index).hp == HeroAt(second->field(), index).hp);
  }
}

TEST_CASE("les héros naissent chacun sur sa case, autour du premier", "[survivors]") {
  const SurvivorsRules& rules = ShippedSurvivorsRules();
  auto run = SurvivorsRun::Start(rules, TeamStart(rules, 3));
  REQUIRE(run);
  const SurvivorsField& field = run->field();
  for (std::size_t first = 0; first < 4; ++first) {
    for (std::size_t second = first + 1; second < 4; ++second) {
      CHECK_FALSE(HeroAt(field, first).position == HeroAt(field, second).position);
    }
  }
  CHECK(ClassAt(field, 0)->class_id == kMageClass);
  CHECK(ClassAt(field, 1)->class_id == kKnightClass);
}

TEST_CASE("mettre un héros en scène échange tout, sauf l'équipe", "[survivors]") {
  const SurvivorsRules& rules = ShippedSurvivorsRules();
  auto run = SurvivorsRun::Start(rules, TeamStart(rules, 1));
  REQUIRE(run);
  SurvivorsField& field = run->field_for_tests();
  field.hero.level = 7;
  field.hero.zeny = 40;
  const FieldPoint first_at = field.hero.position;
  const FieldPoint second_at = HeroAt(field, 1).position;
  PutOnStage(field, 1);
  CHECK(field.stage == 1);
  CHECK(field.hero.position == second_at);
  CHECK(field.hero.level == 7);
  CHECK(field.hero.zeny == 40);
  CHECK(field.hero_class->class_id == kKnightClass);
  CHECK(HeroAt(field, 0).position == first_at);
  CHECK(HeroAt(field, 1).position == second_at);
  PutOnStage(field, 0);
  CHECK(field.hero.position == first_at);
  CHECK(field.hero_class->class_id == kMageClass);
}

TEST_CASE("un héros ne s'éloigne pas au-delà de la laisse", "[survivors]") {
  const SurvivorsRules& rules = ShippedSurvivorsRules();
  auto run = SurvivorsRun::Start(rules, TeamStart(rules, 1));
  REQUIRE(run);
  constexpr std::uint8_t kEast = 0;
  const std::array<SurvivorsInput, 2> inputs{SurvivorsInput{.heading = kEast}, SurvivorsInput{}};
  constexpr std::uint32_t kWalkSteps = 60 * kStepsPerSecond;
  for (std::uint32_t step = 0; step < kWalkSteps; ++step) {
    if (run->field().phase == SurvivorsPhase::kChoosing) {
      REQUIRE(run->Answer(SurvivorsAnswer{}));
      continue;
    }
    if (run->field().phase == SurvivorsPhase::kOver) break;
    run->StepTeam(inputs);
    const std::int64_t leash = Cells(kTeamLeashCells);
    // Une marche du second, poussé par la horde, peut mordre d'un pas sur la laisse.
    const std::int64_t tolerance = Cells(2);
    CHECK(DistanceSquared(HeroAt(run->field(), 0).position, HeroAt(run->field(), 1).position) <=
          (leash + tolerance) * (leash + tolerance));
  }
}

TEST_CASE("un héros tombé reste à terre, et le dernier clôt la partie", "[survivors]") {
  const SurvivorsRules& rules = ShippedSurvivorsRules();
  auto run = SurvivorsRun::Start(rules, TeamStart(rules, 1));
  REQUIRE(run);
  SurvivorsField& field = run->field_for_tests();
  SurvivorsFoe foe;
  foe.serial = 900;
  foe.foe_id = rules.foes.front().foe_id;
  foe.hp = 1000;
  foe.max_hp = 1000;
  field.hero.hp = 1;
  foe.position = field.hero.position;
  foe.previous_position = foe.position;
  field.foes.push_back(foe);
  const std::array<SurvivorsInput, 2> inputs{};
  for (std::uint32_t step = 0; step < kStepsPerSecond && !HeroAt(field, 0).down; ++step) {
    if (field.phase == SurvivorsPhase::kChoosing) {
      REQUIRE(run->Answer(SurvivorsAnswer{}));
      continue;
    }
    run->StepTeam(inputs);
  }
  REQUIRE(HeroAt(field, 0).down);
  CHECK(field.phase != SurvivorsPhase::kOver);
  const FieldPoint fallen_at = HeroAt(field, 0).position;
  // La horde ne vise plus que le héros debout.
  PutOnStage(field, 1);
  field.hero.hp = 1;
  PutOnStage(field, 0);
  for (std::uint32_t step = 0; step < 5 * kStepsPerSecond && field.phase != SurvivorsPhase::kOver;
       ++step) {
    if (field.phase == SurvivorsPhase::kChoosing) {
      REQUIRE(run->Answer(SurvivorsAnswer{}));
      continue;
    }
    if (field.foes.empty()) field.foes.push_back(foe);
    field.foes.front().position = HeroAt(field, 1).position;
    run->StepTeam(inputs);
  }
  CHECK(HeroAt(field, 0).position == fallen_at);
  CHECK(field.phase == SurvivorsPhase::kOver);
  CHECK(field.tally.outcome == SurvivorsOutcome::kFell);
}

// Les PV du boss final né dans une partie à deux, au pas où le héros
// `anchor_hero` tient le théâtre des apparitions.
[[nodiscard]] std::int32_t FinalBossHpOfPair(std::uint16_t first_class, std::uint16_t second_class,
                                             std::uint32_t anchor_hero) {
  const SurvivorsRules& rules = ShippedSurvivorsRules();
  SurvivorsStart start = TeamStart(rules, 1);
  start.class_id = first_class;
  start.first_skill_id = FirstSkillOf(rules, first_class);
  start.allies.front() = SurvivorsAllyStart{.class_id = second_class,
                                            .first_skill_id = FirstSkillOf(rules, second_class)};
  auto run = SurvivorsRun::Start(rules, start);
  REQUIRE(run);
  SurvivorsField& field = run->field_for_tests();
  const BossRule& final_boss = field.content.bosses.back();
  REQUIRE(final_boss.final);
  field.next_boss = field.content.bosses.size() - 1;
  field.step = final_boss.at_ms / kSurvivorsStepMs + anchor_hero;
  const std::array<SurvivorsInput, 2> inputs{};
  run->StepTeam(inputs);
  for (const SurvivorsFoe& foe : field.foes) {
    if (foe.foe_id == final_boss.foe_id) return foe.max_hp;
  }
  FAIL("le boss final n'est pas né");
  return 0;
}

TEST_CASE("le boss final d'une équipe prend la plus forte majoration de ses classes",
          "[survivors]") {
  const SurvivorsRules& rules = ShippedSurvivorsRules();
  const ClassRule* knight = FindClass(rules, kKnightClass);
  REQUIRE(knight != nullptr);
  constexpr std::int64_t kWhole = 100;
  const std::int32_t mages = FinalBossHpOfPair(kMageClass, kMageClass, 0);
  const std::int32_t mixed = FinalBossHpOfPair(kMageClass, kKnightClass, 0);
  CHECK(mixed == std::int64_t{mages} * (kWhole + knight->final_boss_hp_bonus_pct) / kWhole);
  // Ni l'ordre des héros ni celui qui tient la scène ne la changent.
  CHECK(FinalBossHpOfPair(kKnightClass, kMageClass, 0) == mixed);
  CHECK(FinalBossHpOfPair(kKnightClass, kMageClass, 1) == mixed);
  CHECK(FinalBossHpOfPair(kMageClass, kKnightClass, 1) == mixed);
  CHECK(FinalBossHpOfPair(kKnightClass, kKnightClass, 1) == mixed);
}

TEST_CASE("une équipe qui compte un Voleur hérite de sa majoration, sans dépasser la plus forte",
          "[survivors]") {
  const SurvivorsRules& rules = ShippedSurvivorsRules();
  const ClassRule* knight = FindClass(rules, kKnightClass);
  const ClassRule* thief = FindClass(rules, kThiefClass);
  REQUIRE(knight != nullptr);
  REQUIRE(thief != nullptr);
  REQUIRE(thief->final_boss_hp_bonus_pct > 0);
  REQUIRE(thief->final_boss_hp_bonus_pct < knight->final_boss_hp_bonus_pct);
  constexpr std::int64_t kWhole = 100;
  const std::int32_t mages = FinalBossHpOfPair(kMageClass, kMageClass, 0);
  CHECK(FinalBossHpOfPair(kMageClass, kThiefClass, 0) ==
        std::int64_t{mages} * (kWhole + thief->final_boss_hp_bonus_pct) / kWhole);
  // Face au Chevalier, c'est sa majoration qui vaut : ni plus, ni moins.
  CHECK(FinalBossHpOfPair(kKnightClass, kThiefClass, 0) ==
        FinalBossHpOfPair(kMageClass, kKnightClass, 0));
}

TEST_CASE("les charges de l'équipe s'ajoutent, sous leur plafond", "[survivors]") {
  const SurvivorsRules& rules = ShippedSurvivorsRules();
  SurvivorsStart start = TeamStart(rules, 3);
  start.loadout.rerolls = 3;
  start.leaves = 2;
  for (SurvivorsAllyStart& ally : start.allies) {
    ally.loadout.rerolls = 3;
    ally.leaves = 1;
  }
  auto run = SurvivorsRun::Start(rules, start);
  REQUIRE(run);
  CHECK(run->field().rerolls_left == kMostTeamCharges);
  CHECK(run->field().leaves_left == 5);
}

TEST_CASE("un départ de plus de quatre héros est refusé", "[survivors]") {
  const SurvivorsRules& rules = ShippedSurvivorsRules();
  CHECK_FALSE(SurvivorsRun::Start(rules, TeamStart(rules, 4)));
}

TEST_CASE("les offres d'une équipe nomment leur héros", "[survivors]") {
  const SurvivorsRules& rules = ShippedSurvivorsRules();
  auto run = SurvivorsRun::Start(rules, TeamStart(rules, 1));
  REQUIRE(run);
  SurvivorsField& field = run->field_for_tests();
  field.hero.exp = ExpToNext(rules, field.hero.level);
  field.pending_level_ups = 1;
  std::array<bool, 2> named = {};
  for (std::uint32_t attempt = 0; attempt < 40; ++attempt) {
    field.pending_level_ups = 1;
    run->StepTeam(std::array<SurvivorsInput, 2>{});
    REQUIRE(field.phase == SurvivorsPhase::kChoosing);
    for (const SurvivorsOffer& offer : field.offers) {
      REQUIRE(offer.hero_index < 2);
      named[offer.hero_index] = true;
      if (offer.kind == OfferKind::kNewSkill || offer.kind == OfferKind::kSkillLevel) {
        const ClassRule* hero_class = ClassAt(field, offer.hero_index);
        const bool pooled =
            std::ranges::find(hero_class->skill_pool, offer.id) != hero_class->skill_pool.end();
        CHECK(pooled);
      }
    }
    REQUIRE(run->Answer(SurvivorsAnswer{}));
    field.pending_level_ups = 0;
    field.phase = SurvivorsPhase::kRunning;
  }
  CHECK(named[0]);
  CHECK(named[1]);
}

TEST_CASE("chaque coup nomme le héros qui l'a porté", "[survivors]") {
  const SurvivorsRules& rules = ShippedSurvivorsRules();
  auto run = SurvivorsRun::Start(rules, TeamStart(rules, 1));
  REQUIRE(run);
  std::array<std::uint32_t, 2> hits_by_hero{};
  constexpr std::uint32_t kMostSteps = 1500;
  for (std::uint32_t step = 0; step < kMostSteps; ++step) {
    PlayByRobots(*run, 1);
    for (const SurvivorsHit& hit : run->TakeNews().hits) {
      REQUIRE(hit.hero_index < hits_by_hero.size());
      ++hits_by_hero[hit.hero_index];
    }
    if (run->field().phase == SurvivorsPhase::kOver) break;
  }
  CHECK(hits_by_hero[0] > 0);
  CHECK(hits_by_hero[1] > 0);
}

TEST_CASE("une gemme saisie se rapproche de celui qui l'a saisie, à chaque pas", "[survivors]") {
  const SurvivorsRules& rules = ShippedSurvivorsRules();
  auto run = SurvivorsRun::Start(rules, TeamStart(rules, 1));
  REQUIRE(run);
  std::uint32_t seen_moving = 0;
  constexpr std::uint32_t kMostSteps = 3000;
  for (std::uint32_t step = 0; step < kMostSteps; ++step) {
    PlayByRobots(*run, 1);
    const SurvivorsField& field = run->field();
    if (field.phase == SurvivorsPhase::kOver) break;
    for (const SurvivorsGem& gem : field.gems) {
      if (gem.pull_units == 0 || gem.previous_position == gem.position) continue;
      const FieldPoint puller = HeroAt(field, gem.pull_hero).position;
      CHECK(DistanceSquared(gem.position, puller) < DistanceSquared(gem.previous_position, puller));
      ++seen_moving;
    }
  }
  CHECK(seen_moving > 0);
}

}  // namespace
}  // namespace ro::sim
