#include "ro/sim/survivors_run.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "ro/sim/survivors_ground.h"

#include "survivors_space.h"
#include "survivors_systems.h"

namespace ro::sim {

namespace {

// Le départ se cherche jusqu'à deux anneaux d'apparition de la case voulue.
constexpr std::int32_t kStartSearchRings = 2;

[[nodiscard]] core::Result<SurvivorsRun> Refuse(std::string detail) {
  return core::Fail(core::ErrorCode::kInvalidArgument, "survivors : " + std::move(detail));
}

// Les Id verrouillés d'un départ : strictement croissants, au plus
// kMostLockedIds, chacun dans une réserve de la classe, jamais le premier
// skill.
[[nodiscard]] core::Result<void> CheckLockedIds(const ClassRule& hero_class,
                                                const SurvivorsStart& start) {
  const std::vector<std::uint16_t>& locked = start.locked_ids;
  if (locked.size() > kMostLockedIds) {
    return core::Fail(core::ErrorCode::kInvalidArgument,
                      "survivors : " + std::to_string(locked.size()) + " Id verrouillés, au plus " +
                          std::to_string(kMostLockedIds));
  }
  for (std::size_t index = 0; index < locked.size(); ++index) {
    const std::uint16_t id = locked[index];
    const auto in = [id](std::span<const std::uint16_t> pool) {
      return std::find(pool.begin(), pool.end(), id) != pool.end();
    };
    std::string problem;
    if (index > 0 && locked[index - 1] >= id) {
      problem = "Id verrouillés non croissants en " + std::to_string(id);
    } else if (!in(hero_class.skill_pool) && !in(hero_class.card_pool)) {
      problem = "Id verrouillé " + std::to_string(id) + " hors des réserves de la classe";
    } else if (id == start.first_skill_id) {
      problem = "premier skill verrouillé " + std::to_string(id);
    }
    if (!problem.empty()) {
      return core::Fail(core::ErrorCode::kInvalidArgument, "survivors : " + std::move(problem));
    }
  }
  return {};
}

// Le bilan de l'état présent : monstres vivants, niveau et PV du héros. Le
// compte des pas n'en fait pas partie.
void RefreshTally(SurvivorsField& field) {
  field.tally.foes_alive = static_cast<std::uint16_t>(
      std::min<std::size_t>(field.foes.size(), std::numeric_limits<std::uint16_t>::max()));
  field.tally.level = field.hero.level;
  field.tally.hp_left = field.hero.hp;
}

// La fin d'un pas : le compte des pas, le bilan, puis la fin de partie ou le
// choix ouvert par une montée de niveau. Une partie déjà finie dans ce pas (le
// héros tombé) compte encore son pas : le journal s'arrête après lui.
void CloseStep(SurvivorsField& field) {
  ++field.step;
  field.tally.survived_steps = field.step;
  RefreshTally(field);
  if (field.phase != SurvivorsPhase::kRunning) return;
  // Sans boss final dans les règles, la partie se gagne en tenant run_ms.
  // Avec, elle continue jusqu'à sa chute (kVictory), et finit au plus tard en
  // kSurvived après le dépassement.
  std::uint64_t end_ms = field.rules->run_ms;
  if (HasFinalBoss(*field.rules)) end_ms += field.rules->tuning.final_overtime_ms;
  if (std::uint64_t{field.step} * kSurvivorsStepMs >= end_ms) {
    field.phase = SurvivorsPhase::kOver;
    field.tally.outcome = SurvivorsOutcome::kSurvived;
    return;
  }
  if (field.pending_level_ups > 0) {
    survivors::BuildOffers(field);
    field.phase = SurvivorsPhase::kChoosing;
  }
}

}  // namespace

std::optional<CellPosition> SurvivorsStartCell(const SurvivorsRules& rules,
                                               const ArenaRule& arena_rule,
                                               const SurvivorsGround& ground) {
  const std::int32_t wanted_x = arena_rule.start ? arena_rule.start->x : ground.width() / 2;
  const std::int32_t wanted_y = arena_rule.start ? arena_rule.start->y : ground.height() / 2;
  return survivors::NearestPassable(ground, wanted_x, wanted_y,
                                    kStartSearchRings * rules.tuning.spawn_search_ring);
}

std::optional<CellPosition> SurvivorsStartCell(const SurvivorsRules& rules,
                                               const ArenaRule& arena_rule, const WalkMap& arena) {
  return SurvivorsStartCell(rules, arena_rule, SurvivorsGround::Bounded(arena));
}

SurvivorsRun::SurvivorsRun(SurvivorsField field) : field_(std::move(field)) {}

core::Result<SurvivorsRun> SurvivorsRun::Start(const SurvivorsRules& rules, WalkMap arena,
                                               const SurvivorsStart& start) {
  return Start(rules, SurvivorsGround::Bounded(std::move(arena)), start);
}

core::Result<SurvivorsRun> SurvivorsRun::Start(const SurvivorsRules& rules, SurvivorsGround ground,
                                               const SurvivorsStart& start) {
  if (core::Result<void> checked = CheckSurvivorsRules(rules); !checked) {
    return std::unexpected(std::move(checked.error()));
  }
  const ClassRule* hero_class = FindClass(rules, start.class_id);
  if (hero_class == nullptr) return Refuse("classe inconnue " + std::to_string(start.class_id));
  const ArenaRule* arena_rule = FindArena(rules, start.arena_id);
  if (arena_rule == nullptr) return Refuse("arène inconnue " + std::to_string(start.arena_id));
  if (FindSkill(rules, start.first_skill_id) == nullptr) {
    return Refuse("premier skill inconnu " + std::to_string(start.first_skill_id));
  }
  if (std::find(hero_class->starter_skills.begin(), hero_class->starter_skills.end(),
                start.first_skill_id) == hero_class->starter_skills.end()) {
    return Refuse("premier skill hors des skills de départ " +
                  std::to_string(start.first_skill_id));
  }
  if (!LoadoutWithinBounds(start.loadout)) {
    return Refuse("bonus de loadout au-delà de ±" + std::to_string(kMostStatBonus));
  }
  if (start.cursed_water > kMostCursedWater ||
      (start.cursed_water > 0 && start.cursed_water >= rules.cursed_water.size())) {
    return Refuse("Cursed Water " + std::to_string(start.cursed_water) + " hors des règles");
  }
  if (core::Result<void> locked = CheckLockedIds(*hero_class, start); !locked) {
    return std::unexpected(std::move(locked.error()));
  }
  if (ground.bounded() &&
      (ground.width() <= 0 || ground.height() <= 0 || ground.width() > kMostArenaSideCells ||
       ground.height() > kMostArenaSideCells)) {
    return Refuse("arène vide ou trop grande : " + std::to_string(ground.width()) + " × " +
                  std::to_string(ground.height()));
  }
  const std::optional<CellPosition> start_cell = SurvivorsStartCell(rules, *arena_rule, ground);
  if (!start_cell) {
    return Refuse("aucune case praticable près du départ de l'arène " +
                  std::to_string(arena_rule->arena_id));
  }

  SurvivorsField field;
  field.rules = &rules;
  field.hero_class = hero_class;
  field.start = start;
  if (ground.bounded()) {
    field.foe_grid.Reset(ground.width(), ground.height());
  } else {
    field.foe_grid.Reset(kFollowingGridCells, kFollowingGridCells);
  }
  field.ground = std::move(ground);
  field.spawn_random = core::Pcg32(start.seed, kSpawnStream);
  field.offer_random = core::Pcg32(start.seed, kOfferStream);
  field.combat_random = core::Pcg32(start.seed, kCombatStream);
  field.loot_random = core::Pcg32(start.seed, kLootStream);
  field.field_random = core::Pcg32(start.seed, kFieldStream);
  field.card_random = core::Pcg32(start.seed, kCardStream);
  field.next_elite_ms = rules.elites.first_ms;
  field.hero.position = CellCenter(start_cell->x, start_cell->y);
  field.hero.previous_position = field.hero.position;
  field.hero.skills.push_back(OwnedSkill{.skill_id = start.first_skill_id, .level = 1});
  field.rerolls_left = start.loadout.rerolls;
  field.skips_left = start.loadout.skips;
  field.banishes_left = start.loadout.banishes;
  field.lives_left = start.lives;
  survivors::PlantField(field);
  survivors::RecomputeStats(field);
  RefreshTally(field);
  return SurvivorsRun(std::move(field));
}

void SurvivorsRun::Step(SurvivorsInput input) {
  if (field_.phase != SurvivorsPhase::kRunning) return;
  // ⚠ L'ORDRE DU PAS EST LA RÈGLE DU JEU : le changer change chaque partie
  // rejouée, et monte kSurvivorsRulesetVersion.
  survivors::MoveHero(field_, input);
  survivors::RefreshFlow(field_);
  survivors::SpawnFoes(field_);
  survivors::SpawnEvents(field_);
  survivors::RunBossScripts(field_);
  survivors::SteerFoes(field_);
  survivors::MoveHorde(field_);
  survivors::RebuildGrid(field_);
  survivors::FireSkills(field_);
  survivors::LandStrikes(field_);
  survivors::LandBlasts(field_);
  survivors::AdvanceShots(field_);
  survivors::AdvanceZones(field_);
  survivors::BuryFoes(field_);
  // La chute du boss final clôt la partie dans BuryFoes : ni menace ni contact
  // ne blessent plus le héros, et une victoire l'emporte sur une chute du même
  // pas. Un héros tombé sous une menace ne prend plus de contact, ne ramasse
  // plus rien et ne se soigne plus.
  if (field_.phase == SurvivorsPhase::kRunning) survivors::AdvanceThreats(field_);
  if (field_.phase == SurvivorsPhase::kRunning) survivors::TouchHero(field_);
  if (field_.phase == SurvivorsPhase::kRunning) {
    survivors::GatherGems(field_);
    survivors::GatherPickups(field_);
    survivors::Regenerate(field_);
  }
  CloseStep(field_);
}

bool SurvivorsRun::Answer(SurvivorsAnswer answer) {
  if (field_.phase != SurvivorsPhase::kChoosing) return false;
  return survivors::ApplyAnswer(field_, answer);
}

void SurvivorsRun::Abandon() {
  if (field_.phase == SurvivorsPhase::kOver) return;
  // Une réponse donnée pendant le choix a pu changer les PV : le bilan suit
  // l'état du moment.
  RefreshTally(field_);
  field_.phase = SurvivorsPhase::kOver;
  field_.tally.outcome = SurvivorsOutcome::kAbandoned;
}

SurvivorsNews SurvivorsRun::TakeNews() { return std::exchange(field_.news, SurvivorsNews{}); }

void SurvivorsRun::Orbs(std::vector<SurvivorsOrb>& out) const {
  out.clear();
  survivors::CollectOrbs(field_, out);
}

}  // namespace ro::sim
