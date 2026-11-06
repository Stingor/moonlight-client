#include "ro/sim/survivors_run.h"

#include <algorithm>
#include <array>
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
  std::int64_t hp_left = 0;
  for (std::size_t index = 0; index < HeroCount(field); ++index) {
    hp_left += HeroAt(field, index).hp;
  }
  field.tally.hp_left = static_cast<std::int32_t>(
      std::min<std::int64_t>(hp_left, std::numeric_limits<std::int32_t>::max()));
}

// La fin d'un pas : le compte des pas, le bilan, puis la fin de partie ou le
// choix ouvert par une montée de niveau. Une partie déjà finie dans ce pas (le
// héros tombé) compte encore son pas : le journal s'arrête après lui.
void CloseStep(SurvivorsField& field) {
  ++field.step;
  field.tally.survived_steps = field.step;
  RefreshTally(field);
  if (field.phase != SurvivorsPhase::kRunning) return;
  // Sans boss final dans son contenu, la partie se gagne en tenant run_ms.
  // Avec, elle continue jusqu'à sa chute (kVictory), et finit au plus tard en
  // kSurvived après le dépassement.
  std::uint64_t end_ms = field.rules->run_ms;
  if (HasFinalBoss(field.content.bosses)) end_ms += field.rules->tuning.final_overtime_ms;
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

// LE DÉPART D'UN ALLIÉ, vu comme un départ à lui seul : ce qui se juge pour un
// joueur se juge de la même façon pour chacun.
[[nodiscard]] SurvivorsStart StartOfAlly(const SurvivorsStart& team,
                                         const SurvivorsAllyStart& ally) {
  SurvivorsStart start;
  start.seed = team.seed;
  start.class_id = ally.class_id;
  start.arena_id = team.arena_id;
  start.first_skill_id = ally.first_skill_id;
  start.loadout = ally.loadout;
  start.lives = ally.lives;
  start.locked_ids = ally.locked_ids;
  return start;
}

// Vrai si la case est celle d'un héros déjà placé.
[[nodiscard]] bool HeroStandsOn(const SurvivorsField& field, FieldPoint at) {
  for (std::size_t index = 0; index < HeroCount(field); ++index) {
    if (HeroAt(field, index).position == at) return true;
  }
  return false;
}

// Échange la scène et l'équipement `kit`, sauf ce qui appartient à l'équipe.
void Exchange(SurvivorsField& field, SurvivorsKit& kit) {
  std::swap(field.hero, kit.hero);
  std::swap(field.hero.level, kit.hero.level);
  std::swap(field.hero.exp, kit.hero.exp);
  std::swap(field.hero.exp_boost_until_step, kit.hero.exp_boost_until_step);
  std::swap(field.hero.zeny, kit.hero.zeny);
  std::swap(field.hero_class, kit.hero_class);
  std::swap(field.lives_left, kit.lives_left);
  std::swap(field.shots, kit.shots);
  std::swap(field.strikes, kit.strikes);
  std::swap(field.blasts, kit.blasts);
  std::swap(field.zones, kit.zones);
  std::swap(field.start.class_id, kit.start.class_id);
  std::swap(field.start.first_skill_id, kit.start.first_skill_id);
  std::swap(field.start.loadout, kit.start.loadout);
  std::swap(field.start.lives, kit.start.lives);
  std::swap(field.start.leaves, kit.start.leaves);
  std::swap(field.start.locked_ids, kit.start.locked_ids);
}

// L'équipement du héros `hero_index`, quand il n'est pas en scène.
[[nodiscard]] const SurvivorsKit& KitAt(const SurvivorsField& field, std::size_t hero_index) {
  return field.allies[(hero_index == 0 ? field.stage : hero_index) - 1U];
}

// Met en scène le premier héros debout dont c'est le tour : le théâtre des
// apparitions passe de l'un à l'autre, un pas sur deux à deux.
void PutAnchorOnStage(SurvivorsField& field) {
  if (field.allies.empty()) return;
  const std::size_t count = HeroCount(field);
  for (std::size_t turn = 0; turn < count; ++turn) {
    const std::size_t index = (field.step + turn) % count;
    if (!HeroAt(field, index).down) {
      PutOnStage(field, index);
      return;
    }
  }
}

// LA LAISSE : un pas qui éloigne un héros d'un autre au-delà de la distance
// maximale est annulé, mais le héros garde son cap.
void HoldLeash(SurvivorsField& field, FieldPoint from) {
  const std::int64_t leash = Cells(kTeamLeashCells);
  SurvivorsHero& hero = field.hero;
  for (std::size_t index = 0; index < HeroCount(field); ++index) {
    if (index == field.stage) continue;
    const SurvivorsHero& other = HeroAt(field, index);
    if (other.down) continue;
    const std::int64_t after = DistanceSquared(hero.position, other.position);
    if (after > leash * leash && after > DistanceSquared(from, other.position)) {
      hero.position = from;
      return;
    }
  }
}

}  // namespace

std::uint32_t TeamExpToNext(const SurvivorsField& field) {
  const std::uint32_t needed = ExpToNext(*field.rules, field.hero.level);
  if (field.allies.empty()) return needed;
  const std::uint64_t scaled = std::uint64_t{needed} *
                               static_cast<std::uint64_t>(TeamScaleOf(field).exp_needed_pct) /
                               static_cast<std::uint64_t>(survivors::kPercent);
  return static_cast<std::uint32_t>(std::max<std::uint64_t>(scaled, 1));
}

const SurvivorsHero& HeroAt(const SurvivorsField& field, std::size_t hero_index) {
  return hero_index == field.stage ? field.hero : KitAt(field, hero_index).hero;
}

const ClassRule* ClassAt(const SurvivorsField& field, std::size_t hero_index) {
  return hero_index == field.stage ? field.hero_class : KitAt(field, hero_index).hero_class;
}

const std::vector<SurvivorsZone>& ZonesAt(const SurvivorsField& field, std::size_t hero_index) {
  return hero_index == field.stage ? field.zones : KitAt(field, hero_index).zones;
}

void PutOnStage(SurvivorsField& field, std::size_t hero_index) {
  if (hero_index == field.stage) return;
  if (field.stage != 0) Exchange(field, field.allies[field.stage - 1U]);
  if (hero_index != 0) Exchange(field, field.allies[hero_index - 1U]);
  field.stage = static_cast<std::uint8_t>(hero_index);
}

void SyncTeam(SurvivorsField& field) {
  for (SurvivorsKit& kit : field.allies) {
    kit.hero.level = field.hero.level;
    kit.hero.exp = field.hero.exp;
    kit.hero.exp_boost_until_step = field.hero.exp_boost_until_step;
    kit.hero.zeny = field.hero.zeny;
  }
}

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

core::Result<SurvivorsRun> SurvivorsRun::Start(const SurvivorsRules& rules,
                                               const SurvivorsStart& start) {
  const ArenaRule* arena_rule = FindArena(rules, start.arena_id);
  if (arena_rule == nullptr) return Refuse("arène inconnue " + std::to_string(start.arena_id));
  const TerrainRule* terrain = FindTerrain(rules, arena_rule->terrain_id);
  if (arena_rule->terrain_id == 0 || terrain == nullptr) {
    return Refuse("l'arène " + std::to_string(arena_rule->arena_id) +
                  " lit une carte : elle ne naît pas sans elle");
  }
  return Start(rules, SurvivorsGround::Generated(*terrain, start.seed), start);
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
  if (start.allies.size() + 1 > kMostHeroes) {
    return Refuse(std::to_string(start.allies.size() + 1) + " héros, au plus " +
                  std::to_string(kMostHeroes));
  }
  for (const SurvivorsAllyStart& ally : start.allies) {
    const ClassRule* ally_class = FindClass(rules, ally.class_id);
    if (ally_class == nullptr) return Refuse("classe inconnue " + std::to_string(ally.class_id));
    if (std::find(ally_class->starter_skills.begin(), ally_class->starter_skills.end(),
                  ally.first_skill_id) == ally_class->starter_skills.end() ||
        FindSkill(rules, ally.first_skill_id) == nullptr) {
      return Refuse("premier skill hors des skills de départ " +
                    std::to_string(ally.first_skill_id));
    }
    if (!LoadoutWithinBounds(ally.loadout)) {
      return Refuse("bonus de loadout au-delà de ±" + std::to_string(kMostStatBonus));
    }
    if (core::Result<void> locked = CheckLockedIds(*ally_class, StartOfAlly(start, ally));
        !locked) {
      return std::unexpected(std::move(locked.error()));
    }
  }
  if (ground.bounded() &&
      (ground.width() <= 0 || ground.height() <= 0 || ground.width() > kMostArenaSideCells ||
       ground.height() > kMostArenaSideCells)) {
    return Refuse("arène vide ou trop grande : " + std::to_string(ground.width()) + " × " +
                  std::to_string(ground.height()));
  }
  if (ground.bounded() && arena_rule->terrain_id != 0) {
    return Refuse("l'arène " + std::to_string(arena_rule->arena_id) +
                  " est générée : elle ne se joue pas sur une carte");
  }
  // Le journal ne garde que le départ : un sol généré doit être celui que le
  // rejeu en redérive, le terrain de l'arène dans ces règles sous la graine.
  if (!ground.bounded() && (ground.rule() != FindTerrain(rules, arena_rule->terrain_id) ||
                            ground.run_seed() != start.seed)) {
    return Refuse("le sol généré n'est pas celui de l'arène " +
                  std::to_string(arena_rule->arena_id) + " sous la graine du départ");
  }
  const std::optional<CellPosition> start_cell = SurvivorsStartCell(rules, *arena_rule, ground);
  if (!start_cell) {
    return Refuse("aucune case praticable près du départ de l'arène " +
                  std::to_string(arena_rule->arena_id));
  }

  SurvivorsField field;
  field.rules = &rules;
  field.hero_class = hero_class;
  field.content = ContentOf(rules, *arena_rule);
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
  // LES ALLIÉS naissent autour du premier, chacun sur sa case ; les charges et les
  // feuilles de tous forment la réserve de l'équipe.
  if (!start.allies.empty()) {
    std::uint32_t rerolls = start.loadout.rerolls;
    std::uint32_t skips = start.loadout.skips;
    std::uint32_t banishes = start.loadout.banishes;
    std::uint32_t leaves = start.leaves;
    for (const SurvivorsAllyStart& ally : start.allies) {
      SurvivorsKit kit;
      kit.hero_class = FindClass(rules, ally.class_id);
      kit.lives_left = ally.lives;
      kit.start = ally;
      kit.hero.skills.push_back(OwnedSkill{.skill_id = ally.first_skill_id, .level = 1});
      std::optional<CellPosition> cell;
      for (std::int32_t ring = 1; ring <= kStartSearchRings * rules.tuning.spawn_search_ring;
           ++ring) {
        const auto around =
            static_cast<std::uint8_t>(field.allies.size() * kHeadingSteps / (kMostHeroes - 1));
        const FieldPoint wanted = field.hero.position + TurnVector(around, Cells(ring));
        cell = survivors::NearestPassable(field.ground, CellOf(wanted.x), CellOf(wanted.y),
                                          rules.tuning.spawn_search_ring);
        if (cell && !HeroStandsOn(field, CellCenter(cell->x, cell->y))) break;
        cell.reset();
      }
      if (!cell) cell = start_cell;
      kit.hero.position = CellCenter(cell->x, cell->y);
      kit.hero.previous_position = kit.hero.position;
      field.allies.push_back(std::move(kit));
      rerolls += ally.loadout.rerolls;
      skips += ally.loadout.skips;
      banishes += ally.loadout.banishes;
      leaves += ally.leaves;
    }
    field.rerolls_left =
        static_cast<std::uint8_t>(std::min<std::uint32_t>(rerolls, kMostTeamCharges));
    field.skips_left = static_cast<std::uint8_t>(std::min<std::uint32_t>(skips, kMostTeamCharges));
    field.banishes_left =
        static_cast<std::uint8_t>(std::min<std::uint32_t>(banishes, kMostTeamCharges));
    field.leaves_left = static_cast<std::uint8_t>(std::min<std::uint32_t>(leaves, 255U));
  }
  survivors::PlantField(field);
  survivors::SowStartPlants(field);
  survivors::ForEachHero(field, [&field] { survivors::RecomputeStats(field); });
  RefreshTally(field);
  return SurvivorsRun(std::move(field));
}

void SurvivorsRun::Step(SurvivorsInput input) {
  StepTeam(std::span<const SurvivorsInput>(&input, 1));
}

void SurvivorsRun::StepTeam(std::span<const SurvivorsInput> inputs) {
  if (field_.phase != SurvivorsPhase::kRunning) return;
  // ⚠ L'ORDRE DU PAS EST LA RÈGLE DU JEU : le changer change chaque partie
  // rejouée, et monte kSurvivorsRulesetVersion.
  survivors::ForEachStanding(field_, [&] {
    const SurvivorsInput input =
        field_.stage < inputs.size() ? inputs[field_.stage] : SurvivorsInput{};
    const FieldPoint from = field_.hero.position;
    survivors::MoveHero(field_, input);
    if (!field_.allies.empty()) HoldLeash(field_, from);
  });
  // Un héros à terre ne bouge plus : rien ne doit l'interpoler entre deux positions.
  survivors::ForEachHero(field_, [this] {
    if (field_.hero.down) field_.hero.previous_position = field_.hero.position;
  });
  // Un héros en scène sert de théâtre à ce qui ne dépend pas d'un héros : le
  // cadre d'apparition, la grille, les plantes qui naissent.
  PutAnchorOnStage(field_);
  // Les plantes laissées loin du héros qui vient de bouger se perdent.
  survivors::ForgetFarPlants(field_);
  survivors::RefreshFlow(field_);
  survivors::SpawnFoes(field_);
  survivors::SpawnEvents(field_);
  survivors::RunBossScripts(field_);
  survivors::SteerFoes(field_);
  survivors::MoveHorde(field_);
  // Les blocs d'un héros tombé retiennent la horde tant qu'ils tiennent.
  survivors::ForEachHero(field_, [this] { survivors::HoldBarriers(field_); });
  survivors::RebuildGrid(field_);
  // Un héros tombé laisse ses coups en vol finir leur course.
  survivors::ForEachHero(field_, [this] {
    const std::size_t casts_before = field_.news.casts.size();
    if (!field_.hero.down) survivors::FireSkills(field_);
    survivors::LandStrikes(field_);
    survivors::LandBlasts(field_);
    survivors::AdvanceShots(field_);
    survivors::AdvanceZones(field_);
    for (std::size_t cast = casts_before; cast < field_.news.casts.size(); ++cast) {
      field_.news.casts[cast].hero_index = field_.stage;
    }
  });
  survivors::BuryFoes(field_);
  // La chute du boss final clôt la partie dans BuryFoes : ni menace ni contact
  // ne blessent plus le héros, et une victoire l'emporte sur une chute du même
  // pas. Un héros tombé sous une menace ne prend plus de contact, ne ramasse
  // plus rien et ne se soigne plus.
  if (field_.phase == SurvivorsPhase::kRunning) survivors::AdvanceThreats(field_);
  if (field_.phase == SurvivorsPhase::kRunning) {
    survivors::ForEachStanding(field_, [this] { survivors::TouchHero(field_); });
  }
  if (field_.phase == SurvivorsPhase::kRunning) {
    survivors::ForEachStanding(field_, [this] {
      survivors::GatherGems(field_);
      survivors::GatherPickups(field_);
      survivors::Regenerate(field_);
    });
    // Les tués de tout le pas comptent, Holy Water comprise ; la plante
    // naît autour du héros à sa place de fin de pas.
    survivors::SowPlants(field_);
  }
  PutOnStage(field_, 0);
  if (!field_.allies.empty()) SyncTeam(field_);
  CloseStep(field_);
}

void SurvivorsRun::Tick(std::span<const SurvivorsTeamInput> inputs) {
  if (field_.phase == SurvivorsPhase::kOver) return;
  ++field_.ticks;
  if (field_.phase == SurvivorsPhase::kChoosing) {
    survivors::AdvanceVote(field_, inputs);
    return;
  }
  if (field_.hold_ticks_left > 0) {
    --field_.hold_ticks_left;
    return;
  }
  std::array<SurvivorsInput, kMostHeroes> headings{};
  for (std::size_t index = 0; index < std::min(inputs.size(), kMostHeroes); ++index) {
    headings[index].heading = inputs[index].heading;
  }
  StepTeam(headings);
}

void SurvivorsRun::MarkVoterGone(std::size_t player) {
  if (player >= field_.voter_gone.size()) return;
  field_.voter_gone[player] = true;
  field_.ballots[player].reset();
}

bool SurvivorsRun::BallotValid(SurvivorsAnswer ballot) const {
  return survivors::BallotAccepted(field_, ballot);
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
  if (field_.allies.empty()) {
    survivors::CollectOrbs(field_, out);
    return;
  }
  // Chaque héros debout passe en scène le temps de lire ses orbes, et celui qui y
  // était y revient avant de rendre la main.
  auto& staged = const_cast<SurvivorsField&>(field_);
  const std::size_t was_staged = staged.stage;
  for (std::size_t index = 0; index < HeroCount(field_); ++index) {
    PutOnStage(staged, index);
    if (!staged.hero.down) survivors::CollectOrbs(staged, out);
  }
  PutOnStage(staged, was_staged);
}

}  // namespace ro::sim
