// LE CHEVALIER : la seconde classe du catalogue. Il part avec plus de PV que
// le Magicien et un coup de lance, ne se voit offrir que les skills de sa
// réserve, et frappe ce qui l'approche. Ses postures le renforcent par
// moments, et sa carte Peco Peco le met en selle. Le désert, lui, joue son
// propre bestiaire.

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>

#include <catch2/catch_test_macros.hpp>

#include "ro/core/pcg32.h"
#include "ro/sim/survivors_bot.h"
#include "ro/sim/survivors_geometry.h"
#include "ro/sim/survivors_news.h"
#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_run.h"
#include "ro/sim/survivors_state.h"

#include "support/survivors_fixture.h"

namespace {

using ro::sim::SurvivorsRun;

constexpr std::uint16_t kKnightClassId = 2;
constexpr std::uint16_t kPlainsArenaId = 2;  // la prairie générée
constexpr std::uint16_t kBashId = 5;
constexpr std::uint16_t kEndureId = 8;
constexpr std::uint16_t kPecoPecoCardId = 4031;
constexpr std::uint64_t kSeed = 20261103;
// Une minute de partie : assez pour que la horde approche et monte un niveau.
constexpr std::uint32_t kOneMinuteSteps = 60 * ro::sim::kStepsPerSecond;

constexpr std::uint16_t kMageClassId = 1;
constexpr std::uint16_t kFireBoltId = 19;
constexpr std::uint16_t kMagnumBreakId = 7;
constexpr std::uint16_t kBrandishSpearId = 57;
constexpr std::uint16_t kTwoHandQuickenId = 60;
constexpr std::uint16_t kBowlingBashId = 62;
constexpr std::uint16_t kAuraBladeId = 355;
constexpr std::uint16_t kConcentrationId = 357;
constexpr std::uint16_t kFrenzyId = 359;
constexpr std::uint16_t kIgnitionBreakId = 2006;
constexpr std::uint16_t kPupaCardId = 4003;
constexpr std::uint16_t kDragonFlyCardId = 4179;
constexpr std::uint16_t kVagabondWolfCardId = 4183;
constexpr std::uint16_t kEclipseCardId = 4266;
constexpr std::uint8_t kCardTopLevel = 5;
constexpr std::int32_t kWolfTopDurationPct = 50;
constexpr std::int32_t kEclipseTopCooldownPct = 20;
constexpr std::uint16_t kChonchonId = 1011;
// Un monstre de pierre, lent et que rien ne repousse : il reste dans l'aire.
constexpr std::uint16_t kSteadyFoeId = 1040;
constexpr std::int32_t kSteadyFoeCells = 2;
// Assez de pas pour qu'un skill prêt tire et que son coup tombe, avant sa recharge.
constexpr std::uint32_t kFewSteps = 5;
// Un skill qui ne tire jamais pendant l'essai.
constexpr std::uint32_t kNever = std::numeric_limits<std::uint32_t>::max();

constexpr std::array<ro::sim::EvolutionRule, 4> kNewEvolutions{{
    {.skill_id = kMagnumBreakId,
     .card_item_id = kDragonFlyCardId,
     .evolved_skill_id = kAuraBladeId},
    {.skill_id = kEndureId, .card_item_id = kPupaCardId, .evolved_skill_id = kConcentrationId},
    {.skill_id = kTwoHandQuickenId,
     .card_item_id = kVagabondWolfCardId,
     .evolved_skill_id = kFrenzyId},
    {.skill_id = kBowlingBashId,
     .card_item_id = 0,
     .evolved_skill_id = kIgnitionBreakId,
     .partner_skill_id = kBrandishSpearId},
}};
using EvolutionCase = ro::sim::EvolutionRule;

// Mastering, Eclipse, Dragon Fly et Vagabond Wolf : des cartes verrouillées en
// partie libre, que leur mini-boss déverrouille.
constexpr std::array<std::uint16_t, 4> kCardBosses{1090, 1093, 1091, 1092};
// Peco Peco au désert, Condor sur la prairie comme au désert.
constexpr std::array<std::uint16_t, 2> kPecoPecoCarriers{1019, 1009};

[[nodiscard]] ro::sim::SurvivorsStart KnightStart() {
  return ro::sim::SurvivorsStart{.seed = kSeed,
                                 .class_id = kKnightClassId,
                                 .arena_id = kPlainsArenaId,
                                 .first_skill_id = kBashId};
}

[[nodiscard]] ro::sim::SurvivorsStart MageStart() {
  return ro::sim::SurvivorsStart{.seed = kSeed,
                                 .class_id = kMageClassId,
                                 .arena_id = kPlainsArenaId,
                                 .first_skill_id = kFireBoltId};
}

// Un Chevalier sans skill ni monstre, à l'heure où un coffre fait évoluer.
[[nodiscard]] SurvivorsRun KnightAtChestTime(const ro::sim::SurvivorsRules& rules) {
  auto run = SurvivorsRun::Start(rules, KnightStart());
  REQUIRE(run.has_value());
  ro::sim::SurvivorsField& field = run->field_for_tests();
  field.hero.skills.clear();
  field.foes.clear();
  field.step = rules.chests.evolution_from_ms / ro::sim::kSurvivorsStepMs;
  return std::move(*run);
}

[[nodiscard]] ro::sim::OwnedSkill AtTop(const ro::sim::SurvivorsField& field,
                                        std::uint16_t skill_id) {
  const ro::sim::SurvivorsSkillRule* rule = ro::sim::FindSkill(*field.rules, skill_id);
  REQUIRE(rule != nullptr);
  return ro::sim::OwnedSkill{.skill_id = skill_id,
                             .level = static_cast<std::uint8_t>(rule->levels.size()),
                             .ready_step = kNever};
}

// Le héros tient ce que l'évolution demande : ses skills au plus haut niveau,
// muets, et sa carte.
void HoldEvolutionParts(ro::sim::SurvivorsField& field, const EvolutionCase& evolution) {
  field.hero.skills.push_back(AtTop(field, evolution.skill_id));
  if (evolution.partner_skill_id != 0) {
    field.hero.skills.push_back(AtTop(field, evolution.partner_skill_id));
  } else {
    field.hero.cards.push_back(
        ro::sim::OwnedCard{.card_item_id = evolution.card_item_id, .level = 1});
  }
}

// Le héros ne monte ni ne tombe pendant ces pas.
void StepWhile(SurvivorsRun& run, std::uint32_t steps) {
  ro::sim::SurvivorsField& field = run.field_for_tests();
  for (std::uint32_t done = 0; done < steps; ++done) {
    field.hero.exp = 0;
    field.hero.hp = field.hero.max_hp;
    run.Step(ro::sim::SurvivorsInput{});
  }
}

// Un coffre sous les pieds du héros, ouvert au pas suivant.
void OpenChest(SurvivorsRun& run) {
  ro::sim::SurvivorsField& field = run.field_for_tests();
  field.pickups.push_back(ro::sim::SurvivorsPickup{.serial = field.next_serial++,
                                                   .kind = ro::sim::PickupKind::kChest,
                                                   .item_id = field.rules->chests.item_id,
                                                   .position = field.hero.position});
  StepWhile(run, 1);
}

[[nodiscard]] const ro::sim::OwnedSkill* Owned(const ro::sim::SurvivorsField& field,
                                               std::uint16_t skill_id) {
  const auto found = std::ranges::find(field.hero.skills, skill_id, &ro::sim::OwnedSkill::skill_id);
  return found != field.hero.skills.end() ? &*found : nullptr;
}

// Un monstre robuste à portée d'aire, immobile. Rend son numéro.
[[nodiscard]] std::uint32_t PlaceSteadyFoe(ro::sim::SurvivorsField& field) {
  const ro::sim::FieldPoint at =
      field.hero.position + ro::sim::FieldPoint{ro::sim::Cells(kSteadyFoeCells), 0};
  const std::uint32_t serial = ro::test_support::PlaceFoe(field, kSteadyFoeId, at);
  REQUIRE(serial != 0U);
  field.foes.back().units_per_step = 0;
  return serial;
}

[[nodiscard]] std::uint32_t HitsOn(const ro::sim::SurvivorsNews& news, std::uint32_t foe_serial,
                                   std::uint16_t skill_id) {
  return static_cast<std::uint32_t>(
      std::ranges::count_if(news.hits, [&](const ro::sim::SurvivorsHit& hit) {
        return hit.foe_serial == foe_serial && hit.skill_id == skill_id;
      }));
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
  StepWhile(run, 1);
}

[[nodiscard]] std::size_t CardsOnGround(const ro::sim::SurvivorsField& field) {
  return static_cast<std::size_t>(
      std::ranges::count_if(field.pickups, [](const ro::sim::SurvivorsPickup& pickup) {
        return pickup.kind == ro::sim::PickupKind::kCard;
      }));
}

}  // namespace

TEST_CASE("Chevalier survivors : il part avec ses PV et son coup de lance", "[sim][survivors]") {
  const ro::sim::SurvivorsRules& rules = ro::sim::ShippedSurvivorsRules();
  const ro::sim::ClassRule* knight = ro::sim::FindClass(rules, kKnightClassId);
  REQUIRE(knight != nullptr);
  CHECK(knight->max_hp > rules.classes.front().max_hp);

  auto run = SurvivorsRun::Start(rules, ro::sim::SurvivorsStart{.seed = kSeed,
                                                                .class_id = kKnightClassId,
                                                                .arena_id = kPlainsArenaId,
                                                                .first_skill_id = kBashId});
  REQUIRE(run.has_value());
  const ro::sim::SurvivorsField& field = run->field();
  CHECK(field.hero.max_hp == knight->max_hp);
  // Le coup de son épée au premier emplacement, puis son skill de départ.
  REQUIRE(field.hero.skills.size() == 2U);
  CHECK(field.hero.skills[0].skill_id == knight->weapon_skill_id);
  CHECK(field.hero.skills[1].skill_id == kBashId);

  // La partie avance ; chaque choix prend la première offre, qui doit venir
  // de sa réserve ou de celle de ses cartes.
  for (std::uint32_t step = 0; step < kOneMinuteSteps; ++step) {
    if (field.phase == ro::sim::SurvivorsPhase::kChoosing) {
      for (const ro::sim::SurvivorsOffer& offer : field.offers) {
        const bool in_pool =
            std::ranges::find(knight->skill_pool, offer.id) != knight->skill_pool.end() ||
            std::ranges::find(knight->card_pool, offer.id) != knight->card_pool.end() ||
            offer.id == 0;
        CHECK(in_pool);
      }
      REQUIRE(run->Answer(ro::sim::SurvivorsAnswer{}));
    }
    run->Step(ro::sim::SurvivorsInput{});
  }
  CHECK(field.tally.damage_dealt > 0U);
  CHECK(field.tally.kills > 0U);
}

TEST_CASE("Chevalier survivors : ses skills et ses évolutions passent le contrôle",
          "[sim][survivors]") {
  const ro::sim::SurvivorsRules& rules = ro::sim::ShippedSurvivorsRules();
  const ro::sim::ClassRule* knight = ro::sim::FindClass(rules, kKnightClassId);
  REQUIRE(knight != nullptr);
  for (const std::uint16_t skill_id : knight->skill_pool) {
    INFO("skill " << skill_id);
    CHECK(ro::sim::FindSkill(rules, skill_id) != nullptr);
  }
  // Chacune de ses évolutions part d'un skill de sa réserve, et aucune réserve
  // de classe ne donne un évolué.
  std::size_t evolutions = 0;
  for (const ro::sim::EvolutionRule& evolution : rules.evolutions) {
    if (std::ranges::find(knight->skill_pool, evolution.skill_id) == knight->skill_pool.end()) {
      continue;
    }
    ++evolutions;
    CHECK(ro::sim::FindSkill(rules, evolution.evolved_skill_id) != nullptr);
  }
  CHECK(evolutions == 7U);
}

TEST_CASE("Chevalier survivors : chaque évolution remplace ses skills par l'évolué",
          "[sim][survivors]") {
  const ro::sim::SurvivorsRules& rules = ro::sim::ShippedSurvivorsRules();
  const ro::sim::ClassRule* knight = ro::sim::FindClass(rules, kKnightClassId);
  REQUIRE(knight != nullptr);
  for (const EvolutionCase& evolution : kNewEvolutions) {
    INFO("évolué " << evolution.evolved_skill_id);
    SurvivorsRun run = KnightAtChestTime(rules);
    ro::sim::SurvivorsField& field = run.field_for_tests();
    HoldEvolutionParts(field, evolution);
    OpenChest(run);

    CHECK(field.hero.evolutions == 1U);
    const ro::sim::OwnedSkill* evolved = Owned(field, evolution.evolved_skill_id);
    REQUIRE(evolved != nullptr);
    CHECK(evolved->level == 1U);
    CHECK(Owned(field, evolution.skill_id) == nullptr);
    if (evolution.partner_skill_id != 0) {
      CHECK(Owned(field, evolution.partner_skill_id) == nullptr);
      CHECK(field.hero.skills.size() == 1U);
    } else {
      CHECK(std::ranges::find(knight->card_pool, evolution.card_item_id) !=
            knight->card_pool.end());
    }
    // Un évolué ne s'offre pas.
    CHECK(std::ranges::find(knight->skill_pool, evolution.evolved_skill_id) ==
          knight->skill_pool.end());
  }
}

TEST_CASE("Chevalier survivors : sans sa carte ni son second skill, rien n'évolue",
          "[sim][survivors]") {
  const ro::sim::SurvivorsRules& rules = ro::sim::ShippedSurvivorsRules();
  for (const EvolutionCase& evolution : kNewEvolutions) {
    INFO("évolué " << evolution.evolved_skill_id);
    SurvivorsRun run = KnightAtChestTime(rules);
    ro::sim::SurvivorsField& field = run.field_for_tests();
    HoldEvolutionParts(field, evolution);
    field.hero.cards.clear();
    if (evolution.partner_skill_id != 0) field.hero.skills.pop_back();
    OpenChest(run);
    CHECK(field.hero.evolutions == 0U);
    CHECK(Owned(field, evolution.evolved_skill_id) == nullptr);
  }
}

TEST_CASE("Chevalier survivors : Aura Blade passe Magnum Break et blesse tout autour",
          "[sim][survivors]") {
  const ro::sim::SurvivorsRules& rules = ro::sim::ShippedSurvivorsRules();
  const ro::sim::SurvivorsSkillRule* magnum = ro::sim::FindSkill(rules, kMagnumBreakId);
  const ro::sim::SurvivorsSkillRule* aura = ro::sim::FindSkill(rules, kAuraBladeId);
  REQUIRE(magnum != nullptr);
  REQUIRE(aura != nullptr);
  CHECK(aura->archetype == ro::sim::SkillArchetype::kNova);
  CHECK(aura->levels.front().damage > magnum->levels.back().damage);
  CHECK(aura->levels.front().area_radius_units > magnum->levels.back().area_radius_units);

  SurvivorsRun run = KnightAtChestTime(rules);
  ro::sim::SurvivorsField& field = run.field_for_tests();
  HoldEvolutionParts(field, kNewEvolutions[0]);
  const std::uint32_t serial = PlaceSteadyFoe(field);
  OpenChest(run);
  (void)run.TakeNews();
  StepWhile(run, kFewSteps);
  const ro::sim::SurvivorsNews news = run.TakeNews();
  CHECK(HitsOn(news, serial, kAuraBladeId) == 1U);
}

TEST_CASE("Chevalier survivors : l'union frappe en trois vagues et libère un emplacement",
          "[sim][survivors]") {
  const ro::sim::SurvivorsRules& rules = ro::sim::ShippedSurvivorsRules();
  const ro::sim::SurvivorsSkillRule* blast = ro::sim::FindSkill(rules, kIgnitionBreakId);
  REQUIRE(blast != nullptr);
  CHECK(blast->archetype == ro::sim::SkillArchetype::kStorm);
  CHECK(blast->target == ro::sim::TargetRule::kHero);
  constexpr std::uint32_t kWaves = 3;
  REQUIRE(blast->levels.front().amount == kWaves);

  SurvivorsRun run = KnightAtChestTime(rules);
  ro::sim::SurvivorsField& field = run.field_for_tests();
  // Un skill entre les deux, pour qu'il suive l'emplacement libéré.
  HoldEvolutionParts(field, kNewEvolutions[3]);
  field.hero.skills.insert(
      field.hero.skills.begin() + 1,
      ro::sim::OwnedSkill{.skill_id = kEndureId, .level = 1, .ready_step = kNever});
  const std::uint32_t serial = PlaceSteadyFoe(field);
  OpenChest(run);
  REQUIRE(field.hero.skills.size() == 2U);
  CHECK(field.hero.skills[0].skill_id == kIgnitionBreakId);
  CHECK(field.hero.skills[1].skill_id == kEndureId);

  (void)run.TakeNews();
  std::uint32_t hits = 0;
  const std::uint32_t waves_steps =
      kWaves * ro::sim::StepsOf(blast->levels.front().interval_ms) + kFewSteps;
  for (std::uint32_t done = 0; done < waves_steps; ++done) {
    StepWhile(run, 1);
    hits += HitsOn(run.TakeNews(), serial, kIgnitionBreakId);
  }
  CHECK(hits == kWaves);
}

TEST_CASE("Chevalier survivors : ses postures évoluées majorent ses dégâts par moments",
          "[sim][survivors]") {
  const ro::sim::SurvivorsRules& rules = ro::sim::ShippedSurvivorsRules();
  const ro::sim::SurvivorsSkillRule* concentration = ro::sim::FindSkill(rules, kConcentrationId);
  const ro::sim::SurvivorsSkillRule* frenzy = ro::sim::FindSkill(rules, kFrenzyId);
  REQUIRE(concentration != nullptr);
  REQUIRE(frenzy != nullptr);
  CHECK(concentration->stance_stat == ro::sim::SurvivorsStat::kDamagePct);
  CHECK(frenzy->stance_stat == ro::sim::SurvivorsStat::kDamagePct);
  CHECK(frenzy->levels.front().stance_bonus > 2 * concentration->levels.front().stance_bonus);

  for (const ro::sim::SurvivorsSkillRule* stance : {concentration, frenzy}) {
    INFO("posture " << stance->key);
    const ro::sim::SkillLevel& level = stance->levels.front();
    REQUIRE(level.cooldown_ms > level.life_ms);
    SurvivorsRun run = KnightAtChestTime(rules);
    ro::sim::SurvivorsField& field = run.field_for_tests();
    field.hero.skills.push_back(
        ro::sim::OwnedSkill{.skill_id = stance->skill_id, .level = 1, .ready_step = field.step});
    const std::int32_t bare = field.hero.stats.Of(ro::sim::SurvivorsStat::kDamagePct);
    StepWhile(run, 1);
    CHECK(field.hero.stats.Of(ro::sim::SurvivorsStat::kDamagePct) == bare + level.stance_bonus);
    StepWhile(run, ro::sim::StepsOf(level.life_ms));
    CHECK(field.hero.stats.Of(ro::sim::SurvivorsStat::kDamagePct) == bare);
    StepWhile(run, ro::sim::StepsOf(level.cooldown_ms) - ro::sim::StepsOf(level.life_ms));
    CHECK(field.hero.stats.Of(ro::sim::SurvivorsStat::kDamagePct) == bare + level.stance_bonus);
  }
}

TEST_CASE("Chevalier survivors : ses postures retombent sous les cartes au plafond",
          "[sim][survivors]") {
  const ro::sim::SurvivorsRules& rules = ro::sim::ShippedSurvivorsRules();
  constexpr std::uint32_t kWantedRests = 2;
  const std::uint32_t least_rest_steps = ro::sim::StepsOf(rules.tuning.stance_least_rest_ms);
  REQUIRE(least_rest_steps > 0U);
  // Le dernier cas : Two-Hand Quicken raccourcit en plus la recharge de Concentration.
  struct StanceCase {
    std::uint16_t skill_id;
    std::uint16_t beside_id;
  };
  constexpr std::uint16_t kNothingBeside = 0;
  constexpr std::array<StanceCase, 5> cases{{{kEndureId, kNothingBeside},
                                             {kTwoHandQuickenId, kNothingBeside},
                                             {kConcentrationId, kNothingBeside},
                                             {kFrenzyId, kNothingBeside},
                                             {kConcentrationId, kTwoHandQuickenId}}};
  for (const auto& [skill_id, beside_id] : cases) {
    INFO("posture " << skill_id << " à côté de " << beside_id);
    SurvivorsRun run = KnightAtChestTime(rules);
    ro::sim::SurvivorsField& field = run.field_for_tests();
    field.hero.cards.push_back(
        ro::sim::OwnedCard{.card_item_id = kVagabondWolfCardId, .level = kCardTopLevel});
    field.hero.cards.push_back(
        ro::sim::OwnedCard{.card_item_id = kEclipseCardId, .level = kCardTopLevel});
    for (const std::uint16_t held_id : {skill_id, beside_id}) {
      if (held_id == kNothingBeside) continue;
      field.hero.skills.push_back(AtTop(field, held_id));
      field.hero.skills.back().ready_step = field.step;
    }
    // Le premier allumage refait les statistiques : les cartes comptent ensuite.
    StepWhile(run, 1);
    REQUIRE(field.hero.stats.Of(ro::sim::SurvivorsStat::kDurationPct) == kWolfTopDurationPct);
    REQUIRE(field.hero.stats.Of(ro::sim::SurvivorsStat::kCooldownPct) >= kEclipseTopCooldownPct);
    std::uint32_t rests = 0;
    std::uint32_t rest_steps = 0;
    for (std::uint32_t done = 0; done < kOneMinuteSteps; ++done) {
      StepWhile(run, 1);
      if (Owned(field, skill_id)->stance_until_step == 0) {
        ++rest_steps;
      } else if (rest_steps > 0) {
        CHECK(rest_steps >= least_rest_steps);
        ++rests;
        rest_steps = 0;
      }
    }
    CHECK(rests >= kWantedRests);
  }
}

TEST_CASE("Chevalier survivors : une posture active évolue sans laisser son bonus",
          "[sim][survivors]") {
  const ro::sim::SurvivorsRules& rules = ro::sim::ShippedSurvivorsRules();
  const ro::sim::SurvivorsSkillRule* endure = ro::sim::FindSkill(rules, kEndureId);
  const ro::sim::SurvivorsSkillRule* concentration = ro::sim::FindSkill(rules, kConcentrationId);
  REQUIRE(endure != nullptr);
  REQUIRE(concentration != nullptr);

  SurvivorsRun run = KnightAtChestTime(rules);
  ro::sim::SurvivorsField& field = run.field_for_tests();
  const std::int32_t bare_armor = field.hero.stats.Of(ro::sim::SurvivorsStat::kArmor);
  const std::int32_t bare_damage = field.hero.stats.Of(ro::sim::SurvivorsStat::kDamagePct);
  HoldEvolutionParts(field, kNewEvolutions[1]);
  field.hero.skills.front().ready_step = field.step;
  StepWhile(run, 1);
  REQUIRE(field.hero.stats.Of(ro::sim::SurvivorsStat::kArmor) ==
          bare_armor + endure->levels.back().stance_bonus);

  // Le coffre : Endure s'efface avec son armure, en pleine posture.
  OpenChest(run);
  REQUIRE(Owned(field, kConcentrationId) != nullptr);
  CHECK(field.hero.stats.Of(ro::sim::SurvivorsStat::kArmor) == bare_armor);
  CHECK(field.hero.stats.Of(ro::sim::SurvivorsStat::kDamagePct) == bare_damage);

  // L'évolué, prêt à tirer, prend au pas suivant.
  (void)run.TakeNews();
  StepWhile(run, 1);
  CHECK(field.hero.stats.Of(ro::sim::SurvivorsStat::kArmor) == bare_armor);
  CHECK(field.hero.stats.Of(ro::sim::SurvivorsStat::kDamagePct) ==
        bare_damage + concentration->levels.front().stance_bonus);
  const ro::sim::SurvivorsNews news = run.TakeNews();
  CHECK(std::ranges::any_of(news.casts, [](const ro::sim::SurvivorsCast& cast) {
    return cast.kind == ro::sim::CastKind::kStanceOn && cast.skill_id == kConcentrationId;
  }));
}

TEST_CASE("Chevalier survivors : les cartes de ses évolutions lui restent obtenables",
          "[sim][survivors]") {
  const ro::sim::SurvivorsRules& rules = ro::sim::ShippedSurvivorsRules();
  const ro::sim::ClassRule* knight = ro::sim::FindClass(rules, kKnightClassId);
  REQUIRE(knight != nullptr);
  for (const std::uint16_t boss_id : kCardBosses) {
    INFO("mini-boss " << boss_id);
    const ro::sim::FoeRule* foe = ro::sim::FindFoe(rules, boss_id);
    REQUIRE(foe != nullptr);
    CHECK(foe->rank == ro::sim::FoeRank::kMiniBoss);
    CHECK(std::ranges::find(knight->card_pool, foe->card_item_id) != knight->card_pool.end());
    CHECK(std::ranges::any_of(
        rules.bosses, [&](const ro::sim::BossRule& boss) { return boss.foe_id == boss_id; }));
    // Sa carte lui est garantie, comme au Magicien : c'est ce qui la déverrouille.
    SurvivorsRun run = KnightAtChestTime(rules);
    ro::sim::SurvivorsField& field = run.field_for_tests();
    const std::size_t cards_before = CardsOnGround(field);
    Kill(run, boss_id);
    CHECK(CardsOnGround(field) == cards_before + 1U);
    CHECK(std::ranges::any_of(field.pickups, [&](const ro::sim::SurvivorsPickup& pickup) {
      return pickup.kind == ro::sim::PickupKind::kCard && pickup.item_id == foe->card_item_id;
    }));
  }
}

TEST_CASE("Peco Peco Card : elle tombe du Peco Peco et du Condor, pour le Chevalier seul",
          "[sim][survivors]") {
  // Une cote de un : chaque tirage fait tomber la carte.
  ro::sim::SurvivorsRules rules = ro::sim::ShippedSurvivorsRules();
  rules.card_drops.common_odds = 1;
  const ro::sim::ClassRule& mage = rules.classes.front();
  CHECK(std::ranges::find(mage.card_pool, kPecoPecoCardId) == mage.card_pool.end());

  for (const std::uint16_t foe_id : kPecoPecoCarriers) {
    INFO("monstre " << foe_id);
    const ro::sim::FoeRule* foe = ro::sim::FindFoe(rules, foe_id);
    REQUIRE(foe != nullptr);
    CHECK(foe->card_item_id == kPecoPecoCardId);

    SECTION("le Chevalier la voit tomber") {
      auto run = SurvivorsRun::Start(rules, KnightStart());
      REQUIRE(run.has_value());
      ro::sim::SurvivorsField& field = run->field_for_tests();
      field.foes.clear();
      const ro::core::Pcg32 before = field.card_random;
      Kill(*run, foe_id);
      CHECK(field.card_random != before);
      CHECK(CardsOnGround(field) == 1U);
    }

    SECTION("le Magicien ne tire rien et ne voit rien tomber") {
      auto run = SurvivorsRun::Start(rules, MageStart());
      REQUIRE(run.has_value());
      ro::sim::SurvivorsField& field = run->field_for_tests();
      field.foes.clear();
      const ro::core::Pcg32 before = field.card_random;
      const std::uint32_t card_serial = field.next_card_serial;
      Kill(*run, foe_id);
      CHECK(field.card_random == before);
      CHECK(field.next_card_serial == card_serial);
      CHECK(CardsOnGround(field) == 0U);
    }

    SECTION("un Chevalier allié la fait tomber pour le Magicien") {
      ro::sim::SurvivorsStart start = MageStart();
      start.allies.push_back(
          ro::sim::SurvivorsAllyStart{.class_id = kKnightClassId, .first_skill_id = kBashId});
      auto run = SurvivorsRun::Start(rules, start);
      REQUIRE(run.has_value());
      ro::sim::SurvivorsField& field = run->field_for_tests();
      field.foes.clear();
      Kill(*run, foe_id);
      CHECK(CardsOnGround(field) == 1U);
    }
  }

  SECTION("le Chevalier ne tire plus la carte de Chonchon, hors de sa réserve") {
    auto run = SurvivorsRun::Start(rules, KnightStart());
    REQUIRE(run.has_value());
    ro::sim::SurvivorsField& field = run->field_for_tests();
    field.foes.clear();
    const ro::core::Pcg32 before = field.card_random;
    Kill(*run, kChonchonId);
    CHECK(field.card_random == before);
    CHECK(CardsOnGround(field) == 0U);
  }

  SECTION("le Magicien tire toujours les cartes de sa réserve") {
    auto run = SurvivorsRun::Start(rules, MageStart());
    REQUIRE(run.has_value());
    ro::sim::SurvivorsField& field = run->field_for_tests();
    field.foes.clear();
    Kill(*run, kChonchonId);
    CHECK(CardsOnGround(field) == 1U);
  }
}

TEST_CASE("Chevalier survivors : le robot connaît ses évolutions", "[sim][survivors]") {
  const ro::sim::SurvivorsRules& rules = ro::sim::ShippedSurvivorsRules();
  const ro::sim::ClassRule* knight = ro::sim::FindClass(rules, kKnightClassId);
  REQUIRE(knight != nullptr);
  const ro::sim::SurvivorsBotProfile& profile = ro::sim::KnightBotProfile();
  // Toute sa réserve, et rien d'autre.
  CHECK(profile.skill_order.size() == knight->skill_pool.size());
  for (const std::uint16_t skill_id : knight->skill_pool) {
    CHECK(std::ranges::find(profile.skill_order, skill_id) != profile.skill_order.end());
  }
  CHECK(profile.card_order.size() == knight->card_pool.size());
  for (const std::uint16_t card_item_id : knight->card_pool) {
    CHECK(std::ranges::find(profile.card_order, card_item_id) != profile.card_order.end());
  }
  // Les deux skills de l'union tiennent dans ses premiers emplacements, et les
  // cartes de ses évolutions dans ses premières cartes.
  const auto first_skills = profile.skill_order.first(rules.caps.skill_slots);
  CHECK(std::ranges::find(first_skills, kBowlingBashId) != first_skills.end());
  CHECK(std::ranges::find(first_skills, kBrandishSpearId) != first_skills.end());
  const auto first_cards = profile.card_order.first(rules.caps.card_slots);
  for (const ro::sim::EvolutionRule& evolution : rules.evolutions) {
    if (evolution.card_item_id == 0 ||
        std::ranges::find(first_skills, evolution.skill_id) == first_skills.end()) {
      continue;
    }
    INFO("évolution de " << evolution.skill_id);
    CHECK(std::ranges::find(first_cards, evolution.card_item_id) != first_cards.end());
  }
}

TEST_CASE("Chevalier survivors : Endure arme le héros par moments", "[sim][survivors]") {
  const ro::sim::SurvivorsRules& rules = ro::sim::ShippedSurvivorsRules();
  const ro::sim::SurvivorsSkillRule* endure = ro::sim::FindSkill(rules, kEndureId);
  REQUIRE(endure != nullptr);
  REQUIRE(endure->archetype == ro::sim::SkillArchetype::kStance);
  const ro::sim::SkillLevel& first = endure->levels.front();
  REQUIRE(first.cooldown_ms > first.life_ms);

  // Une partie part sur un coup : Endure s'ajoute à côté de Bash.
  auto run = SurvivorsRun::Start(rules, ro::sim::SurvivorsStart{.seed = kSeed,
                                                                .class_id = kKnightClassId,
                                                                .arena_id = kPlainsArenaId,
                                                                .first_skill_id = kBashId});
  REQUIRE(run.has_value());
  ro::sim::SurvivorsField& field = run->field_for_tests();
  field.hero.skills.push_back(
      ro::sim::OwnedSkill{.skill_id = kEndureId, .level = 1, .ready_step = field.step});
  const std::int32_t bare_armor = field.hero.stats.Of(ro::sim::SurvivorsStat::kArmor);
  // Le héros ne monte ni ne tombe : la posture reste au niveau 1, seule.
  const auto step = [&] {
    field.hero.exp = 0;
    field.hero.hp = field.hero.max_hp;
    run->Step(ro::sim::SurvivorsInput{});
  };

  // Elle prend au premier pas, et le dit.
  step();
  CHECK(field.hero.stats.Of(ro::sim::SurvivorsStat::kArmor) == bare_armor + first.stance_bonus);
  const ro::sim::SurvivorsNews news = run->TakeNews();
  CHECK(std::ranges::any_of(news.casts, [](const ro::sim::SurvivorsCast& cast) {
    return cast.kind == ro::sim::CastKind::kStanceOn && cast.skill_id == kEndureId;
  }));

  // Elle s'éteint à son terme, et reprend à sa recharge.
  const std::uint32_t life_steps = ro::sim::StepsOf(first.life_ms);
  const std::uint32_t cooldown_steps = ro::sim::StepsOf(first.cooldown_ms);
  for (std::uint32_t done = 1; done <= life_steps; ++done) step();
  CHECK(field.hero.stats.Of(ro::sim::SurvivorsStat::kArmor) == bare_armor);
  for (std::uint32_t done = life_steps + 1; done <= cooldown_steps; ++done) step();
  CHECK(field.hero.stats.Of(ro::sim::SurvivorsStat::kArmor) == bare_armor + first.stance_bonus);
}

TEST_CASE("Chevalier survivors : la carte Peco Peco le met en selle", "[sim][survivors]") {
  const ro::sim::SurvivorsRules& rules = ro::sim::ShippedSurvivorsRules();
  const ro::sim::ClassRule* knight = ro::sim::FindClass(rules, kKnightClassId);
  REQUIRE(knight != nullptr);
  REQUIRE(knight->mount_card_item_id == kPecoPecoCardId);
  CHECK(ro::sim::FindCard(rules, kPecoPecoCardId) != nullptr);
  CHECK(std::ranges::find(knight->card_pool, kPecoPecoCardId) != knight->card_pool.end());

  ro::sim::SurvivorsHero hero;
  CHECK(ro::sim::HeroJobOf(*knight, hero) == knight->job_id);
  hero.cards.push_back(ro::sim::OwnedCard{.card_item_id = kPecoPecoCardId, .level = 1});
  CHECK(ro::sim::HeroJobOf(*knight, hero) == knight->mounted_job_id);
  hero.evolutions = 1;
  CHECK(ro::sim::HeroJobOf(*knight, hero) == knight->mounted_job_id);
  hero.level = knight->reborn_level;
  CHECK(ro::sim::HeroJobOf(*knight, hero) == knight->reborn_mounted_job_id);
  hero.cards.clear();
  CHECK(ro::sim::HeroJobOf(*knight, hero) == knight->reborn_job_id);

  // Le Magicien n'a pas de monture : la carte ne change pas son corps.
  const ro::sim::ClassRule& mage = rules.classes.front();
  hero.level = 1;
  hero.cards.push_back(ro::sim::OwnedCard{.card_item_id = kPecoPecoCardId, .level = 1});
  CHECK(ro::sim::HeroJobOf(mage, hero) == mage.evolved_job_id);
}

TEST_CASE("Survivors : chaque classe renaît au niveau 50", "[sim][survivors]") {
  // Chaque classe porte sa 2e classe dès le départ, puis sa transcendante :
  // Wizard puis High Wizard ; Knight puis Lord Knight ; Assassin puis Assassin
  // Cross ; Hunter puis Sniper ; Alchemist puis Creator.
  constexpr std::uint16_t kRebornLevel = 50;
  constexpr std::array<std::array<std::uint16_t, 3>, 5> kBodies{
      {{9, 9, 4010}, {7, 7, 4008}, {12, 12, 4013}, {11, 11, 4012}, {18, 18, 4019}}};
  const ro::sim::SurvivorsRules& rules = ro::sim::ShippedSurvivorsRules();
  REQUIRE(rules.classes.size() == kBodies.size());
  for (std::size_t index = 0; index < kBodies.size(); ++index) {
    const ro::sim::ClassRule& hero_class = rules.classes[index];
    INFO(hero_class.key);
    ro::sim::SurvivorsHero hero;
    CHECK(ro::sim::HeroJobOf(hero_class, hero) == kBodies[index][0]);
    hero.evolutions = 1;
    hero.level = kRebornLevel - 1;
    CHECK(ro::sim::HeroJobOf(hero_class, hero) == kBodies[index][1]);
    CHECK_FALSE(ro::sim::Reborn(hero_class, hero.level));
    hero.level = kRebornLevel;
    CHECK(ro::sim::HeroJobOf(hero_class, hero) == kBodies[index][2]);
    CHECK(ro::sim::Reborn(hero_class, hero.level));
    CHECK(hero_class.reborn_damage_pct > 0);
  }
}

TEST_CASE("Survivors : la renaissance se dit et majore les dégâts", "[sim][survivors]") {
  const ro::sim::SurvivorsRules& rules = ro::sim::ShippedSurvivorsRules();
  auto run = SurvivorsRun::Start(rules, ro::sim::SurvivorsStart{.seed = kSeed,
                                                                .class_id = kKnightClassId,
                                                                .arena_id = kPlainsArenaId,
                                                                .first_skill_id = kBashId});
  REQUIRE(run.has_value());
  ro::sim::SurvivorsField& field = run->field_for_tests();
  REQUIRE(field.hero_class != nullptr);
  const ro::sim::ClassRule& knight = *field.hero_class;
  const std::int32_t before = field.hero.stats.Of(ro::sim::SurvivorsStat::kDamagePct);
  // Un niveau sous la renaissance, et une gemme sous ses pieds qui en donne
  // largement un de plus.
  constexpr std::uint32_t kPlentyExp = 1000000;
  field.hero.level = static_cast<std::uint16_t>(knight.reborn_level - 1);
  (void)run->TakeNews();
  field.gems.push_back(ro::sim::SurvivorsGem{.position = field.hero.position,
                                             .previous_position = field.hero.position,
                                             .exp = kPlentyExp});
  run->Step(ro::sim::SurvivorsInput{});
  REQUIRE(field.hero.level >= knight.reborn_level);
  CHECK(run->TakeNews().reborn_by_hero[0]);
  CHECK(field.hero.stats.Of(ro::sim::SurvivorsStat::kDamagePct) ==
        before + knight.reborn_damage_pct);
  CHECK(ro::sim::HeroJobOf(knight, field.hero) == knight.reborn_job_id);
}

// Les PV du boss final de la prairie, né sous les pieds d'une partie menée
// jusqu'à son heure.
[[nodiscard]] std::int32_t FinalBossHpFacing(std::uint16_t class_id, std::uint16_t first_skill_id) {
  const ro::sim::SurvivorsRules& rules = ro::sim::ShippedSurvivorsRules();
  auto run = SurvivorsRun::Start(rules, ro::sim::SurvivorsStart{.seed = kSeed,
                                                                .class_id = class_id,
                                                                .arena_id = kPlainsArenaId,
                                                                .first_skill_id = first_skill_id});
  REQUIRE(run.has_value());
  ro::sim::SurvivorsField& field = run->field_for_tests();
  const ro::sim::BossRule& final_boss = field.content.bosses.back();
  REQUIRE(final_boss.final);
  field.next_boss = field.content.bosses.size() - 1;
  field.step = final_boss.at_ms / ro::sim::kSurvivorsStepMs;
  run->Step(ro::sim::SurvivorsInput{});
  for (const ro::sim::SurvivorsFoe& foe : field.foes) {
    if (foe.foe_id == final_boss.foe_id) return foe.max_hp;
  }
  FAIL("le boss final n'est pas né");
  return 0;
}

TEST_CASE("Chevalier survivors : le boss final a ses PV majorés, pas face au Magicien",
          "[sim][survivors]") {
  const ro::sim::SurvivorsRules& rules = ro::sim::ShippedSurvivorsRules();
  const ro::sim::ClassRule* knight = ro::sim::FindClass(rules, kKnightClassId);
  const ro::sim::ClassRule* mage = ro::sim::FindClass(rules, kMageClassId);
  REQUIRE(knight != nullptr);
  REQUIRE(mage != nullptr);
  REQUIRE(knight->final_boss_hp_bonus_pct > 0);
  CHECK(mage->final_boss_hp_bonus_pct == 0);

  const std::int32_t facing_mage = FinalBossHpFacing(kMageClassId, kFireBoltId);
  const std::int32_t facing_knight = FinalBossHpFacing(kKnightClassId, kBashId);
  constexpr std::int64_t kWhole = 100;
  CHECK(facing_knight ==
        std::int64_t{facing_mage} * (kWhole + knight->final_boss_hp_bonus_pct) / kWhole);

  // Le Magicien l'affronte tel que le contenu le décrit : ses PV de base,
  // grandis des seules minutes écoulées.
  const ro::sim::BossRule& final_boss = rules.bosses.back();
  const ro::sim::FoeRule* foe = ro::sim::FindFoe(rules, final_boss.foe_id);
  REQUIRE(foe != nullptr);
  const std::int64_t minutes =
      final_boss.at_ms / ro::sim::kSurvivorsStepMs / ro::sim::kStepsPerMinute;
  CHECK(facing_mage == std::int64_t{foe->hp} *
                           (kWhole + rules.tuning.boss_hp_growth_pct_per_minute * minutes) /
                           kWhole);
}

TEST_CASE("Désert survivors : l'arène joue son propre bestiaire", "[sim][survivors]") {
  // La première vague du désert : Picky, Baby Desert Wolf et Poring ; aucun
  // monstre de la prairie n'y naît.
  constexpr std::uint16_t kDesertArenaId = 3;
  constexpr std::array<std::uint16_t, 3> kFirstWave{1049, 1107, 1002};
  constexpr std::uint32_t kHalfMinuteSteps = 30 * ro::sim::kStepsPerSecond;
  const ro::sim::SurvivorsRules& rules = ro::sim::ShippedSurvivorsRules();
  auto run = SurvivorsRun::Start(rules, ro::sim::SurvivorsStart{.seed = kSeed,
                                                                .class_id = kMageClassId,
                                                                .arena_id = kDesertArenaId,
                                                                .first_skill_id = kFireBoltId});
  REQUIRE(run.has_value());
  const ro::sim::SurvivorsField& field = run->field();
  CHECK(field.content.waves.data() != rules.waves.data());
  for (std::uint32_t step = 0; step < kHalfMinuteSteps; ++step) {
    if (field.phase == ro::sim::SurvivorsPhase::kChoosing) {
      REQUIRE(run->Answer(ro::sim::SurvivorsAnswer{}));
    }
    run->Step(ro::sim::SurvivorsInput{});
  }
  REQUIRE_FALSE(field.foes.empty());
  for (const ro::sim::SurvivorsFoe& foe : field.foes) {
    INFO("monstre " << foe.foe_id);
    CHECK(std::ranges::find(kFirstWave, foe.foe_id) != kFirstWave.end());
  }
}
