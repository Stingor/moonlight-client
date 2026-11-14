#include "ro/sim/survivors_rules.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/core/error.h"
#include "ro/sim/survivors_geometry.h"

#include "support/survivors_fixture.h"

using ro::sim::ArenaRule;
using ro::sim::BossRule;
using ro::sim::CardRule;
using ro::sim::CheckSurvivorsRules;
using ro::sim::ClassRule;
using ro::sim::ElementPct;
using ro::sim::ExpSegment;
using ro::sim::ExpToNext;
using ro::sim::FindArena;
using ro::sim::FindCard;
using ro::sim::FindClass;
using ro::sim::FindFoe;
using ro::sim::FindSkill;
using ro::sim::FoeRule;
using ro::sim::SkillLevel;
using ro::sim::SurvivorsElement;
using ro::sim::SurvivorsRules;
using ro::sim::SurvivorsSkillRule;
using ro::sim::WaveRule;
using ro::test_support::kTinyCardId;
using ro::test_support::kTinyCards;
using ro::test_support::kTinyClasses;
using ro::test_support::kTinyFoes;
using ro::test_support::kTinyHomingId;
using ro::test_support::kTinyLunaticId;
using ro::test_support::kTinyOrbitId;
using ro::test_support::kTinyPoringId;
using ro::test_support::kTinySkills;
using ro::test_support::kTinyStrikeId;
using ro::test_support::kTinyWaves;
using ro::test_support::TinyRules;

namespace {

// Refusé par le contrôle, comme une faute de l'appelant, avec un détail.
[[nodiscard]] bool Refused(const SurvivorsRules& rules) {
  const ro::core::Result<void> checked = CheckSurvivorsRules(rules);
  return !checked && checked.error().code == ro::core::ErrorCode::kInvalidArgument &&
         !checked.error().detail.empty();
}

[[nodiscard]] bool Accepted(const SurvivorsRules& rules) {
  return CheckSurvivorsRules(rules).has_value();
}

constexpr std::size_t kStrikeIndex = 0;
constexpr std::size_t kHomingIndex = 1;
constexpr std::size_t kOrbitIndex = 2;
constexpr std::uint16_t kUnknownId = 9999;

}  // namespace

TEST_CASE("Règles survivors : recherches, XP et éléments", "[sim][survivors]") {
  const SurvivorsRules& rules = TinyRules();
  REQUIRE(FindSkill(rules, kTinyOrbitId) != nullptr);
  CHECK(FindSkill(rules, kTinyOrbitId)->archetype == ro::sim::SkillArchetype::kOrbit);
  // Les skills se lisent dans l'ordre de la table, sans en sortir.
  REQUIRE(rules.skills.size() == 3U);
  CHECK(rules.skills[1].skill_id == kTinyHomingId);
  CHECK(FindSkill(rules, kUnknownId) == nullptr);
  CHECK(FindCard(rules, kTinyCardId) != nullptr);
  CHECK(FindCard(rules, 1) == nullptr);
  CHECK(FindFoe(rules, 2) == nullptr);
  CHECK(FindClass(rules, 2) == nullptr);
  CHECK(FindFoe(rules, kTinyPoringId)->hp == 8);
  CHECK(FindClass(rules, ro::test_support::kTinyClassId)->max_hp == 100);
  CHECK(FindArena(rules, ro::test_support::kTinyArenaId) != nullptr);
  CHECK(FindArena(rules, 2) == nullptr);

  // La courbe livrée s'éprouve sur la table livrée, plus bas.
  // Une courbe nulle ne rend jamais 0 : une boucle de montées doit finir.
  constexpr std::array<ExpSegment, 1> kFlatCurve{{{1, 0, 0}}};
  SurvivorsRules flat = rules;
  flat.exp_curve = kFlatCurve;
  CHECK(ExpToNext(flat, 1) == 1U);
  CHECK(ExpToNext(flat, 30) == 1U);
  CHECK(ExpToNext(flat, 0) == 0xFFFFFFFFU);
  SurvivorsRules curveless = rules;
  curveless.exp_curve = {};
  CHECK(ExpToNext(curveless, 1) == 0xFFFFFFFFU);

  CHECK(ElementPct(rules, SurvivorsElement::kWind, SurvivorsElement::kWater) == 175);
  CHECK(ElementPct(rules, SurvivorsElement::kFire, SurvivorsElement::kWater) == 50);
  CHECK(ElementPct(rules, SurvivorsElement::kPoison, SurvivorsElement::kUndead) == 0);
  // Sans table complète, une attaque ne gagne ni ne perd rien.
  SurvivorsRules elementless = rules;
  elementless.elements = {};
  CHECK(ElementPct(elementless, SurvivorsElement::kWind, SurvivorsElement::kWater) == 100);
}

TEST_CASE("Règles survivors : la table livrée porte le ruleset 2", "[sim][survivors]") {
  const SurvivorsRules& shipped = ro::sim::ShippedSurvivorsRules();
  CHECK(&shipped == &ro::sim::ShippedSurvivorsRules());
  CHECK(shipped.ruleset_version == ro::sim::kSurvivorsRulesetVersion);
  CHECK(shipped.run_ms == 900000U);
  CHECK(shipped.caps.most_foes == 400);
  CHECK(shipped.caps.offers == 3);
  CHECK(shipped.tuning.foe_walk_scale_pct == 180);
  CHECK(shipped.tuning.hero_radius_units == ro::sim::CellTenths(4));
  CHECK(shipped.spawn_frame.half_width_units == ro::sim::Cells(30));

  const ro::core::Result<void> checked = CheckSurvivorsRules(shipped);
  INFO((checked ? std::string_view{} : std::string_view{checked.error().detail}));
  CHECK(checked.has_value());

  // Le détail du catalogue est épinglé dans survivors_catalogue_test.cpp.
  REQUIRE(shipped.classes.size() == 5U);
  REQUIRE(shipped.skills.size() == 101U);
  REQUIRE(shipped.cards.size() == 30U);
  REQUIRE(shipped.foes.size() == 113U);
  REQUIRE(shipped.waves.size() == 18U);
  REQUIRE(shipped.bosses.size() == 5U);
  REQUIRE(shipped.arenas.size() == 4U);

  // Chaque description est un texte à traduire, jamais vide.
  for (const SurvivorsSkillRule& skill : shipped.skills) {
    CHECK(!std::string_view{skill.description}.empty());
    CHECK(!std::string_view{skill.key}.empty());
  }
  for (const CardRule& card : shipped.cards) {
    CHECK(!std::string_view{card.description}.empty());
    CHECK(!std::string_view{card.key}.empty());
  }

  // Les monstres marchent au WalkSpeed de RO, ralenti du réglage : 180 % de
  // ms par case rendent un Lunatic plus LENT.
  const FoeRule* lunatic = FindFoe(shipped, 1063);
  REQUIRE(lunatic != nullptr);
  const auto walk_scale_pct = static_cast<std::uint32_t>(shipped.tuning.foe_walk_scale_pct);
  CHECK(ro::sim::UnitsPerStep(lunatic->ro_walk_ms_per_cell * walk_scale_pct / 100U) == 56);
  const FoeRule* mastering = FindFoe(shipped, 1090);
  REQUIRE(mastering != nullptr);
  CHECK(mastering->rank == ro::sim::FoeRank::kMiniBoss);
  CHECK(shipped.bosses.front().foe_id == 1090);
  CHECK(shipped.bosses.front().at_ms == 180000U);
  CHECK(shipped.waves.back().from_ms == 890000U);
  // Aucune arène livrée ne lit de carte : toutes sont des biomes générés.
  CHECK(FindArena(shipped, 1) == nullptr);
  // L'arène générée nomme une règle de terrain du catalogue.
  REQUIRE(FindArena(shipped, 2) != nullptr);
  CHECK(ro::sim::FindTerrain(shipped, FindArena(shipped, 2)->terrain_id) != nullptr);

  // La courbe livrée, pente comprise : les niveaux 19 et 39 et les cumuls
  // dépendent de per_level.
  CHECK(ExpToNext(shipped, 1) == 5U);
  CHECK(ExpToNext(shipped, 19) == 185U);
  CHECK(ExpToNext(shipped, 20) == 220U);
  CHECK(ExpToNext(shipped, 39) == 980U);
  CHECK(ExpToNext(shipped, 40) == 3500U);
  std::uint32_t shipped_total = 0;
  for (std::uint16_t level = 1; level < 50; ++level) {
    shipped_total += ExpToNext(shipped, level);
    if (level == 19) CHECK(shipped_total == 1805U);
    if (level == 39) CHECK(shipped_total == 13805U);
  }
  CHECK(shipped_total == 57805U);

  // La table livrée et celle des essais recopient la même sous-table du
  // serveur.
  REQUIRE(shipped.elements.size() == ro::sim::kSurvivorsElementCount);
  for (std::size_t row = 0; row < ro::sim::kSurvivorsElementCount; ++row) {
    CHECK(shipped.elements[row] == ro::test_support::kTinyElements[row]);
  }
  CHECK(ElementPct(shipped, SurvivorsElement::kWind, SurvivorsElement::kWater) == 175);
  CHECK(ElementPct(shipped, SurvivorsElement::kFire, SurvivorsElement::kWater) == 50);
  CHECK(ElementPct(shipped, SurvivorsElement::kPoison, SurvivorsElement::kUndead) == 0);
  CHECK(ElementPct(shipped, SurvivorsElement::kNeutral, SurvivorsElement::kGhost) == 25);
}

namespace {

// ── Les skills et les cartes du lot 1, recopiés de la spécification ─────────
// Une faute de frappe dans la table livrée se voit ici, avant que le rejeu en
// or ne la fige. Le reste du catalogue : survivors_catalogue_test.cpp.

// Les niveaux d'un skill, en colonnes : une valeur par niveau, du 1 au 8.
template <typename Value>
using PerLevel = std::array<Value, ro::sim::kMostSkillLevels>;

struct PinnedBolts {
  PerLevel<std::uint8_t> amount;
  PerLevel<std::int32_t> damage;
  PerLevel<std::uint32_t> cooldown_ms;
  PerLevel<std::uint32_t> chill_ms;
};
constexpr PinnedBolts kPinnedFireBolt{.amount = {1, 1, 1, 1, 1, 1, 1, 1},
                                      .damage = {29, 43, 50, 67, 67, 84, 120, 168},
                                      .cooldown_ms = {1100, 1100, 1100, 1100, 950, 950, 950, 950},
                                      .chill_ms = {}};
constexpr PinnedBolts kPinnedColdBolt{
    .amount = {1, 1, 1, 1, 1, 1, 1, 1},
    .damage = {26, 40, 43, 43, 58, 58, 72, 96},
    .cooldown_ms = {1300, 1300, 1300, 1300, 1300, 1100, 1100, 1100},
    .chill_ms = {1500, 1500, 1500, 2500, 2500, 2500, 2500, 3000}};
constexpr PinnedBolts kPinnedLightningBolt{
    .amount = {1, 1, 1, 1, 1, 1, 1, 1},
    .damage = {38, 58, 68, 91, 91, 115, 144, 202},
    .cooldown_ms = {1500, 1500, 1500, 1500, 1250, 1250, 1250, 1250},
    .chill_ms = {}};

struct PinnedSoul {
  PerLevel<std::uint8_t> amount;
  PerLevel<std::int32_t> damage;
  PerLevel<std::uint32_t> cooldown_ms;
  PerLevel<std::uint8_t> pierce;
};
constexpr PinnedSoul kPinnedSoulStrike{.amount = {2, 3, 3, 3, 4, 4, 6, 6},
                                       .damage = {8, 8, 11, 11, 11, 11, 11, 16},
                                       .cooldown_ms = {1000, 1000, 1000, 1000, 1000, 850, 850, 850},
                                       .pierce = {0, 0, 0, 1, 1, 1, 1, 2}};

// Le rayon d'orbite en dixièmes de case ; life_ms allumée, rest_ms éteinte.
struct PinnedOrbit {
  PerLevel<std::uint8_t> amount;
  PerLevel<std::int32_t> damage;
  PerLevel<std::int32_t> orbit_tenths;
  PerLevel<std::uint32_t> life_ms;
  PerLevel<std::uint32_t> rest_ms;
};
constexpr PinnedOrbit kPinnedSight{.amount = {1, 2, 2, 2, 3, 3, 4, 4},
                                   .damage = {10, 10, 14, 14, 14, 20, 20, 20},
                                   .orbit_tenths = {20, 20, 20, 25, 25, 25, 25, 25},
                                   .life_ms = {3000, 3000, 3000, 3000, 4000, 4000, 4000, 4000},
                                   .rest_ms = {3000, 3000, 3000, 3000, 3000, 3000, 3000, 0}};

void CheckBolt(const SurvivorsRules& rules, std::uint16_t skill_id, std::string_view key,
               ro::sim::TargetRule target, SurvivorsElement element, const PinnedBolts& pinned) {
  INFO("skill " << skill_id);
  const SurvivorsSkillRule* skill = FindSkill(rules, skill_id);
  REQUIRE(skill != nullptr);
  CHECK(std::string_view{skill->key} == key);
  CHECK(skill->archetype == ro::sim::SkillArchetype::kTargetStrike);
  CHECK(skill->target == target);
  CHECK(skill->element == element);
  REQUIRE(skill->levels.size() == pinned.amount.size());
  for (std::size_t index = 0; index < pinned.amount.size(); ++index) {
    INFO("niveau " << index + 1);
    const SkillLevel& level = skill->levels[index];
    CHECK(level.amount == pinned.amount[index]);
    CHECK(level.damage == pinned.damage[index]);
    CHECK(level.cooldown_ms == pinned.cooldown_ms[index]);
    CHECK(level.chill_ms == pinned.chill_ms[index]);
    CHECK(level.interval_ms == 120U);
    CHECK(level.reach_units == ro::sim::Cells(9));
  }
}

void CheckCard(const SurvivorsRules& rules, std::uint16_t card_item_id, std::string_view key,
               ro::sim::SurvivorsStat stat, std::span<const std::int32_t> per_level) {
  INFO("carte " << card_item_id);
  const CardRule* card = FindCard(rules, card_item_id);
  REQUIRE(card != nullptr);
  CHECK(std::string_view{card->key} == key);
  CHECK(card->stat == stat);
  CHECK(std::ranges::equal(card->per_level, per_level));
}

}  // namespace

TEST_CASE("Règles survivors : le catalogue livré, valeur par valeur", "[sim][survivors]") {
  const SurvivorsRules& shipped = ro::sim::ShippedSurvivorsRules();

  CHECK(shipped.spawn_frame.north_units == ro::sim::Cells(20));
  CHECK(shipped.spawn_frame.south_units == ro::sim::Cells(17));
  CHECK(shipped.spawn_frame.margin_units == ro::sim::Cells(3));
  CHECK(shipped.caps.most_shots == 256);
  CHECK(shipped.caps.most_strikes == 512);
  CHECK(shipped.caps.most_gems == 400);
  CHECK(shipped.caps.skill_slots == 7);
  CHECK(shipped.caps.card_slots == 6);

  const ClassRule* mage = FindClass(shipped, 1);
  REQUIRE(mage != nullptr);
  CHECK(std::string_view{mage->key} == "mage");
  CHECK(mage->job_id == 9);
  CHECK(mage->weapon_item_id == 1601);
  CHECK(mage->max_hp == 340);
  CHECK(mage->walk_ms_per_cell == 150U);
  CHECK(mage->magnet_units == ro::sim::Cells(3));
  CHECK(mage->damage_pct_per_level == 1);
  CHECK(mage->damage_pct_cap == 25);
  constexpr std::array<std::uint16_t, 3> kStarters{19, 14, 20};
  constexpr std::array<std::uint16_t, 13> kSkillPool{19, 14, 20, 13, 10, 11, 18,
                                                     15, 90, 21, 17, 86, 87};
  constexpr std::array<std::uint16_t, 16> kCardPool{4019, 4003, 4009, 4001, 4022, 4016, 4072, 4197,
                                                    4266, 4179, 4183, 4147, 4044, 4079, 4085, 4127};
  CHECK(std::ranges::equal(mage->starter_skills, kStarters));
  CHECK(std::ranges::equal(mage->skill_pool, kSkillPool));
  CHECK(std::ranges::equal(mage->card_pool, kCardPool));

  CheckBolt(shipped, 19, "fire_bolt", ro::sim::TargetRule::kNearest, SurvivorsElement::kFire,
            kPinnedFireBolt);
  CheckBolt(shipped, 14, "cold_bolt", ro::sim::TargetRule::kRandom, SurvivorsElement::kWater,
            kPinnedColdBolt);
  CheckBolt(shipped, 20, "lightning_bolt", ro::sim::TargetRule::kStrongest, SurvivorsElement::kWind,
            kPinnedLightningBolt);

  const SurvivorsSkillRule* soul_strike = FindSkill(shipped, 13);
  REQUIRE(soul_strike != nullptr);
  CHECK(std::string_view{soul_strike->key} == "soul_strike");
  CHECK(soul_strike->archetype == ro::sim::SkillArchetype::kHoming);
  CHECK(soul_strike->target == ro::sim::TargetRule::kNearest);
  CHECK(soul_strike->element == SurvivorsElement::kGhost);
  REQUIRE(soul_strike->levels.size() == kPinnedSoulStrike.amount.size());
  for (std::size_t index = 0; index < kPinnedSoulStrike.amount.size(); ++index) {
    INFO("Soul Strike niveau " << index + 1);
    const SkillLevel& level = soul_strike->levels[index];
    CHECK(level.amount == kPinnedSoulStrike.amount[index]);
    CHECK(level.damage == kPinnedSoulStrike.damage[index]);
    CHECK(level.cooldown_ms == kPinnedSoulStrike.cooldown_ms[index]);
    CHECK(level.pierce == kPinnedSoulStrike.pierce[index]);
    CHECK(level.reach_units == ro::sim::Cells(10));
    CHECK(level.speed_units_per_s == ro::sim::Cells(9));
    CHECK(level.hit_radius_units == ro::sim::CellTenths(4));
    CHECK(level.life_ms == 2000U);
  }

  const SurvivorsSkillRule* sight = FindSkill(shipped, 10);
  REQUIRE(sight != nullptr);
  CHECK(std::string_view{sight->key} == "sight");
  CHECK(sight->archetype == ro::sim::SkillArchetype::kOrbit);
  CHECK(sight->element == SurvivorsElement::kFire);
  REQUIRE(sight->levels.size() == kPinnedSight.amount.size());
  for (std::size_t index = 0; index < kPinnedSight.amount.size(); ++index) {
    INFO("Sight niveau " << index + 1);
    const SkillLevel& level = sight->levels[index];
    CHECK(level.amount == kPinnedSight.amount[index]);
    CHECK(level.damage == kPinnedSight.damage[index]);
    CHECK(level.reach_units == ro::sim::CellTenths(kPinnedSight.orbit_tenths[index]));
    CHECK(level.life_ms == kPinnedSight.life_ms[index]);
    CHECK(level.rest_ms == kPinnedSight.rest_ms[index]);
    CHECK(level.period_ms == 1600U);
    CHECK(level.hit_radius_units == ro::sim::CellTenths(6));
    CHECK(level.rehit_ms == 500U);
  }

  constexpr std::array<std::int32_t, 5> kHornet{6, 12, 18, 24, 30};
  constexpr std::array<std::int32_t, 5> kPupa{20, 40, 60, 80, 100};
  constexpr std::array<std::int32_t, 5> kChonchon{8, 16, 24, 32, 40};
  constexpr std::array<std::int32_t, 5> kPoring{10, 20, 30, 40, 50};
  CheckCard(shipped, 4019, "hornet_card", ro::sim::SurvivorsStat::kDamagePct, kHornet);
  CheckCard(shipped, 4003, "pupa_card", ro::sim::SurvivorsStat::kMaxHp, kPupa);
  CheckCard(shipped, 4009, "chonchon_card", ro::sim::SurvivorsStat::kWalkPct, kChonchon);
  CheckCard(shipped, 4001, "poring_card", ro::sim::SurvivorsStat::kLuckPct, kPoring);

  for (const ArenaRule& arena : shipped.arenas) CHECK_FALSE(arena.start.has_value());
}

TEST_CASE("Règles survivors : les règles des essais passent le contrôle", "[sim][survivors]") {
  CHECK(Accepted(TinyRules()));
  // Une vague à plancher et lot nuls ne fait naître personne, et reste valide.
  CHECK(Accepted(ro::test_support::QuietRules()));
}

TEST_CASE("Règles survivors : un catalogue aux Id incohérents est refusé", "[sim][survivors]") {
  SurvivorsRules rules = TinyRules();
  rules.ruleset_version = 0;
  CHECK(Refused(rules));
  rules = TinyRules();
  rules.run_ms = 0;
  CHECK(Refused(rules));

  // Un Id en double, table par table ; le reste du catalogue reste juste.
  const std::array<ClassRule, 2> twin_classes{kTinyClasses[0], kTinyClasses[0]};
  rules = TinyRules();
  rules.classes = twin_classes;
  CHECK(Refused(rules));
  const std::array<SurvivorsSkillRule, 4> twin_skills{kTinySkills[0], kTinySkills[1],
                                                      kTinySkills[2], kTinySkills[0]};
  rules = TinyRules();
  rules.skills = twin_skills;
  CHECK(Refused(rules));
  const std::array<CardRule, 2> twin_cards{kTinyCards[0], kTinyCards[0]};
  rules = TinyRules();
  rules.cards = twin_cards;
  CHECK(Refused(rules));
  const std::array<FoeRule, 3> twin_foes{kTinyFoes[0], kTinyFoes[1], kTinyFoes[0]};
  rules = TinyRules();
  rules.foes = twin_foes;
  CHECK(Refused(rules));
  const std::array<ArenaRule, 2> twin_arenas{ro::test_support::kTinyArenas[0],
                                             ro::test_support::kTinyArenas[0]};
  rules = TinyRules();
  rules.arenas = twin_arenas;
  CHECK(Refused(rules));
  // Sans arène classée, la partie classée et le défi n'ont où se jouer.
  std::array<ArenaRule, 1> free_only{ro::test_support::kTinyArenas[0]};
  free_only[0].ranked = false;
  rules = TinyRules();
  rules.arenas = free_only;
  CHECK(Refused(rules));

  // Les réserves et les skills de départ d'une classe.
  const std::array<std::uint16_t, 1> strike_only{kTinyStrikeId};
  const std::array<std::uint16_t, 4> pool_with_unknown{kTinyStrikeId, kTinyHomingId, kTinyOrbitId,
                                                       kUnknownId};
  std::array<ClassRule, 1> hero_class{kTinyClasses[0]};
  hero_class[0].skill_pool = pool_with_unknown;
  hero_class[0].starter_skills = strike_only;
  rules = TinyRules();
  rules.classes = hero_class;
  CHECK(Refused(rules));

  const std::array<std::uint16_t, 2> cards_with_unknown{kTinyCardId, kUnknownId};
  hero_class = {kTinyClasses[0]};
  hero_class[0].card_pool = cards_with_unknown;
  rules.classes = hero_class;
  CHECK(Refused(rules));

  const std::array<std::uint16_t, 1> unknown_starter{kUnknownId};
  hero_class = {kTinyClasses[0]};
  hero_class[0].starter_skills = unknown_starter;
  rules.classes = hero_class;
  CHECK(Refused(rules));

  // Un skill connu, mais hors de la réserve de la classe.
  const std::array<std::uint16_t, 2> strike_and_homing{kTinyStrikeId, kTinyHomingId};
  const std::array<std::uint16_t, 1> orbit_only{kTinyOrbitId};
  hero_class = {kTinyClasses[0]};
  hero_class[0].skill_pool = strike_and_homing;
  hero_class[0].starter_skills = orbit_only;
  rules.classes = hero_class;
  CHECK(Refused(rules));
  hero_class[0].starter_skills = strike_only;
  CHECK(Accepted(rules));

  // Un Id deux fois dans une même liste d'une classe : un tirage sans remise
  // pourrait l'offrir deux fois.
  const std::array<std::uint16_t, 4> pool_with_twin{kTinyStrikeId, kTinyHomingId, kTinyOrbitId,
                                                    kTinyStrikeId};
  hero_class = {kTinyClasses[0]};
  hero_class[0].skill_pool = pool_with_twin;
  hero_class[0].starter_skills = strike_only;
  rules = TinyRules();
  rules.classes = hero_class;
  CHECK(Refused(rules));
  const std::array<std::uint16_t, 2> twin_card_pool{kTinyCardId, kTinyCardId};
  hero_class = {kTinyClasses[0]};
  hero_class[0].card_pool = twin_card_pool;
  rules.classes = hero_class;
  CHECK(Refused(rules));
  const std::array<std::uint16_t, 2> twin_starters{kTinyStrikeId, kTinyStrikeId};
  hero_class = {kTinyClasses[0]};
  hero_class[0].starter_skills = twin_starters;
  rules.classes = hero_class;
  CHECK(Refused(rules));
  hero_class[0].starter_skills = strike_and_homing;
  CHECK(Accepted(rules));

  // Un Id à la fois skill et carte de la réserve : un bannissement serait
  // ambigu.
  std::array<CardRule, 2> cards_named_like_skill{kTinyCards[0], kTinyCards[0]};
  cards_named_like_skill[1].card_item_id = kTinyHomingId;
  const std::array<std::uint16_t, 2> card_pool_like_skill{kTinyCardId, kTinyHomingId};
  hero_class = {kTinyClasses[0]};
  hero_class[0].card_pool = card_pool_like_skill;
  rules = TinyRules();
  rules.cards = cards_named_like_skill;
  rules.classes = hero_class;
  CHECK(Refused(rules));

  // Un monstre de vague ou un boss inconnus.
  const std::array<std::uint16_t, 2> wave_with_unknown{kTinyLunaticId, kUnknownId};
  std::array<WaveRule, 1> waves{kTinyWaves[0]};
  waves[0].foe_ids = wave_with_unknown;
  rules = TinyRules();
  rules.waves = waves;
  CHECK(Refused(rules));
  const std::array<BossRule, 1> unknown_boss{{{.at_ms = 1000, .foe_id = kUnknownId}}};
  rules = TinyRules();
  rules.bosses = unknown_boss;
  CHECK(Refused(rules));
}

TEST_CASE("Règles survivors : un skill ou une carte sans niveau valide est refusé",
          "[sim][survivors]") {
  SurvivorsRules rules = TinyRules();
  std::array<SurvivorsSkillRule, 3> skills = kTinySkills;

  skills[kStrikeIndex].levels = {};
  rules.skills = skills;
  CHECK(Refused(rules));

  std::array<SkillLevel, ro::sim::kMostSkillLevels + 1> too_many_levels{};
  too_many_levels.fill(ro::test_support::kTinyStrikeLevels[0]);
  skills = kTinySkills;
  skills[kStrikeIndex].levels = too_many_levels;
  CHECK(Refused(rules));
  skills[kStrikeIndex].levels = std::span(too_many_levels).first(ro::sim::kMostSkillLevels);
  CHECK(Accepted(rules));

  // Un niveau sans quantité ne tirerait rien.
  std::array<SkillLevel, 2> strike_levels = ro::test_support::kTinyStrikeLevels;
  strike_levels[1].amount = 0;
  skills = kTinySkills;
  skills[kStrikeIndex].levels = strike_levels;
  CHECK(Refused(rules));

  // Une orbite sans période ou sans phase active.
  std::array<SkillLevel, 2> orbit_levels = ro::test_support::kTinyOrbitLevels;
  orbit_levels[1].period_ms = 0;
  skills = kTinySkills;
  skills[kOrbitIndex].levels = orbit_levels;
  CHECK(Refused(rules));
  orbit_levels = ro::test_support::kTinyOrbitLevels;
  orbit_levels[0].life_ms = 0;
  CHECK(Refused(rules));
  // Une orbite permanente (sans phase éteinte) reste valide.
  orbit_levels = ro::test_support::kTinyOrbitLevels;
  orbit_levels[0].rest_ms = 0;
  CHECK(Accepted(rules));

  // Un tir sans vitesse ou sans durée de vie.
  std::array<SkillLevel, 2> homing_levels = ro::test_support::kTinyHomingLevels;
  homing_levels[0].speed_units_per_s = 0;
  skills = kTinySkills;
  skills[kHomingIndex].levels = homing_levels;
  CHECK(Refused(rules));
  // Une vitesse négative ferait reculer le projectile.
  homing_levels[0].speed_units_per_s = -1;
  CHECK(Refused(rules));
  // Une sous-unité par pas, le moins qui avance.
  homing_levels[0].speed_units_per_s = static_cast<std::int32_t>(ro::sim::kStepsPerSecond);
  CHECK(Accepted(rules));
  homing_levels = ro::test_support::kTinyHomingLevels;
  homing_levels[1].life_ms = 0;
  CHECK(Refused(rules));

  // Ces contraintes ne valent que pour leur archétype : une frappe n'a ni
  // période ni vitesse.
  skills = kTinySkills;
  CHECK(Accepted(rules));

  std::array<CardRule, 1> cards = kTinyCards;
  rules.cards = cards;
  cards[0].per_level = {};
  CHECK(Refused(rules));
  const std::array<std::int32_t, ro::sim::kMostCardLevels + 1> too_many_card_levels{1, 2, 3,
                                                                                    4, 5, 6};
  cards[0].per_level = too_many_card_levels;
  CHECK(Refused(rules));
  cards[0].per_level = std::span(too_many_card_levels).first(ro::sim::kMostCardLevels);
  CHECK(Accepted(rules));
}

TEST_CASE("Règles survivors : vagues, boss, courbe et éléments incohérents sont refusés",
          "[sim][survivors]") {
  SurvivorsRules rules = TinyRules();

  // Des poids qui ne vont pas avec leurs monstres, nuls, ou trop lourds pour
  // un tirage en 32 bits.
  std::array<WaveRule, 1> waves{kTinyWaves[0]};
  rules.waves = waves;
  const std::array<std::uint32_t, 1> one_weight{1};
  waves[0].weights = one_weight;
  CHECK(Refused(rules));
  const std::array<std::uint32_t, 2> no_weight{0, 0};
  waves[0].weights = no_weight;
  CHECK(Refused(rules));
  const std::array<std::uint32_t, 2> heavy_weights{0xFFFFFFFFU, 1};
  waves[0].weights = heavy_weights;
  CHECK(Refused(rules));
  const std::array<std::uint32_t, 2> full_weights{0xFFFFFFFEU, 1};
  waves[0].weights = full_weights;
  CHECK(Accepted(rules));

  // La première vague à 0, les suivantes triées.
  rules.waves = {};
  CHECK(Refused(rules));
  waves = {kTinyWaves[0]};
  waves[0].from_ms = 1000;
  rules.waves = waves;
  CHECK(Refused(rules));
  std::array<WaveRule, 3> three_waves{kTinyWaves[0], kTinyWaves[0], kTinyWaves[0]};
  three_waves[1].from_ms = 2000;
  three_waves[2].from_ms = 1000;
  rules.waves = three_waves;
  CHECK(Refused(rules));
  three_waves[2].from_ms = 3000;
  CHECK(Accepted(rules));
  // Triées veut dire non décroissantes : deux vagues au même instant passent.
  three_waves[2].from_ms = 2000;
  CHECK(Accepted(rules));

  rules = TinyRules();
  std::array<BossRule, 2> bosses{
      {{.at_ms = 2000, .foe_id = kTinyLunaticId}, {.at_ms = 1000, .foe_id = kTinyPoringId}}};
  rules.bosses = bosses;
  CHECK(Refused(rules));
  bosses[1].at_ms = 3000;
  CHECK(Accepted(rules));
  bosses[1].at_ms = 2000;
  CHECK(Accepted(rules));

  rules = TinyRules();
  rules.exp_curve = {};
  CHECK(Refused(rules));
  const std::array<ExpSegment, 1> late_curve{{{.from_level = 2, .base = 5, .per_level = 10}}};
  rules.exp_curve = late_curve;
  CHECK(Refused(rules));
  const std::array<ExpSegment, 3> unsorted_curve{{{.from_level = 1, .base = 5, .per_level = 10},
                                                  {.from_level = 20, .base = 195, .per_level = 13},
                                                  {.from_level = 10, .base = 95, .per_level = 10}}};
  rules.exp_curve = unsorted_curve;
  CHECK(Refused(rules));
  const std::array<ExpSegment, 2> free_curve{{{.from_level = 1, .base = 5, .per_level = 10},
                                              {.from_level = 20, .base = 0, .per_level = 13}}};
  rules.exp_curve = free_curve;
  CHECK(Refused(rules));

  rules = TinyRules();
  rules.elements = std::span(ro::test_support::kTinyElements).first(9);
  CHECK(Refused(rules));
  rules.elements = {};
  CHECK(Refused(rules));
}

TEST_CASE("Règles survivors : monstres, cadre et plafonds incohérents sont refusés",
          "[sim][survivors]") {
  SurvivorsRules rules = TinyRules();
  std::array<FoeRule, 2> foes = kTinyFoes;
  rules.foes = foes;
  foes[1].hp = 0;
  CHECK(Refused(rules));
  foes[1].hp = -1;
  CHECK(Refused(rules));
  foes = kTinyFoes;
  foes[0].touch_interval_ms = 0;
  CHECK(Refused(rules));
  // Sans dégâts de contact, l'intervalle ne sert pas.
  foes[0].touch_damage = 0;
  CHECK(Accepted(rules));

  rules = TinyRules();
  rules.spawn_frame = {};
  CHECK(Refused(rules));
  rules = TinyRules();
  rules.spawn_frame.half_width_units = 0;
  CHECK(Refused(rules));
  rules = TinyRules();
  rules.spawn_frame.north_units = 0;
  rules.spawn_frame.south_units = 0;
  CHECK(Refused(rules));
  rules = TinyRules();
  rules.spawn_frame.north_units = -1;
  CHECK(Refused(rules));
  rules = TinyRules();
  rules.spawn_frame.south_units = -1;
  CHECK(Refused(rules));
  // Un côté nul suffit, si l'autre ne l'est pas.
  rules = TinyRules();
  rules.spawn_frame.north_units = 0;
  CHECK(Accepted(rules));
  rules = TinyRules();
  rules.spawn_frame.margin_units = -1;
  CHECK(Refused(rules));
  rules = TinyRules();
  rules.spawn_frame.margin_units = 0;
  CHECK(Accepted(rules));

  // Chaque mesure du cadre tient dans le côté de la plus grande arène.
  const std::int32_t widest = ro::sim::Cells(ro::sim::kMostArenaSideCells);
  rules = TinyRules();
  rules.spawn_frame = {.half_width_units = widest,
                       .north_units = widest,
                       .south_units = widest,
                       .margin_units = widest};
  CHECK(Accepted(rules));
  rules.spawn_frame.half_width_units = widest + 1;
  CHECK(Refused(rules));
  rules.spawn_frame.half_width_units = widest;
  rules.spawn_frame.north_units = widest + 1;
  CHECK(Refused(rules));
  rules.spawn_frame.north_units = widest;
  rules.spawn_frame.south_units = widest + 1;
  CHECK(Refused(rules));
  rules.spawn_frame.south_units = widest;
  rules.spawn_frame.margin_units = std::numeric_limits<std::int32_t>::max();
  CHECK(Refused(rules));

  rules = TinyRules();
  rules.caps.most_foes = 0;
  CHECK(Refused(rules));
  rules = TinyRules();
  rules.caps.most_shots = 0;
  CHECK(Refused(rules));
  rules = TinyRules();
  rules.caps.most_strikes = 0;
  CHECK(Refused(rules));
  rules = TinyRules();
  rules.caps.most_gems = 0;
  CHECK(Refused(rules));
  rules = TinyRules();
  rules.caps.skill_slots = 0;
  CHECK(Refused(rules));
  rules = TinyRules();
  rules.caps.card_slots = 0;
  CHECK(Refused(rules));

  // Un monstre suit au plus kMostSkillSlots emplacements de skill.
  rules = TinyRules();
  rules.caps.skill_slots = static_cast<std::uint8_t>(ro::sim::kMostSkillSlots + 1);
  CHECK(Refused(rules));
  rules.caps.skill_slots = static_cast<std::uint8_t>(ro::sim::kMostSkillSlots);
  CHECK(Accepted(rules));

  rules = TinyRules();
  rules.caps.offers = 0;
  CHECK(Refused(rules));
  rules.caps.offers = static_cast<std::uint8_t>(ro::sim::kMostOffers + 1);
  CHECK(Refused(rules));
  rules.caps.offers = 1;
  CHECK(Accepted(rules));
  rules.caps.offers = ro::sim::kMostOffers;
  CHECK(Accepted(rules));
}

TEST_CASE("Survivors : les classes qui peuvent obtenir une carte", "[survivors][rules]") {
  constexpr std::uint16_t kSharedCard = 4001;
  constexpr std::uint16_t kOwnCard = 4002;
  constexpr std::uint16_t kNobodysCard = 4003;
  constexpr std::uint16_t kSecondClassId = 77;
  const std::array<std::uint16_t, 2> first_pool{kSharedCard, kOwnCard};
  const std::array<std::uint16_t, 1> second_pool{kSharedCard};
  std::array<ClassRule, 2> classes{kTinyClasses[0], kTinyClasses[0]};
  classes[0].card_pool = first_pool;
  classes[1].class_id = kSecondClassId;
  classes[1].card_pool = second_pool;
  SurvivorsRules rules = TinyRules();
  rules.classes = classes;

  CHECK(ro::sim::ClassesHoldingCard(rules, kSharedCard).size() == 2U);
  const std::vector<const ClassRule*> own = ro::sim::ClassesHoldingCard(rules, kOwnCard);
  REQUIRE(own.size() == 1U);
  CHECK(own.front()->class_id == kTinyClasses[0].class_id);
  CHECK(ro::sim::ClassesHoldingCard(rules, kNobodysCard).empty());
}
