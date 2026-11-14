#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "ro/sim/survivors_geometry.h"
#include "ro/sim/survivors_play.h"
#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_run.h"
#include "ro/sim/walk_map.h"

namespace ro::test_support {

// LES PIÈCES COMMUNES DES ESSAIS DU MODE SURVIVORS : une arène dessinée, des
// règles minuscules et autonomes, et un joueur scripté. Rien n'y lit le
// catalogue livré : un essai de mécanique ne bouge pas quand le jeu s'équilibre.

// ── L'arène ─────────────────────────────────────────────────────────────────

inline constexpr std::int32_t kPillarPeriodCells = 12;
inline constexpr std::int32_t kPillarOffsetCells = 6;
inline constexpr std::int32_t kPillarSideCells = 2;

// Un carré de `side_cells` praticable, sauf des piliers de 2 × 2 toutes les 12
// cases, posés à 6 cases du coin de chaque maille : le centre d'une arène paire
// tombe hors d'un pilier.
[[nodiscard]] inline sim::WalkMap PillarArena(std::int32_t side_cells) {
  const std::int32_t side = side_cells > 0 ? side_cells : 0;
  std::vector<std::uint8_t> passable(
      static_cast<std::size_t>(side) * static_cast<std::size_t>(side), 1U);
  for (std::int32_t y = 0; y < side; ++y) {
    for (std::int32_t x = 0; x < side; ++x) {
      const std::int32_t in_x = x % kPillarPeriodCells - kPillarOffsetCells;
      const std::int32_t in_y = y % kPillarPeriodCells - kPillarOffsetCells;
      const bool pillar =
          in_x >= 0 && in_x < kPillarSideCells && in_y >= 0 && in_y < kPillarSideCells;
      if (pillar) {
        passable[static_cast<std::size_t>(y) * static_cast<std::size_t>(side) +
                 static_cast<std::size_t>(x)] = 0U;
      }
    }
  }
  return sim::WalkMap(side, side, std::move(passable));
}

// ── Les règles minuscules ───────────────────────────────────────────────────
//
// Une classe, un skill de chaque archétype (deux niveaux chacun), une carte,
// deux monstres, une vague, des plafonds petits. Tous les identifiants sont
// ceux du jeu, pour qu'un essai se lise.

inline constexpr std::uint16_t kTinyClassId = 1;
inline constexpr std::uint16_t kTinyArenaId = 1;
inline constexpr std::uint16_t kTinyStrikeId = 19;  // Fire Bolt
inline constexpr std::uint16_t kTinyHomingId = 13;  // Soul Strike
inline constexpr std::uint16_t kTinyOrbitId = 10;   // Sight
inline constexpr std::uint16_t kTinyCardId = 4019;  // Hornet Card
inline constexpr std::uint16_t kTinyLunaticId = 1063;
inline constexpr std::uint16_t kTinyPoringId = 1002;

inline constexpr std::array<sim::SkillLevel, 2> kTinyStrikeLevels{{
    {.damage = 10,
     .cooldown_ms = 1000,
     .amount = 2,
     .interval_ms = 120,
     .reach_units = sim::Cells(9)},
    {.damage = 14,
     .cooldown_ms = 900,
     .amount = 3,
     .interval_ms = 120,
     .reach_units = sim::Cells(9)},
}};
inline constexpr std::array<sim::SkillLevel, 2> kTinyHomingLevels{{
    {.damage = 8,
     .cooldown_ms = 1000,
     .amount = 2,
     .reach_units = sim::Cells(10),
     .speed_units_per_s = sim::Cells(9),
     .hit_radius_units = sim::CellTenths(4),
     .life_ms = 2000},
    {.damage = 8,
     .cooldown_ms = 1000,
     .amount = 3,
     .reach_units = sim::Cells(10),
     .speed_units_per_s = sim::Cells(9),
     .hit_radius_units = sim::CellTenths(4),
     .life_ms = 2000,
     .pierce = 1},
}};
inline constexpr std::array<sim::SkillLevel, 2> kTinyOrbitLevels{{
    {.damage = 10,
     .amount = 1,
     .reach_units = sim::Cells(2),
     .hit_radius_units = sim::CellTenths(6),
     .life_ms = 3000,
     .rest_ms = 3000,
     .period_ms = 1600,
     .rehit_ms = 500},
    {.damage = 10,
     .amount = 2,
     .reach_units = sim::Cells(2),
     .hit_radius_units = sim::CellTenths(6),
     .life_ms = 3000,
     .rest_ms = 0,
     .period_ms = 1600,
     .rehit_ms = 500},
}};
inline constexpr std::array<sim::SurvivorsSkillRule, 3> kTinySkills{{
    {.skill_id = kTinyStrikeId,
     .key = "tiny_strike",
     .description = "Frappe d'essai.",
     .archetype = sim::SkillArchetype::kTargetStrike,
     .target = sim::TargetRule::kNearest,
     .element = sim::SurvivorsElement::kFire,
     .levels = kTinyStrikeLevels},
    {.skill_id = kTinyHomingId,
     .key = "tiny_homing",
     .description = "Tir chercheur d'essai.",
     .archetype = sim::SkillArchetype::kHoming,
     .target = sim::TargetRule::kNearest,
     .element = sim::SurvivorsElement::kGhost,
     .levels = kTinyHomingLevels},
    {.skill_id = kTinyOrbitId,
     .key = "tiny_orbit",
     .description = "Orbite d'essai.",
     .archetype = sim::SkillArchetype::kOrbit,
     .target = sim::TargetRule::kNearest,
     .element = sim::SurvivorsElement::kFire,
     .levels = kTinyOrbitLevels},
}};

inline constexpr std::array<std::int32_t, 2> kTinyCardLevels{10, 20};
inline constexpr std::array<sim::CardRule, 1> kTinyCards{{
    {.card_item_id = kTinyCardId,
     .key = "tiny_card",
     .description = "Carte d'essai.",
     .stat = sim::SurvivorsStat::kDamagePct,
     .per_level = kTinyCardLevels},
}};

inline constexpr std::array<std::uint16_t, 3> kTinySkillPool{kTinyStrikeId, kTinyHomingId,
                                                             kTinyOrbitId};
inline constexpr std::array<std::uint16_t, 1> kTinyCardPool{kTinyCardId};
inline constexpr std::array<sim::ClassRule, 1> kTinyClasses{{
    {.class_id = kTinyClassId,
     .key = "tiny",
     .job_id = 2,
     .weapon_item_id = 1601,
     .max_hp = 100,
     .walk_ms_per_cell = 150,
     .magnet_units = sim::Cells(2),
     .damage_pct_per_level = 1,
     .damage_pct_cap = 50,
     .starter_skills = kTinySkillPool,
     .skill_pool = kTinySkillPool,
     .card_pool = kTinyCardPool},
}};

inline constexpr std::array<sim::FoeRule, 2> kTinyFoes{{
    {.foe_id = kTinyLunaticId,
     .ro_level = 3,
     .rank = sim::FoeRank::kCommon,
     .size = sim::FoeSize::kSmall,
     .element = sim::SurvivorsElement::kNeutral,
     .hp = 6,
     .ro_walk_ms_per_cell = 200,
     .touch_damage = 3,
     .touch_interval_ms = 1456,
     .exp = 1},
    {.foe_id = kTinyPoringId,
     .ro_level = 1,
     .rank = sim::FoeRank::kCommon,
     .size = sim::FoeSize::kMedium,
     .element = sim::SurvivorsElement::kWater,
     .hp = 8,
     .ro_walk_ms_per_cell = 400,
     .touch_damage = 4,
     .touch_interval_ms = 1872,
     .exp = 1},
}};
inline constexpr std::array<std::uint16_t, 2> kTinyWaveFoes{kTinyLunaticId, kTinyPoringId};
inline constexpr std::array<std::uint32_t, 2> kTinyWaveWeights{1, 1};
inline constexpr std::array<sim::WaveRule, 1> kTinyWaves{{
    {.from_ms = 0,
     .foe_ids = kTinyWaveFoes,
     .weights = kTinyWaveWeights,
     .floor = 4,
     .interval_ms = 1000,
     .batch = 1},
}};
inline constexpr std::array<sim::ArenaRule, 1> kTinyArenas{{
    {.arena_id = kTinyArenaId,
     .key = "tiny",
     .map_name = "",
     .start = std::nullopt,
     .ranked = true},
}};
inline constexpr std::array<sim::ExpSegment, 1> kTinyExpCurve{{
    {.from_level = 1, .base = 5, .per_level = 10},
}};
// Attaque (ligne) contre défense (colonne), en %, dans l'ordre de
// SurvivorsElement : la sous-table de niveau 1 des éléments du serveur.
inline constexpr std::array<sim::ElementRow, sim::kSurvivorsElementCount> kTinyElements{{
    {100, 100, 100, 100, 100, 100, 100, 100, 25, 100},
    {100, 25, 100, 150, 50, 100, 75, 100, 100, 100},
    {100, 100, 100, 50, 150, 100, 75, 100, 100, 100},
    {100, 50, 150, 25, 100, 100, 75, 100, 100, 125},
    {100, 175, 50, 100, 25, 100, 75, 100, 100, 100},
    {100, 100, 125, 125, 125, 0, 75, 50, 100, -25},
    {100, 100, 100, 100, 100, 100, 0, 125, 100, 150},
    {100, 100, 100, 100, 100, 50, 125, 0, 100, -25},
    {25, 100, 100, 100, 100, 100, 75, 75, 125, 100},
    {100, 100, 100, 100, 100, 50, 100, 0, 100, 0},
}};

inline constexpr std::uint32_t kTinyRunMs = 60000;

// Les règles minuscules : une table immuable, qui survit à toute partie.
// Un essai qui veut les retoucher en fait une COPIE locale, qu'il garde en vie
// aussi longtemps que la partie qui la lit.
[[nodiscard]] inline const sim::SurvivorsRules& TinyRules() {
  static const sim::SurvivorsRules kRules{
      .ruleset_version = sim::kSurvivorsRulesetVersion,
      .run_ms = kTinyRunMs,
      .classes = kTinyClasses,
      .skills = kTinySkills,
      .cards = kTinyCards,
      .foes = kTinyFoes,
      .waves = kTinyWaves,
      .bosses = {},
      .arenas = kTinyArenas,
      .exp_curve = kTinyExpCurve,
      .elements = kTinyElements,
      .spawn_frame = {.half_width_units = sim::Cells(10),
                      .north_units = sim::Cells(8),
                      .south_units = sim::Cells(7),
                      .margin_units = sim::Cells(1)},
      .caps = {.most_foes = 32,
               .most_shots = 16,
               .most_strikes = 32,
               .most_gems = 32,
               .skill_slots = 6,
               .card_slots = 6,
               .offers = 3},
      .tuning = {.foe_walk_scale_pct = 160,
                 .foe_hp_growth_pct_per_minute = 15,
                 .chill_slow_pct = 30,
                 .boss_chill_share_pct = 50,
                 .hero_radius_units = sim::CellTenths(4),
                 .foe_radius_units = {358, 460, 716},
                 .touch_reach_units = 128,
                 .direct_chase_units = sim::Cells(3),
                 .recall_units = sim::Cells(48),
                 .flow_radius_cells = 20,
                 .flow_refresh_steps = 5,
                 .separation_max_push_units = 128,
                 .hero_shove_pct = 20,
                 .gem_pull_start_units = 64,
                 .gem_pull_accel_units = 16,
                 .offer_weight_owned = 100,
                 .offer_weight_fresh = 60,
                 .heal_offer_hp = 30,
                 .zeny_offer = 25,
                 .most_floor_spawns_per_step = 10,
                 .spawn_tries_per_foe = 4,
                 .spawn_search_ring = 4,
                 .most_cooldown_pct = 60},
  };
  return kRules;
}

// ── Les règles silencieuses ─────────────────────────────────────────────────
//
// Les règles minuscules, sans aucune naissance : une vague à plancher et lot
// nuls, que CheckSurvivorsRules accepte. Un essai de mécanique y pose lui-même
// ses monstres par PlaceFoe, sans qu'une apparition vienne s'en mêler.

inline constexpr std::array<sim::WaveRule, 1> kQuietWaves{{
    {.from_ms = 0,
     .foe_ids = kTinyWaveFoes,
     .weights = kTinyWaveWeights,
     .floor = 0,
     .interval_ms = 1000,
     .batch = 0},
}};

[[nodiscard]] inline const sim::SurvivorsRules& QuietRules() {
  static const sim::SurvivorsRules kRules = [] {
    sim::SurvivorsRules rules = TinyRules();
    rules.waves = kQuietWaves;
    return rules;
  }();
  return kRules;
}

inline constexpr std::uint32_t kWholePct = 100;

// Un monstre `foe_id` posé en `at` comme SpawnFoes le ferait naître au pas
// courant, sans croissance de PV : numéro de naissance suivant, PV de sa
// règle, marche du réglage (en 64 bits, l'échelle négative ramenée à 0),
// contact prêt tout de suite, tourné vers le héros. Rend son numéro, ou 0
// (aucun monstre posé) pour un monstre inconnu des règles.
inline std::uint32_t PlaceFoe(sim::SurvivorsField& field, std::uint16_t foe_id,
                              sim::FieldPoint at) {
  const sim::FoeRule* rule = sim::FindFoe(*field.rules, foe_id);
  if (rule == nullptr) return 0;
  const auto walk_scale_pct =
      static_cast<std::uint64_t>(std::max<std::int32_t>(field.rules->tuning.foe_walk_scale_pct, 0));
  const std::uint64_t walk_ms =
      std::uint64_t{rule->ro_walk_ms_per_cell} * walk_scale_pct / kWholePct;
  sim::SurvivorsFoe foe;
  foe.serial = field.next_serial++;
  foe.foe_id = foe_id;
  foe.position = at;
  foe.previous_position = at;
  foe.hp = rule->hp;
  foe.max_hp = rule->hp;
  foe.units_per_step = sim::UnitsPerStep(static_cast<std::uint32_t>(
      std::min<std::uint64_t>(walk_ms, std::numeric_limits<std::uint32_t>::max())));
  foe.born_step = field.step;
  foe.touch_ready_step = field.step;
  foe.facing = sim::HeadingOf(field.hero.position - at, 0);
  field.foes.push_back(foe);
  return foe.serial;
}

// Le départ ordinaire des essais : la classe, l'arène et le skill de frappe
// des règles minuscules, sans loadout.
[[nodiscard]] inline sim::SurvivorsStart TinyStart(std::uint64_t seed) {
  return sim::SurvivorsStart{.seed = seed,
                             .class_id = kTinyClassId,
                             .arena_id = kTinyArenaId,
                             .first_skill_id = kTinyStrikeId,
                             .loadout = {}};
}

// L'entrée d'un pas au cap `heading`.
[[nodiscard]] inline sim::SurvivorsInput Heading(std::uint8_t heading) {
  return sim::SurvivorsInput{.heading = heading};
}

// Un bonus de loadout : la statistique `stat` vaut `value` dès le départ.
inline void SetBonus(sim::SurvivorsStart& start, sim::SurvivorsStat stat, std::int32_t value) {
  start.loadout.bonus.values[static_cast<std::size_t>(stat)] = value;
}

// `steps` pas sans cap : le héros reste où il est. La grille des monstres est
// celle du pas, refaite par RebuildGrid juste avant FireSkills.
inline void StepStill(sim::SurvivorsRun& run, std::uint32_t steps = 1) {
  for (std::uint32_t step = 0; step < steps; ++step) run.Step(sim::SurvivorsInput{});
}

// ── Le joueur scripté ───────────────────────────────────────────────────────

inline constexpr std::uint32_t kScriptedTurnSteps = 25;  // un cap de plus toutes les 0,5 s
inline constexpr std::int32_t kScriptedGemReachCells = 3;

// Le héros tourne en rond, un cap de plus toutes les 0,5 s (un tour en 32 s),
// sauf s'il voit une gemme à moins de 3 cases : il marche alors vers la plus
// proche (la première née à égalité). Le cercle tient la horde derrière lui,
// le détour ramasse ce qu'elle laisse. Tout est entier : le script rend la
// même entrée sous tout compilateur.
[[nodiscard]] inline sim::SurvivorsInput ScriptedInput(const sim::SurvivorsField& field) {
  const auto circling =
      static_cast<std::uint8_t>(field.step / kScriptedTurnSteps % sim::kHeadingSteps);
  const std::int64_t reach = sim::Cells(kScriptedGemReachCells);
  std::int64_t nearest = reach * reach + 1;
  const sim::SurvivorsGem* chosen = nullptr;
  for (const sim::SurvivorsGem& gem : field.gems) {
    const std::int64_t distance = sim::DistanceSquared(gem.position, field.hero.position);
    if (distance < nearest) {
      nearest = distance;
      chosen = &gem;
    }
  }
  if (chosen == nullptr) return sim::SurvivorsInput{.heading = circling};
  return sim::SurvivorsInput{.heading =
                                 sim::HeadingOf(chosen->position - field.hero.position, circling)};
}

// La réponse du joueur scripté : la première offre d'un skill nouveau s'il y
// en a une, sinon la première offre. Les skills d'abord : c'est ce qui fait
// passer la partie en or par chaque archétype.
[[nodiscard]] inline sim::SurvivorsAnswer ScriptedAnswer(const sim::SurvivorsField& field) {
  std::uint8_t chosen = 0;
  for (std::size_t index = 0; index < field.offers.size(); ++index) {
    if (field.offers[index].kind == sim::OfferKind::kNewSkill) {
      chosen = static_cast<std::uint8_t>(index);
      break;
    }
  }
  return sim::SurvivorsAnswer{.kind = sim::AnswerKind::kPick, .offer_index = chosen};
}

// `steps` pas de plus, par tours d'horloge de 20 ms, avec ScriptedInput, en
// répondant ScriptedAnswer à chaque choix. S'arrête à la fin de la partie,
// ou dès qu'un tour ne fait plus rien (réponse refusée, horloge immobile) : un
// essai ne boucle pas.
inline void PlayScripted(sim::SurvivorsPlay& play, std::uint32_t steps) {
  const std::uint32_t last_step = play.run().field().step + steps;
  while (play.run().field().step < last_step) {
    const sim::SurvivorsField& field = play.run().field();
    if (field.phase == sim::SurvivorsPhase::kOver) return;
    if (field.phase == sim::SurvivorsPhase::kChoosing) {
      if (!play.Answer(ScriptedAnswer(field))) return;
      continue;
    }
    if (play.Advance(sim::kSurvivorsStepMs, ScriptedInput(field)) == 0) return;
  }
}

}  // namespace ro::test_support
