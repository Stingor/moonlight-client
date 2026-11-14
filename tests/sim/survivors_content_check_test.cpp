#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <span>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

#include "ro/core/error.h"
#include "ro/sim/survivors_content.h"
#include "ro/sim/survivors_geometry.h"
#include "ro/sim/survivors_rules.h"

#include "support/survivors_content_fixture.h"
#include "support/survivors_fixture.h"

using ro::sim::BossMoveKind;
using ro::sim::Cells;
using ro::sim::CheckSurvivorsRules;
using ro::sim::EventKind;
using ro::sim::FoeMove;
using ro::sim::kMostArenaSideCells;
using ro::sim::kMostStatBonus;
using ro::sim::PickupKind;
using ro::sim::SkillArchetype;
using ro::sim::SurvivorsRules;
using ro::sim::TargetRule;
using ro::test_support::ContentTables;

namespace {

// LE CONTRÔLE DU CONTENU D'UNE ARÈNE COMPLÈTE : chaque refus ferme une division
// par zéro, une boucle sans fin ou un débordement que la simulation ferait en
// lisant ces règles. Chaque essai part des règles de contenu, en retouche une
// valeur, et attend un refus ; à la borne, elles se jouent.

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

// Refusé après `change`, sur une variante fraîche qui passait.
[[nodiscard]] bool RefusedAfter(const std::function<void(ContentTables&)>& change) {
  ContentTables tables;
  REQUIRE(Accepted(tables.rules));
  change(tables);
  return Refused(tables.rules);
}

// Accepté après `change`, sur une variante fraîche.
[[nodiscard]] bool AcceptedAfter(const std::function<void(ContentTables&)>& change) {
  ContentTables tables;
  change(tables);
  return Accepted(tables.rules);
}

constexpr std::int32_t kLargestMeasure = Cells(kMostArenaSideCells);
constexpr std::int32_t kSignedStepsPerSecond = static_cast<std::int32_t>(ro::sim::kStepsPerSecond);
constexpr std::uint32_t kStepMs = ro::sim::kSurvivorsStepMs;
// Les index des espèces de contenu dans ContentTables::foes.
constexpr std::size_t kArcherIndex = 5;
constexpr std::size_t kSavageIndex = 6;
constexpr std::size_t kBathoryIndex = 7;
constexpr std::size_t kSmokieIndex = 8;
constexpr std::size_t kPoporingIndex = 9;
constexpr std::size_t kWolfIndex = 10;
constexpr std::size_t kChargeMove = 0;
constexpr std::size_t kSummonMove = 1;
constexpr std::size_t kRingEvent = 2;
constexpr std::size_t kHolyWaterPickup = 5;
constexpr std::uint16_t kUnknownId = 9999;

}  // namespace

TEST_CASE("Contenu survivors : les règles de contenu et le ruleset livré passent",
          "[sim][survivors]") {
  CHECK(Accepted(ro::test_support::ContentRules()));
  CHECK(Accepted(ro::sim::ShippedSurvivorsRules()));
  // Les index de la variante désignent bien ce que les essais retouchent.
  ContentTables tables;
  CHECK(tables.foes[kArcherIndex].behaviour.move == FoeMove::kRanged);
  CHECK(tables.foes[kSavageIndex].behaviour.move == FoeMove::kCharger);
  CHECK(tables.foes[kBathoryIndex].behaviour.move == FoeMove::kBlinker);
  CHECK((tables.foes[kSmokieIndex].behaviour.traits & ro::sim::kFoeHides) != 0);
  CHECK(tables.foes[kPoporingIndex].behaviour.puddle_damage > 0);
  CHECK(tables.foes[kWolfIndex].behaviour.pack > 1);
  CHECK(tables.boss_moves[kChargeMove].kind == BossMoveKind::kCharge);
  CHECK(tables.boss_moves[kSummonMove].kind == BossMoveKind::kSummon);
  CHECK(tables.events[kRingEvent].kind == EventKind::kRing);
  CHECK(tables.pickups[kHolyWaterPickup].kind == PickupKind::kHolyWater);
}

TEST_CASE("Contenu survivors : le ruleset 3 emploie chaque mécanique d'une arène complète",
          "[sim][survivors]") {
  // Une mécanique que le catalogue livré laisse à son défaut inerte n'est
  // éprouvée par aucune partie : la partie en or ne la verrait pas.
  const SurvivorsRules& shipped = ro::sim::ShippedSurvivorsRules();
  // La méta : trente et un porteurs de carte, la Cursed Water entière.
  CHECK(std::count_if(shipped.foes.begin(), shipped.foes.end(),
                      [](const ro::sim::FoeRule& foe) { return foe.card_item_id != 0; }) == 31);
  CHECK(shipped.cursed_water.size() == std::size_t{ro::sim::kMostCursedWater} + 1);
  CHECK(shipped.card_drops.common_odds == ro::sim::kCommonCardOdds);
  CHECK_FALSE(shipped.events.empty());
  CHECK_FALSE(shipped.evolutions.empty());
  CHECK_FALSE(shipped.plants.empty());
  CHECK(shipped.elites.interval_ms != 0U);
  CHECK(shipped.chests.item_id != 0);
  CHECK(ro::sim::HasFinalBoss(shipped));
  std::array<bool, 4> event_kinds{};
  for (const ro::sim::EventRule& event : shipped.events) {
    event_kinds[static_cast<std::size_t>(event.kind)] = true;
  }
  CHECK(event_kinds == std::array<bool, 4>{true, true, true, true});
  // Une règle d'objet par sorte, le coffre compris.
  constexpr std::size_t kPickupKinds = 7;
  for (std::size_t kind = 0; kind < kPickupKinds; ++kind) {
    CHECK(ro::sim::FindPickup(shipped, static_cast<PickupKind>(kind)) != nullptr);
  }

  // Chaque coup de boss, une escorte, une marche propre à chaque boss.
  std::array<bool, 6> move_kinds{};
  bool escorted = false;
  for (const ro::sim::BossRule& boss : shipped.bosses) {
    CHECK(boss.walk_ms_per_cell != 0U);
    escorted = escorted || boss.escort_count > 0;
    for (const ro::sim::BossMoveRule& move : boss.moves) {
      move_kinds[static_cast<std::size_t>(move.kind)] = true;
    }
  }
  CHECK(escorted);
  CHECK(move_kinds == std::array<bool, 6>{true, true, true, true, true, true});

  // Chaque comportement, chaque trait, une meute et une flaque.
  std::array<bool, 7> moves{};
  std::uint8_t traits = 0;
  bool pack = false;
  bool puddle = false;
  for (const ro::sim::FoeRule& foe : shipped.foes) {
    moves[static_cast<std::size_t>(foe.behaviour.move)] = true;
    traits = static_cast<std::uint8_t>(traits | foe.behaviour.traits);
    pack = pack || foe.behaviour.pack > 1;
    puddle = puddle || foe.behaviour.puddle_damage > 0;
  }
  CHECK(moves == std::array<bool, 7>{true, true, true, true, true, true, true});
  constexpr std::uint8_t kEveryTrait = ro::sim::kFoeKnockbackImmune | ro::sim::kFoeFreezeImmune |
                                       ro::sim::kFoePassesWalls | ro::sim::kFoeHides |
                                       ro::sim::kFoeOverHorde;
  CHECK(traits == kEveryTrait);
  CHECK(pack);
  CHECK(puddle);

  // Chaque archétype, le plus dense, le héros pour cible, le gel, le gel dès
  // une vague, le recul, le rebond, l'aveuglement, la malédiction, le poison,
  // l'union, et les invocations qui marchent, soignent ou échangent leur place,
  // sous un groupe exclusif.
  constexpr std::size_t kArchetypeCount = 15;
  bool walking_summon = false;
  bool healing_summon = false;
  bool swapping_summon = false;
  bool exclusive = false;
  for (const ro::sim::SurvivorsSkillRule& skill : shipped.skills) {
    exclusive = exclusive || skill.exclusive_group != 0;
    if (skill.archetype != ro::sim::SkillArchetype::kSummon) continue;
    for (const ro::sim::SkillLevel& level : skill.levels) {
      walking_summon = walking_summon || level.speed_units_per_s > 0;
      healing_summon = healing_summon || level.heal_pct > 0;
      swapping_summon = swapping_summon || level.reflex_below_hp_pct > 0;
    }
  }
  CHECK(walking_summon);
  CHECK(healing_summon);
  CHECK(swapping_summon);
  CHECK(exclusive);
  std::array<bool, kArchetypeCount> archetypes{};
  bool densest = false;
  bool on_hero = false;
  bool frozen = false;
  bool frozen_late = false;
  bool knocked = false;
  bool bounced = false;
  bool blinded = false;
  bool cursed = false;
  bool poisoned = false;
  for (const ro::sim::SurvivorsSkillRule& skill : shipped.skills) {
    archetypes.at(static_cast<std::size_t>(skill.archetype)) = true;
    densest = densest || skill.target == TargetRule::kDensest;
    on_hero = on_hero || skill.target == TargetRule::kHero;
    for (const ro::sim::SkillLevel& level : skill.levels) {
      frozen = frozen || level.freeze_ms > 0;
      frozen_late = frozen_late || level.freeze_from_wave > 1;
      knocked = knocked || level.knockback_units > 0;
      bounced = bounced || level.bounces > 0;
      blinded = blinded || level.blind_ms > 0;
      cursed = cursed || level.curse_ms > 0;
      poisoned = poisoned || level.poison_ms > 0;
    }
  }
  std::array<bool, kArchetypeCount> every_archetype{};
  every_archetype.fill(true);
  CHECK(archetypes == every_archetype);
  CHECK(densest);
  CHECK(on_hero);
  CHECK(frozen);
  CHECK(frozen_late);
  CHECK(knocked);
  CHECK(bounced);
  CHECK(blinded);
  CHECK(cursed);
  CHECK(poisoned);
  CHECK(std::ranges::any_of(shipped.evolutions, [](const ro::sim::EvolutionRule& evolution) {
    return evolution.partner_skill_id != 0;
  }));

  // Les boss gardent leurs PV et leur marche ; la horde seule croît.
  CHECK(shipped.tuning.boss_hp_growth_pct_per_minute == 0);
  CHECK(shipped.tuning.boss_walk_scale_pct == 100);
  CHECK(shipped.tuning.foe_hp_growth_pct_per_minute == 4);
  CHECK(shipped.tuning.knock_units_per_step > 0);
  CHECK(shipped.tuning.final_overtime_ms == 300000U);
}

TEST_CASE("Contenu survivors : une posture demande un repos minimal", "[sim][survivors]") {
  SurvivorsRules rules = ro::sim::ShippedSurvivorsRules();
  CHECK_FALSE(Refused(rules));
  rules.tuning.stance_least_rest_ms = 0;
  CHECK(Refused(rules));
}

TEST_CASE("Contenu survivors : les recherches du contenu", "[sim][survivors]") {
  const SurvivorsRules& rules = ro::test_support::ContentRules();
  const ro::sim::EvolutionRule* of =
      ro::sim::FindEvolutionOf(rules, ro::test_support::kTinyStrikeId);
  REQUIRE(of != nullptr);
  CHECK(of->evolved_skill_id == ro::test_support::kContentEvolvedId);
  CHECK(ro::sim::FindEvolutionTo(rules, ro::test_support::kContentEvolvedId) == of);
  CHECK(ro::sim::FindEvolutionOf(rules, ro::test_support::kContentEvolvedId) == nullptr);
  CHECK(ro::sim::FindEvolutionTo(rules, ro::test_support::kTinyStrikeId) == nullptr);
  const ro::sim::PickupRule* holy = ro::sim::FindPickup(rules, PickupKind::kHolyWater);
  REQUIRE(holy != nullptr);
  CHECK(holy->item_id == 523);
  CHECK(ro::sim::HasFinalBoss(rules));
  CHECK(ro::sim::FindPickup(ro::test_support::TinyRules(), PickupKind::kChest) == nullptr);
}

TEST_CASE("Contenu survivors : un skill neuf qui ne frappe rien est refusé", "[sim][survivors]") {
  // Un éclat, une nova ou un cercle de piliers sans aire.
  CHECK(RefusedAfter([](ContentTables& t) { t.blast.area_radius_units = 0; }));
  CHECK(RefusedAfter([](ContentTables& t) { t.nova.area_radius_units = 0; }));
  CHECK(RefusedAfter([](ContentTables& t) { t.ring.area_radius_units = 0; }));
  // Un mur sans longueur, un mur ou des piliers sans vie.
  CHECK(RefusedAfter([](ContentTables& t) { t.wall.length_units = 0; }));
  CHECK(RefusedAfter([](ContentTables& t) { t.wall.life_ms = 0; }));
  CHECK(RefusedAfter([](ContentTables& t) { t.ring.life_ms = 0; }));
  // Un projectile droit qui ne bouge pas d'une sous-unité par pas, ou sans portée.
  CHECK(RefusedAfter(
      [](ContentTables& t) { t.linear.speed_units_per_s = kSignedStepsPerSecond - 1; }));
  CHECK(RefusedAfter([](ContentTables& t) { t.linear.reach_units = 0; }));
  CHECK(RefusedAfter([](ContentTables& t) { t.blast.reach_units = 0; }));
  // Un perçage au-delà de la mémoire d'un tir.
  CHECK(RefusedAfter([](ContentTables& t) {
    t.linear.pierce = static_cast<std::uint8_t>(ro::sim::kMostShotHits);
  }));
  // Aux bornes : une sous-unité par pas, le plus grand perçage, une nova sans
  // portée (elle frappe autour du héros).
  CHECK(AcceptedAfter([](ContentTables& t) {
    t.linear.speed_units_per_s = kSignedStepsPerSecond;
    t.linear.pierce = static_cast<std::uint8_t>(ro::sim::kMostShotHits - 1);
    t.nova.reach_units = 0;
    t.blast.area_radius_units = 1;
  }));
}

TEST_CASE("Contenu survivors : éclat, rebond et plus dense hors bornes sont refusés",
          "[sim][survivors]") {
  CHECK(RefusedAfter([](ContentTables& t) { t.blast.splash_pct = -1; }));
  CHECK(RefusedAfter([](ContentTables& t) { t.blast.splash_pct = kMostStatBonus + 1; }));
  CHECK(RefusedAfter([](ContentTables& t) { t.bolt.bounce_pct = -1; }));
  CHECK(RefusedAfter([](ContentTables& t) { t.bolt.bounce_pct = kMostStatBonus + 1; }));
  // Un rebond sans portée, ou plus de rebonds qu'un coup n'en retient.
  CHECK(RefusedAfter([](ContentTables& t) { t.bolt.bounce_reach_units = 0; }));
  CHECK(RefusedAfter([](ContentTables& t) {
    t.bolt.bounces = static_cast<std::uint8_t>(ro::sim::kMostShotHits);
  }));
  // Le plus dense ne se cherche que pour une frappe ou un éclat.
  CHECK(RefusedAfter([](ContentTables& t) {
    t.skills[ro::test_support::kContentLinearIndex].target = TargetRule::kDensest;
  }));
  CHECK(RefusedAfter([](ContentTables& t) {
    t.skills[ro::test_support::kContentNovaIndex].target = TargetRule::kDensest;
  }));
  // Aucun tirage n'est prévu pour viser un éclat au hasard.
  CHECK(RefusedAfter([](ContentTables& t) {
    t.skills[ro::test_support::kContentBlastIndex].target = TargetRule::kRandom;
  }));
  CHECK(AcceptedAfter([](ContentTables& t) {
    t.skills[ro::test_support::kContentBlastIndex].target = TargetRule::kStrongest;
  }));
  CHECK(AcceptedAfter([](ContentTables& t) {
    t.blast.splash_pct = kMostStatBonus;
    t.bolt.bounce_pct = kMostStatBonus;
    t.bolt.bounces = static_cast<std::uint8_t>(ro::sim::kMostShotHits - 1);
    t.blast.splash_pct = 0;
  }));
  // Le plus dense compte ses voisins dans l'aire de la frappe : sans aire, il
  // n'en compte aucun.
  CHECK(RefusedAfter([](ContentTables& t) {
    t.skills[ro::test_support::kContentBoltIndex].target = TargetRule::kDensest;
  }));
  CHECK(AcceptedAfter([](ContentTables& t) {
    t.skills[ro::test_support::kContentBoltIndex].target = TargetRule::kDensest;
    t.bolt.area_radius_units = Cells(1);
  }));
  // Sans rebond, la portée de rebond peut rester nulle.
  CHECK(AcceptedAfter([](ContentTables& t) {
    t.bolt.bounces = 0;
    t.bolt.bounce_reach_units = 0;
  }));
}

TEST_CASE("Contenu survivors : une mesure neuve au-delà de la plus grande arène est refusée",
          "[sim][survivors]") {
  CHECK(RefusedAfter([](ContentTables& t) { t.blast.area_radius_units = kLargestMeasure + 1; }));
  CHECK(RefusedAfter([](ContentTables& t) { t.wall.length_units = -1; }));
  CHECK(RefusedAfter([](ContentTables& t) { t.wall.knockback_units = kLargestMeasure + 1; }));
  CHECK(RefusedAfter([](ContentTables& t) { t.bolt.bounce_reach_units = kLargestMeasure + 1; }));
  CHECK(RefusedAfter(
      [](ContentTables& t) { t.foes[kArcherIndex].behaviour.reach_units = kLargestMeasure + 1; }));
  CHECK(RefusedAfter(
      [](ContentTables& t) { t.foes[kArcherIndex].behaviour.shot_radius_units = -1; }));
  CHECK(RefusedAfter([](ContentTables& t) {
    t.foes[kPoporingIndex].behaviour.puddle_radius_units = kLargestMeasure + 1;
  }));
  CHECK(RefusedAfter([](ContentTables& t) {
    t.foes[kSavageIndex].behaviour.dash_units_per_s = kLargestMeasure * kSignedStepsPerSecond + 1;
  }));
  CHECK(RefusedAfter(
      [](ContentTables& t) { t.boss_moves[kChargeMove].reach_units = kLargestMeasure + 1; }));
  CHECK(RefusedAfter([](ContentTables& t) { t.events[kRingEvent].radius_units = -1; }));
  CHECK(RefusedAfter(
      [](ContentTables& t) { t.pickups[kHolyWaterPickup].reach_units = kLargestMeasure + 1; }));
  CHECK(RefusedAfter(
      [](ContentTables& t) { t.rules.tuning.knock_units_per_step = kLargestMeasure + 1; }));
  CHECK(RefusedAfter([](ContentTables& t) { t.rules.tuning.pickup_radius_units = -1; }));
  CHECK(RefusedAfter(
      [](ContentTables& t) { t.rules.tuning.plant_block_cells = kMostArenaSideCells + 1; }));
  CHECK(AcceptedAfter([](ContentTables& t) {
    t.blast.area_radius_units = kLargestMeasure;
    t.wall.knockback_units = kLargestMeasure;
    t.foes[kArcherIndex].behaviour.reach_units = kLargestMeasure;
    t.foes[kSavageIndex].behaviour.dash_units_per_s = kLargestMeasure * kSignedStepsPerSecond;
    t.events[kRingEvent].radius_units = kLargestMeasure;
    t.rules.tuning.pickup_radius_units = kLargestMeasure;
    t.rules.tuning.plant_block_cells = kMostArenaSideCells;
  }));
}

TEST_CASE("Contenu survivors : un comportement sans rythme est refusé", "[sim][survivors]") {
  // Une meute vide ne fait naître personne pour un tirage.
  CHECK(RefusedAfter([](ContentTables& t) { t.foes[kWolfIndex].behaviour.pack = 0; }));
  // Un acte (tir, charge, bond, cachette) sans période.
  CHECK(RefusedAfter([](ContentTables& t) { t.foes[kArcherIndex].behaviour.period_ms = 0; }));
  CHECK(RefusedAfter([](ContentTables& t) { t.foes[kSavageIndex].behaviour.period_ms = 0; }));
  CHECK(RefusedAfter([](ContentTables& t) { t.foes[kBathoryIndex].behaviour.period_ms = 0; }));
  CHECK(RefusedAfter([](ContentTables& t) { t.foes[kSmokieIndex].behaviour.period_ms = 0; }));
  // Une annonce aussi longue que sa période ne laisse jamais jouer l'acte ; les
  // deux se comptent en pas entamés.
  CHECK(RefusedAfter([](ContentTables& t) {
    t.foes[kBathoryIndex].behaviour.telegraph_ms = t.foes[kBathoryIndex].behaviour.period_ms;
  }));
  CHECK(RefusedAfter([](ContentTables& t) {
    t.foes[kBathoryIndex].behaviour.telegraph_ms = t.foes[kBathoryIndex].behaviour.period_ms - 1;
  }));
  // Une cachette qui, annonce comprise, couvre toute sa période se renoue sans
  // fin : le monstre ne redevient jamais tangible.
  CHECK(RefusedAfter([](ContentTables& t) {
    t.foes[kSmokieIndex].behaviour.active_ms = t.foes[kSmokieIndex].behaviour.period_ms;
  }));
  CHECK(RefusedAfter([](ContentTables& t) {
    ro::sim::FoeBehaviour& smokie = t.foes[kSmokieIndex].behaviour;
    smokie.telegraph_ms = kStepMs;
    smokie.active_ms = smokie.period_ms - kStepMs;
  }));
  // Un tir ou une charge qui ne bouge pas d'une sous-unité par pas.
  CHECK(
      RefusedAfter([](ContentTables& t) { t.foes[kArcherIndex].behaviour.shot_units_per_s = 0; }));
  CHECK(RefusedAfter([](ContentTables& t) {
    t.foes[kSavageIndex].behaviour.dash_units_per_s = kSignedStepsPerSecond - 1;
  }));
  // Une flaque qui blesse sans période.
  CHECK(RefusedAfter(
      [](ContentTables& t) { t.foes[kPoporingIndex].behaviour.puddle_period_ms = 0; }));
  // Aux bornes : l'annonce plus courte d'un pas que la période, une cachette
  // qui laisse un pas tangible, une sous-unité par pas, une flaque inoffensive
  // sans période.
  CHECK(AcceptedAfter([](ContentTables& t) {
    t.foes[kBathoryIndex].behaviour.telegraph_ms =
        t.foes[kBathoryIndex].behaviour.period_ms - kStepMs;
    t.foes[kSmokieIndex].behaviour.active_ms = t.foes[kSmokieIndex].behaviour.period_ms - kStepMs;
    t.foes[kArcherIndex].behaviour.shot_units_per_s = kSignedStepsPerSecond;
    t.foes[kSavageIndex].behaviour.dash_units_per_s = kSignedStepsPerSecond;
    t.foes[kPoporingIndex].behaviour.puddle_damage = 0;
    t.foes[kPoporingIndex].behaviour.puddle_period_ms = 0;
  }));
}

TEST_CASE("Contenu survivors : un boss final mal placé est refusé", "[sim][survivors]") {
  // Un second boss, final aussi, puis final mais pas le dernier.
  ContentTables two_finals;
  std::array<ro::sim::BossRule, 2> bosses{{two_finals.bosses[0], two_finals.bosses[0]}};
  bosses[1].at_ms = bosses[0].at_ms + 1;
  two_finals.rules.bosses = bosses;
  CHECK(Refused(two_finals.rules));
  bosses[1].final = false;
  CHECK(Refused(two_finals.rules));
  // Le boss final en dernier : accepté.
  bosses[0].final = false;
  bosses[1].final = true;
  CHECK(Accepted(two_finals.rules));
  // Une escorte inconnue.
  CHECK(RefusedAfter([](ContentTables& t) { t.bosses[0].escort_foe_id = kUnknownId; }));
  CHECK(AcceptedAfter([](ContentTables& t) {
    t.bosses[0].escort_foe_id = kUnknownId;
    t.bosses[0].escort_count = 0;
  }));
}

TEST_CASE("Contenu survivors : un coup de boss incohérent est refusé", "[sim][survivors]") {
  // Une invocation sans espèce, ou d'espèce inconnue ; un coup qui nomme une
  // espèce inconnue.
  CHECK(RefusedAfter([](ContentTables& t) { t.boss_moves[kSummonMove].foe_id = 0; }));
  CHECK(RefusedAfter([](ContentTables& t) { t.boss_moves[kSummonMove].foe_id = kUnknownId; }));
  CHECK(RefusedAfter([](ContentTables& t) { t.boss_moves[kChargeMove].foe_id = kUnknownId; }));
  // Des seuils de PV vides ou au-delà de 100 %.
  CHECK(RefusedAfter([](ContentTables& t) {
    t.boss_moves[kChargeMove].to_hp_pct = t.boss_moves[kChargeMove].from_hp_pct;
  }));
  CHECK(RefusedAfter(
      [](ContentTables& t) { t.boss_moves[kChargeMove].from_hp_pct = ro::sim::kWholeHpPct + 1; }));
  // Sans période, ou annoncé aussi longtemps que sa période.
  CHECK(RefusedAfter([](ContentTables& t) { t.boss_moves[kSummonMove].period_ms = 0; }));
  CHECK(RefusedAfter([](ContentTables& t) {
    t.boss_moves[kChargeMove].telegraph_ms = t.boss_moves[kChargeMove].period_ms;
  }));
  CHECK(RefusedAfter([](ContentTables& t) {
    t.boss_moves[kChargeMove].telegraph_ms = t.boss_moves[kChargeMove].period_ms - 1;
  }));
  // Une majoration hors de [0, kMostStatBonus].
  CHECK(RefusedAfter([](ContentTables& t) { t.boss_moves[kChargeMove].boost_pct = -1; }));
  CHECK(RefusedAfter(
      [](ContentTables& t) { t.boss_moves[kChargeMove].boost_pct = kMostStatBonus + 1; }));
  // Aux bornes : une phase d'un point de PV, la majoration pleine, une charge
  // qui nomme une espèce connue.
  CHECK(AcceptedAfter([](ContentTables& t) {
    t.boss_moves[kChargeMove].from_hp_pct = 1;
    t.boss_moves[kChargeMove].to_hp_pct = 0;
    t.boss_moves[kChargeMove].boost_pct = kMostStatBonus;
    t.boss_moves[kChargeMove].foe_id = ro::test_support::kContentBaphometJrId;
    t.boss_moves[kChargeMove].telegraph_ms = t.boss_moves[kChargeMove].period_ms - kStepMs;
  }));
  // LE GEL : hors d'un séisme, sur un séisme sans dégâts, une moitié sans
  // l'autre, au-delà de ses bornes.
  constexpr std::size_t kQuakeMove = 2;
  constexpr std::int32_t kSlowPct = 40;
  constexpr std::uint32_t kSlowMs = 2000;
  const auto frost = [](ro::sim::BossMoveRule& move, std::int32_t slow_pct, std::uint32_t slow_ms) {
    move.slow_pct = slow_pct;
    move.slow_ms = slow_ms;
  };
  CHECK(
      RefusedAfter([&](ContentTables& t) { frost(t.boss_moves[kChargeMove], kSlowPct, kSlowMs); }));
  CHECK(RefusedAfter([&](ContentTables& t) {
    frost(t.boss_moves[kQuakeMove], kSlowPct, kSlowMs);
    t.boss_moves[kQuakeMove].damage = 0;
  }));
  CHECK(RefusedAfter([&](ContentTables& t) { frost(t.boss_moves[kQuakeMove], kSlowPct, 0); }));
  CHECK(RefusedAfter([&](ContentTables& t) { frost(t.boss_moves[kQuakeMove], 0, kSlowMs); }));
  CHECK(RefusedAfter([&](ContentTables& t) {
    frost(t.boss_moves[kQuakeMove], ro::sim::kMostFrostSlowPct + 1, kSlowMs);
  }));
  CHECK(RefusedAfter([&](ContentTables& t) {
    frost(t.boss_moves[kQuakeMove], kSlowPct, ro::sim::kMostFrostMs + 1);
  }));
  CHECK(AcceptedAfter([&](ContentTables& t) {
    frost(t.boss_moves[kQuakeMove], ro::sim::kMostFrostSlowPct, ro::sim::kMostFrostMs);
  }));
  // Un script plus long que ce qu'un boss suit.
  ContentTables long_script;
  std::array<ro::sim::BossMoveRule, ro::sim::kMostBossMoves + 1> moves{};
  for (ro::sim::BossMoveRule& move : moves) move = long_script.boss_moves[kSummonMove];
  long_script.bosses[0].moves = moves;
  CHECK(Refused(long_script.rules));
  long_script.bosses[0].moves = std::span(moves).first(ro::sim::kMostBossMoves);
  CHECK(Accepted(long_script.rules));
  // Un script sur une espèce qui joue un acte (Savage charge) ; sans script,
  // la même espèce peut être un boss.
  CHECK(RefusedAfter([](ContentTables& t) { t.bosses[0].foe_id = t.foes[kSavageIndex].foe_id; }));
  CHECK(AcceptedAfter([](ContentTables& t) {
    t.bosses[0].foe_id = t.foes[kSavageIndex].foe_id;
    t.bosses[0].moves = {};
  }));
}

TEST_CASE("Contenu survivors : un événement ou une élite incohérents sont refusés",
          "[sim][survivors]") {
  CHECK(RefusedAfter([](ContentTables& t) { t.events[1].at_ms = t.events[0].at_ms - 1; }));
  CHECK(RefusedAfter([](ContentTables& t) { t.events[0].foe_id = kUnknownId; }));
  CHECK(RefusedAfter([](ContentTables& t) { t.events[0].count = 0; }));
  CHECK(RefusedAfter([](ContentTables& t) { t.events[kRingEvent].radius_units = 0; }));
  // Des élites sans majoration de PV naîtraient à zéro PV.
  CHECK(RefusedAfter([](ContentTables& t) { t.rules.elites.hp_pct = 0; }));
  CHECK(RefusedAfter([](ContentTables& t) { t.rules.elites.touch_pct = -1; }));
  CHECK(RefusedAfter([](ContentTables& t) { t.rules.elites.exp_pct = kMostStatBonus + 1; }));
  // Aux bornes : deux événements au même instant, un essaim sans rayon, des
  // élites désactivées sans PV.
  CHECK(AcceptedAfter([](ContentTables& t) {
    t.events[1].at_ms = t.events[0].at_ms;
    t.events[0].radius_units = 0;
    t.rules.elites.hp_pct = kMostStatBonus;
  }));
  CHECK(AcceptedAfter([](ContentTables& t) {
    t.rules.elites.interval_ms = 0;
    t.rules.elites.hp_pct = 0;
  }));
}

TEST_CASE("Contenu survivors : une évolution incohérente est refusée", "[sim][survivors]") {
  CHECK(RefusedAfter([](ContentTables& t) { t.evolutions[0].skill_id = kUnknownId; }));
  CHECK(RefusedAfter([](ContentTables& t) { t.evolutions[0].card_item_id = kUnknownId; }));
  CHECK(RefusedAfter([](ContentTables& t) { t.evolutions[0].evolved_skill_id = kUnknownId; }));
  // L'évolué ne s'offre pas : il n'est dans aucune réserve de classe.
  ContentTables pooled;
  const std::array<std::uint16_t, 4> pool{
      ro::test_support::kTinyStrikeId, ro::test_support::kTinyHomingId,
      ro::test_support::kTinyOrbitId, ro::test_support::kContentEvolvedId};
  pooled.classes[0].skill_pool = pool;
  CHECK(Refused(pooled.rules));
  // Un skill à deux évolutions, un évolué qui vient de deux skills.
  ContentTables twice;
  std::array<ro::sim::EvolutionRule, 2> evolutions{{twice.evolutions[0], twice.evolutions[0]}};
  evolutions[1].evolved_skill_id = ro::test_support::kContentRingId;
  twice.rules.evolutions = evolutions;
  CHECK(Refused(twice.rules));
  evolutions[1].skill_id = ro::test_support::kContentWallId;
  evolutions[1].evolved_skill_id = ro::test_support::kContentEvolvedId;
  CHECK(Refused(twice.rules));
  evolutions[1].evolved_skill_id = ro::test_support::kContentRingId;
  CHECK(Accepted(twice.rules));
}

TEST_CASE("Contenu survivors : un butin incohérent est refusé", "[sim][survivors]") {
  // Une plante dont l'objet n'a pas de règle.
  CHECK(RefusedAfter(
      [](ContentTables& t) { t.rules.pickups = std::span(t.pickups).first(kHolyWaterPickup); }));
  // Deux règles pour une même sorte d'objet.
  CHECK(RefusedAfter([](ContentTables& t) { t.pickups[1].kind = PickupKind::kGold; }));
  // Des poids de plantes de somme nulle, ou au-delà de 32 bits.
  CHECK(RefusedAfter([](ContentTables& t) {
    for (ro::sim::PlantRule& plant : t.plants) plant.weight = 0;
  }));
  CHECK(RefusedAfter([](ContentTables& t) {
    for (ro::sim::PlantRule& plant : t.plants)
      plant.weight = std::numeric_limits<std::uint32_t>::max();
  }));
  // Des élites sans objet de coffre.
  CHECK(RefusedAfter([](ContentTables& t) { t.rules.chests.item_id = 0; }));
  // Un coffre qui promet plus de gains que sa nouvelle n'en porte, ou en désordre.
  CHECK(RefusedAfter([](ContentTables& t) {
    t.rules.chests.quintuple = static_cast<std::uint8_t>(ro::sim::kMostChestGains + 1);
  }));
  CHECK(RefusedAfter([](ContentTables& t) { t.rules.chests.single = 0; }));
  CHECK(RefusedAfter([](ContentTables& t) { t.rules.chests.triple = 0; }));
  // Aux bornes : une seule plante de poids un, des gains tous égaux, un coffre
  // sans objet quand aucune élite ne naît.
  CHECK(AcceptedAfter([](ContentTables& t) {
    for (ro::sim::PlantRule& plant : t.plants) plant.weight = 0;
    t.plants[0].weight = 1;
    t.rules.chests.single = static_cast<std::uint8_t>(ro::sim::kMostChestGains);
    t.rules.chests.triple = static_cast<std::uint8_t>(ro::sim::kMostChestGains);
    t.rules.chests.quintuple = static_cast<std::uint8_t>(ro::sim::kMostChestGains);
  }));
  CHECK(AcceptedAfter([](ContentTables& t) {
    t.rules.elites.interval_ms = 0;
    t.rules.chests.item_id = 0;
  }));
}

TEST_CASE("Contenu survivors : des réglages neufs hors bornes sont refusés", "[sim][survivors]") {
  CHECK(RefusedAfter([](ContentTables& t) { t.rules.tuning.boss_hp_growth_pct_per_minute = -1; }));
  CHECK(RefusedAfter(
      [](ContentTables& t) { t.rules.tuning.boss_walk_scale_pct = kMostStatBonus + 1; }));
  // Des plantes sans bloc, ou sans densité : la chance d'un bloc se divise.
  CHECK(RefusedAfter([](ContentTables& t) { t.rules.tuning.plant_block_cells = 0; }));
  CHECK(RefusedAfter([](ContentTables& t) { t.rules.tuning.plant_cells_per_plant = 0; }));
  // Un recul sans vitesse ne finit jamais.
  CHECK(RefusedAfter([](ContentTables& t) { t.rules.tuning.knock_units_per_step = 0; }));
  // Le dépassement d'un boss final au-delà de 32 bits de ms.
  CHECK(RefusedAfter([](ContentTables& t) {
    t.rules.tuning.final_overtime_ms =
        std::numeric_limits<std::uint32_t>::max() - t.rules.run_ms + 1;
  }));
  // Un plafond neuf nul.
  CHECK(RefusedAfter([](ContentTables& t) { t.rules.caps.most_blasts = 0; }));
  CHECK(RefusedAfter([](ContentTables& t) { t.rules.caps.most_zones = 0; }));
  CHECK(RefusedAfter([](ContentTables& t) { t.rules.caps.most_foe_shots = 0; }));
  CHECK(RefusedAfter([](ContentTables& t) { t.rules.caps.most_hazards = 0; }));
  CHECK(RefusedAfter([](ContentTables& t) { t.rules.caps.most_pickups = 0; }));
  // Aux bornes : majorations pleines, dépassement jusqu'à 32 bits ; sans plante
  // ni recul, ces réglages peuvent rester nuls.
  CHECK(AcceptedAfter([](ContentTables& t) {
    t.rules.tuning.boss_hp_growth_pct_per_minute = kMostStatBonus;
    t.rules.tuning.boss_walk_scale_pct = 0;
    t.rules.tuning.final_overtime_ms = std::numeric_limits<std::uint32_t>::max() - t.rules.run_ms;
  }));
  CHECK(AcceptedAfter([](ContentTables& t) {
    t.rules.plants = {};
    t.rules.tuning.plant_block_cells = 0;
    t.rules.tuning.plant_cells_per_plant = 0;
    t.wall.knockback_units = 0;
    t.rules.tuning.knock_units_per_step = 0;
  }));
}

TEST_CASE("Contenu survivors : une méta incohérente est refusée", "[sim][survivors]") {
  CHECK(RefusedAfter([](ContentTables& t) { t.foes[0].card_item_id = kUnknownId; }));
  CHECK(RefusedAfter([](ContentTables& t) { t.rules.card_drops.common_odds = 0; }));
  // La Cursed Water : vide, ou un rang de 0 à kMostCursedWater, rang 0 neutre.
  static constexpr std::array<ro::sim::CursedWaterRank, ro::sim::kMostCursedWater + 1> kRanks{
      {{}, {.foe_hp_pct = 20, .touch_pct = 10}, {}, {}, {}, {.foe_hp_pct = kMostStatBonus}}};
  CHECK(AcceptedAfter([](ContentTables& t) { t.rules.cursed_water = kRanks; }));
  CHECK(RefusedAfter(
      [](ContentTables& t) { t.rules.cursed_water = std::span(kRanks).first(kRanks.size() - 1); }));
  static constexpr std::array<ro::sim::CursedWaterRank, ro::sim::kMostCursedWater + 1>
      kNoisyRankZero{{{.foe_hp_pct = 0, .touch_pct = 1}}};
  CHECK(RefusedAfter([](ContentTables& t) { t.rules.cursed_water = kNoisyRankZero; }));
  static constexpr std::array<ro::sim::CursedWaterRank, ro::sim::kMostCursedWater + 1> kTooCursed{
      {{}, {.foe_hp_pct = kMostStatBonus + 1}}};
  CHECK(RefusedAfter([](ContentTables& t) { t.rules.cursed_water = kTooCursed; }));
  static constexpr std::array<ro::sim::CursedWaterRank, ro::sim::kMostCursedWater + 1> kHealing{
      {{}, {.foe_hp_pct = 0, .touch_pct = -1}}};
  CHECK(RefusedAfter([](ContentTables& t) { t.rules.cursed_water = kHealing; }));
  // La relève : des PV dans [1, 100] %, une onde dans la plus grande arène.
  CHECK(RefusedAfter([](ContentTables& t) { t.rules.revive.hp_pct = 0; }));
  CHECK(RefusedAfter([](ContentTables& t) { t.rules.revive.hp_pct = 101; }));
  CHECK(AcceptedAfter([](ContentTables& t) { t.rules.revive.hp_pct = 100; }));
  CHECK(RefusedAfter([](ContentTables& t) { t.rules.revive.push_units = kLargestMeasure + 1; }));
  CHECK(RefusedAfter([](ContentTables& t) { t.rules.revive.push_units = -1; }));
}
