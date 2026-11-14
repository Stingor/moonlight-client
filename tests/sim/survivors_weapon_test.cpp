// LE COUP DE L'ARME : toute classe livrée frappe aussi de son arme, au premier
// emplacement, sans offre ni coffre, et son arme change à la première
// évolution. Trois cartes ne touchent que lui — recharge, perforation, rebond —
// et Smokie, le critique, entre dans toutes les réserves.

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/survivors_geometry.h"
#include "ro/sim/survivors_news.h"
#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_run.h"
#include "ro/sim/survivors_state.h"
#include "ro/sim/survivors_trophies.h"

#include "support/survivors_fixture.h"

namespace {

using ro::sim::SurvivorsRun;
using ro::sim::SurvivorsStat;

constexpr std::uint16_t kThiefClassId = 3;
constexpr std::uint16_t kArcherClassId = 4;
constexpr std::uint16_t kMerchantClassId = 5;
constexpr std::uint16_t kJurItemId = 1250;
constexpr std::uint16_t kHollgrehennHammerItemId = 1535;
constexpr std::uint16_t kPlainsArenaId = 2;
constexpr std::uint64_t kSeed = 20261110;

constexpr std::uint16_t kMageClassId = 1;
constexpr std::uint16_t kFireBoltId = 19;
constexpr std::uint16_t kDoubleAttackId = 48;
constexpr std::uint16_t kDoubleStrafeId = 46;
constexpr std::uint16_t kDaggerStabId = 60002;
constexpr std::uint16_t kBowShotId = 60005;

constexpr std::uint16_t kSmokieCardId = 4044;
constexpr std::uint16_t kMantisCardId = 4079;
constexpr std::uint16_t kOrcSkeletonCardId = 4085;
constexpr std::uint16_t kNightmareCardId = 4127;

// La recharge du coup de dague, en pas de 20 ms : 600 ms.
constexpr std::uint32_t kDaggerCooldownSteps = 30;
constexpr std::int32_t kDelayCutPct = 40;
constexpr std::int32_t kWholePct = 100;

constexpr std::uint16_t kSteadyFoeId = 1040;
constexpr std::uint16_t kBaphometId = 1039;
constexpr std::int32_t kSteadyFoeHp = 1000000;
// Assez de pas pour qu'une flèche traverse dix cases.
constexpr std::uint32_t kFlightSteps = 50;
constexpr std::uint32_t kFewSteps = 5;

[[nodiscard]] SurvivorsRun Bare(const ro::sim::SurvivorsRules& rules, std::uint16_t class_id,
                                std::uint16_t first_skill_id) {
  auto run = SurvivorsRun::Start(rules, ro::sim::SurvivorsStart{.seed = kSeed,
                                                                .class_id = class_id,
                                                                .arena_id = kPlainsArenaId,
                                                                .first_skill_id = first_skill_id});
  REQUIRE(run.has_value());
  ro::sim::SurvivorsField& field = run->field_for_tests();
  field.hero.skills.clear();
  field.foes.clear();
  field.plants.clear();
  return std::move(*run);
}

void Hold(ro::sim::SurvivorsField& field, std::uint16_t skill_id, std::uint8_t level = 1) {
  field.hero.skills.push_back(
      ro::sim::OwnedSkill{.skill_id = skill_id, .level = level, .ready_step = field.step});
}

void SetStat(ro::sim::SurvivorsField& field, SurvivorsStat stat, std::int32_t value) {
  field.hero.stats.values[static_cast<std::size_t>(stat)] = value;
}

// Le héros ne monte ni ne tombe pendant ces pas, et aucun monstre ne naît.
void StepQuietly(SurvivorsRun& run, std::uint32_t steps) {
  ro::sim::SurvivorsField& field = run.field_for_tests();
  for (std::uint32_t done = 0; done < steps; ++done) {
    field.hero.exp = 0;
    field.gems.clear();
    field.wave_spawn_step = field.step + 1;
    std::erase_if(field.foes,
                  [](const ro::sim::SurvivorsFoe& foe) { return foe.max_hp != kSteadyFoeHp; });
    run.Step({});
  }
}

// Un monstre robuste et immobile, à `offset` du héros.
[[nodiscard]] std::uint32_t PlaceSteadyFoe(ro::sim::SurvivorsField& field,
                                           ro::sim::FieldPoint offset) {
  const std::uint32_t serial =
      ro::test_support::PlaceFoe(field, kSteadyFoeId, field.hero.position + offset);
  REQUIRE(serial != 0U);
  ro::sim::SurvivorsFoe& foe = field.foes.back();
  foe.units_per_step = 0;
  foe.hp = kSteadyFoeHp;
  foe.max_hp = kSteadyFoeHp;
  foe.touch_ready_step = std::numeric_limits<std::uint32_t>::max();
  return serial;
}

[[nodiscard]] std::uint32_t HitsOn(const ro::sim::SurvivorsNews& news, std::uint32_t foe_serial,
                                   std::uint16_t skill_id) {
  return static_cast<std::uint32_t>(
      std::ranges::count_if(news.hits, [&](const ro::sim::SurvivorsHit& hit) {
        return hit.foe_serial == foe_serial && hit.skill_id == skill_id;
      }));
}

[[nodiscard]] ro::sim::FieldPoint East(std::int32_t tenths) {
  return ro::sim::FieldPoint{ro::sim::CellTenths(tenths), 0};
}

// Une partie de `class_id` ouverte par `first_skill_id` (0 : l'auto-attaque
// seule), sans monstre.
[[nodiscard]] SurvivorsRun Opened(const ro::sim::SurvivorsRules& rules, std::uint16_t class_id,
                                  std::uint16_t first_skill_id) {
  auto run = SurvivorsRun::Start(rules, ro::sim::SurvivorsStart{.seed = kSeed,
                                                                .class_id = class_id,
                                                                .arena_id = kPlainsArenaId,
                                                                .first_skill_id = first_skill_id});
  REQUIRE(run.has_value());
  run->field_for_tests().foes.clear();
  return std::move(*run);
}

// Une gemme aux pieds du héros, de quoi monter d'un niveau : le pas la ramasse
// et ouvre le choix.
void LevelUp(SurvivorsRun& run) {
  ro::sim::SurvivorsField& field = run.field_for_tests();
  field.gems.push_back(ro::sim::SurvivorsGem{
      .position = field.hero.position,
      .previous_position = field.hero.position,
      .exp = ro::sim::ExpToNext(*field.rules, field.hero.level) - field.hero.exp});
  run.Step({});
}

}  // namespace

TEST_CASE(
    "Départ survivors : chaque classe part de son auto-attaque, et son premier niveau offre "
    "ses skills de départ",
    "[sim][survivors]") {
  const ro::sim::SurvivorsRules& rules = ro::sim::ShippedSurvivorsRules();
  for (const ro::sim::ClassRule& hero_class : rules.classes) {
    INFO("classe " << hero_class.key);
    SurvivorsRun run = Opened(rules, hero_class.class_id, 0);
    ro::sim::SurvivorsField& field = run.field_for_tests();
    REQUIRE(field.hero.skills.size() == 1U);
    CHECK(field.hero.skills.front().skill_id == hero_class.weapon_skill_id);
    LevelUp(run);
    REQUIRE(field.phase == ro::sim::SurvivorsPhase::kChoosing);
    CHECK(field.offers.size() ==
          std::min<std::size_t>(hero_class.starter_skills.size(), rules.caps.offers));
    for (const ro::sim::SurvivorsOffer& offer : field.offers) {
      CHECK(offer.kind == ro::sim::OfferKind::kNewSkill);
      CHECK(std::ranges::find(hero_class.starter_skills, offer.id) !=
            hero_class.starter_skills.end());
    }
    // Le skill pris, le niveau suivant revient aux offres ordinaires.
    REQUIRE(run.Answer(ro::sim::SurvivorsAnswer{.offer_index = 0}));
    REQUIRE(field.hero.skills.size() == 2U);
    LevelUp(run);
    REQUIRE(field.phase == ro::sim::SurvivorsPhase::kChoosing);
    CHECK(std::ranges::any_of(field.offers, [](const ro::sim::SurvivorsOffer& offer) {
      return offer.kind != ro::sim::OfferKind::kNewSkill;
    }));
  }
}

TEST_CASE("Départ survivors : un skill d'une autre classe ouvre la partie et monte",
          "[sim][survivors]") {
  const ro::sim::SurvivorsRules& rules = ro::sim::ShippedSurvivorsRules();
  const ro::sim::ClassRule* thief = ro::sim::FindClass(rules, kThiefClassId);
  REQUIRE(thief != nullptr);
  REQUIRE(std::ranges::find(thief->skill_pool, kFireBoltId) == thief->skill_pool.end());
  SurvivorsRun run = Opened(rules, kThiefClassId, kFireBoltId);
  ro::sim::SurvivorsField& field = run.field_for_tests();
  REQUIRE(field.hero.skills.size() == 2U);
  // Sa réserve bannie, il ne reste à offrir que Fire Bolt, au niveau 2.
  field.banished.assign(thief->skill_pool.begin(), thief->skill_pool.end());
  field.banished.insert(field.banished.end(), thief->card_pool.begin(), thief->card_pool.end());
  LevelUp(run);
  REQUIRE(field.phase == ro::sim::SurvivorsPhase::kChoosing);
  REQUIRE(field.offers.size() == 1U);
  CHECK(field.offers.front() == ro::sim::SurvivorsOffer{.kind = ro::sim::OfferKind::kSkillLevel,
                                                        .id = kFireBoltId,
                                                        .next_level = 2});

  // Un coup d'arme ni un Id inconnu n'ouvrent une partie.
  for (const std::uint16_t refused : {kDaggerStabId, std::uint16_t{9999}}) {
    CHECK_FALSE(SurvivorsRun::Start(rules, ro::sim::SurvivorsStart{.seed = kSeed,
                                                                   .class_id = kThiefClassId,
                                                                   .arena_id = kPlainsArenaId,
                                                                   .first_skill_id = refused})
                    .has_value());
  }
}

TEST_CASE("Départ survivors : chaque classe tient sa partie avec chaque skill d'ouverture",
          "[sim][survivors]") {
  constexpr std::uint32_t kPlayedSteps = 250;
  const ro::sim::SurvivorsRules& rules = ro::sim::ShippedSurvivorsRules();
  const std::vector<std::uint16_t> opening = ro::sim::OpeningSkills(rules);
  REQUIRE_FALSE(opening.empty());
  for (const ro::sim::ClassRule& hero_class : rules.classes) {
    for (const std::uint16_t skill_id : opening) {
      INFO("classe " << hero_class.key << ", skill " << skill_id);
      auto run = SurvivorsRun::Start(rules, ro::sim::SurvivorsStart{.seed = kSeed,
                                                                    .class_id = hero_class.class_id,
                                                                    .arena_id = kPlainsArenaId,
                                                                    .first_skill_id = skill_id});
      REQUIRE(run.has_value());
      for (std::uint32_t step = 0; step < kPlayedSteps; ++step) {
        if (run->field().phase == ro::sim::SurvivorsPhase::kChoosing) {
          REQUIRE(run->Answer(ro::test_support::ScriptedAnswer(run->field())));
        }
        run->Step({});
      }
      CHECK(std::ranges::any_of(
          run->field().hero.skills,
          [skill_id](const ro::sim::OwnedSkill& owned) { return owned.skill_id == skill_id; }));
    }
  }
}

TEST_CASE("Coup de l'arme survivors : chaque classe livrée a le sien, aux armes de son corps",
          "[sim][survivors]") {
  const ro::sim::SurvivorsRules& rules = ro::sim::ShippedSurvivorsRules();
  REQUIRE(ro::sim::CheckSurvivorsRules(rules).has_value());
  for (const ro::sim::ClassRule& hero_class : rules.classes) {
    CAPTURE(hero_class.class_id);
    const ro::sim::SurvivorsSkillRule* weapon =
        ro::sim::FindSkill(rules, hero_class.weapon_skill_id);
    REQUIRE(weapon != nullptr);
    CHECK(ro::sim::IsWeaponSkill(*weapon));
    CHECK(weapon->weapon_item_ids.front() == hero_class.weapon_item_id);
    CHECK(std::ranges::find(hero_class.skill_pool, weapon->skill_id) ==
          hero_class.skill_pool.end());
    // Les quatre cartes communes, dans toutes les réserves.
    for (const std::uint16_t card :
         {kSmokieCardId, kMantisCardId, kOrcSkeletonCardId, kNightmareCardId}) {
      CAPTURE(card);
      CHECK(std::ranges::find(hero_class.card_pool, card) != hero_class.card_pool.end());
    }
  }
  // L'Alchimiste tient un marteau, et son coup de l'arme aussi.
  const ro::sim::ClassRule* merchant = ro::sim::FindClass(rules, kMerchantClassId);
  REQUIRE(merchant != nullptr);
  CHECK(merchant->evolved_weapon_item_id == kHollgrehennHammerItemId);
}

TEST_CASE("Coup de l'arme survivors : posé au premier emplacement, avant le skill de départ",
          "[sim][survivors]") {
  const ro::sim::SurvivorsRules& rules = ro::sim::ShippedSurvivorsRules();
  auto run = SurvivorsRun::Start(rules, ro::sim::SurvivorsStart{.seed = kSeed,
                                                                .class_id = kThiefClassId,
                                                                .arena_id = kPlainsArenaId,
                                                                .first_skill_id = kDoubleAttackId});
  REQUIRE(run.has_value());
  const ro::sim::SurvivorsHero& hero = run->field().hero;
  REQUIRE(hero.skills.size() == 2U);
  CHECK(hero.skills[0].skill_id == kDaggerStabId);
  CHECK(hero.skills[0].level == 1U);
  CHECK(hero.skills[1].skill_id == kDoubleAttackId);
  // Une place reste pour chacun des cinq autres skills.
  CHECK(rules.caps.skill_slots == ro::sim::kMostSkillSlots);
}

TEST_CASE("Coup de l'arme survivors : la carte Mantis raccourcit sa seule recharge",
          "[sim][survivors]") {
  const ro::sim::SurvivorsRules& rules = ro::sim::ShippedSurvivorsRules();
  const auto ready_after_one_blow = [&](std::int32_t cut_pct, std::uint16_t skill_id) {
    SurvivorsRun run = Bare(rules, kThiefClassId, kDoubleAttackId);
    ro::sim::SurvivorsField& field = run.field_for_tests();
    Hold(field, skill_id);
    SetStat(field, SurvivorsStat::kWeaponDelayCutPct, cut_pct);
    (void)PlaceSteadyFoe(field, East(10));
    const std::uint32_t fired_step = field.step;
    StepQuietly(run, 1);
    return field.hero.skills.front().ready_step - fired_step;
  };
  CHECK(ready_after_one_blow(0, kDaggerStabId) == kDaggerCooldownSteps);
  CHECK(ready_after_one_blow(kDelayCutPct, kDaggerStabId) ==
        kDaggerCooldownSteps * (kWholePct - kDelayCutPct) / kWholePct);
  // Sous le plafond des réglages, comme toute recharge.
  const std::int32_t most_cut = rules.tuning.most_cooldown_pct;
  CHECK(ready_after_one_blow(kWholePct, kDaggerStabId) ==
        kDaggerCooldownSteps * static_cast<std::uint32_t>(kWholePct - most_cut) / kWholePct);
  // Un skill qui n'est pas l'arme n'en reçoit rien.
  CHECK(ready_after_one_blow(0, kDoubleAttackId) ==
        ready_after_one_blow(kDelayCutPct, kDoubleAttackId));
}

TEST_CASE("Coup de l'arme survivors : la carte Orc Skeleton le fait transpercer",
          "[sim][survivors]") {
  const ro::sim::SurvivorsRules& rules = ro::sim::ShippedSurvivorsRules();
  SECTION("une flèche traverse le premier monstre et touche le second") {
    const auto far_hits = [&](std::int32_t pierce) {
      SurvivorsRun run = Bare(rules, kArcherClassId, kDoubleStrafeId);
      ro::sim::SurvivorsField& field = run.field_for_tests();
      Hold(field, kBowShotId);
      SetStat(field, SurvivorsStat::kWeaponPierce, pierce);
      SetStat(field, SurvivorsStat::kWeaponDelayCutPct, 0);
      const std::uint32_t near = PlaceSteadyFoe(field, East(20));
      const std::uint32_t far = PlaceSteadyFoe(field, East(40));
      (void)run.TakeNews();
      // Une seule flèche : la recharge ne revient pas pendant son vol.
      field.hero.skills.front().ready_step = field.step;
      StepQuietly(run, 1);
      field.hero.skills.front().ready_step = std::numeric_limits<std::uint32_t>::max();
      StepQuietly(run, kFlightSteps);
      const ro::sim::SurvivorsNews news = run.TakeNews();
      CHECK(HitsOn(news, near, kBowShotId) == 1U);
      return HitsOn(news, far, kBowShotId);
    };
    CHECK(far_hits(0) == 0U);
    CHECK(far_hits(1) == 1U);
  }
  SECTION("une frappe enchaîne sur le monstre qui jouxte sa cible") {
    const auto neighbour_hits = [&](std::int32_t pierce) {
      SurvivorsRun run = Bare(rules, kThiefClassId, kDoubleAttackId);
      ro::sim::SurvivorsField& field = run.field_for_tests();
      Hold(field, kDaggerStabId);
      SetStat(field, SurvivorsStat::kWeaponPierce, pierce);
      const std::uint32_t target = PlaceSteadyFoe(field, East(12));
      // Hors de portée de la dague, à moins d'une case de sa cible.
      const std::uint32_t neighbour = PlaceSteadyFoe(field, East(20));
      (void)run.TakeNews();
      StepQuietly(run, kFewSteps);
      const ro::sim::SurvivorsNews news = run.TakeNews();
      CHECK(HitsOn(news, target, kDaggerStabId) == 1U);
      return HitsOn(news, neighbour, kDaggerStabId);
    };
    CHECK(neighbour_hits(0) == 0U);
    CHECK(neighbour_hits(1) == 1U);
  }
}

TEST_CASE("Coup de l'arme survivors : la carte Nightmare le fait rebondir, lui seul",
          "[sim][survivors]") {
  const ro::sim::SurvivorsRules& rules = ro::sim::ShippedSurvivorsRules();
  const auto split_hits = [&](std::uint16_t skill_id, std::int32_t chance_pct) {
    SurvivorsRun run = Bare(rules, kThiefClassId, kDoubleAttackId);
    ro::sim::SurvivorsField& field = run.field_for_tests();
    Hold(field, skill_id);
    SetStat(field, SurvivorsStat::kWeaponSplitChancePct, chance_pct);
    const std::uint32_t target = PlaceSteadyFoe(field, East(10));
    // Hors de portée de la dague, à deux cases de la cible : dans le rebond.
    const std::uint32_t nearby = PlaceSteadyFoe(field, East(30));
    (void)run.TakeNews();
    StepQuietly(run, kFewSteps);
    const ro::sim::SurvivorsNews news = run.TakeNews();
    CHECK(HitsOn(news, target, skill_id) > 0U);
    return HitsOn(news, nearby, skill_id);
  };
  CHECK(split_hits(kDaggerStabId, 0) == 0U);
  CHECK(split_hits(kDaggerStabId, kWholePct) == 1U);
  // Double Attack n'est pas l'arme : il ne rebondit pas.
  CHECK(split_hits(kDoubleAttackId, kWholePct) == 0U);
}

TEST_CASE("Coup de l'arme survivors : deux niveaux, l'arme du départ puis celle de l'évolution",
          "[sim][survivors]") {
  const ro::sim::SurvivorsRules& rules = ro::sim::ShippedSurvivorsRules();
  const ro::sim::SurvivorsSkillRule* dagger = ro::sim::FindSkill(rules, kDaggerStabId);
  REQUIRE(dagger != nullptr);
  // Deux niveaux : l'arme du départ, puis celle de l'évolution.
  CHECK(dagger->levels.size() == 2U);
  CHECK(dagger->weapon_item_ids.size() == 2U);
  CHECK(dagger->weapon_item_ids[1] == kJurItemId);
}

TEST_CASE("Coup de l'arme survivors : la baguette ne retire pas le trophée Pyromane",
          "[sim][survivors]") {
  const ro::sim::SurvivorsRules& rules = ro::sim::ShippedSurvivorsRules();
  auto run = SurvivorsRun::Start(rules, ro::sim::SurvivorsStart{.seed = kSeed,
                                                                .class_id = kMageClassId,
                                                                .arena_id = kPlainsArenaId,
                                                                .first_skill_id = kFireBoltId});
  REQUIRE(run.has_value());
  ro::sim::SurvivorsField& field = run->field_for_tests();
  // Le coup de baguette, neutre, puis Fire Bolt : seul Fire Bolt compte.
  REQUIRE(field.hero.skills.size() == 2U);
  static constexpr std::array<ro::sim::BossRule, 1> kBaphometFight{
      {{.foe_id = kBaphometId, .final = true}}};
  field.tally.outcome = ro::sim::SurvivorsOutcome::kVictory;
  field.content.bosses = kBaphometFight;
  ro::sim::MinigameRecord record;
  const std::vector<ro::sim::MinigameTrophy> earned = ro::sim::RecordSurvivors(record, *run);
  CHECK(std::ranges::find(earned, ro::sim::MinigameTrophy::kSurvivorsPyromaniac) != earned.end());
}
