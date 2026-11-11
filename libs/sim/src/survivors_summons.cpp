// LES INVOCATIONS ET LES SOINS : des alliés posés près du héros, qui frappent
// ou soignent pour lui, et le soin du héros le plus blessé.
//
// 🔴 UN ALLIÉ N'EST PAS UN CORPS DE LA PARTIE : la horde ne le voit, ne le
// vise ni ne le bloque, et il traverse le décor. Il ne tire du hasard que pour
// un ciblage au hasard, et ses coups passent par HurtFoe comme ceux du héros :
// critiques, vol de vie et dégâts à distance compris.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "ro/sim/survivors_geometry.h"
#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_run.h"

#include "survivors_space.h"
#include "survivors_systems.h"
#include "survivors_volley.h"

namespace ro::sim::survivors {

namespace {

// Un allié naît à cette distance du héros, les suivants répartis autour de
// lui.
constexpr std::int32_t kSummonPoseCells = 1;
// Sans monstre dans sa laisse, un allié qui marche revient à cette distance
// du héros.
constexpr std::int32_t kSummonHeelCells = 2;
// Le héros qui vient d'échanger sa place avec un allié ne prend aucun coup ce
// temps.
constexpr std::uint32_t kSwapVeilMs = 600;
// La part des PV d'un héros, en millièmes, qui départage les blessés.
constexpr std::int64_t kHealthPerMille = 1000;

// Le héros `hero_index`, en écriture.
[[nodiscard]] SurvivorsHero& HeroRef(SurvivorsField& field, std::size_t hero_index) {
  if (hero_index == field.stage) return field.hero;
  return field.allies[(hero_index == 0 ? field.stage : hero_index) - 1U].hero;
}

// Les alliés de toute l'équipe.
[[nodiscard]] std::size_t TeamSummons(const SurvivorsField& field) {
  std::size_t count = 0;
  for (std::size_t index = 0; index < HeroCount(field); ++index) {
    count += SummonsAt(field, index).size();
  }
  return count;
}

// LE HÉROS DEBOUT LE PLUS BLESSÉ à `reach_units` de `from` : la plus petite part
// de PV, le plus petit index à égalité. Rien quand aucun n'est blessé.
[[nodiscard]] std::optional<std::size_t> MostWounded(const SurvivorsField& field, FieldPoint from,
                                                     std::int32_t reach_units) {
  std::optional<std::size_t> wounded;
  std::int64_t least_per_mille = 0;
  const std::int64_t reach = reach_units;
  for (std::size_t index = 0; index < HeroCount(field); ++index) {
    const SurvivorsHero& hero = HeroAt(field, index);
    if (hero.down || hero.max_hp <= 0 || hero.hp >= hero.max_hp) continue;
    if (DistanceSquared(hero.position, from) > reach * reach) continue;
    const std::int64_t per_mille = std::int64_t{hero.hp} * kHealthPerMille / hero.max_hp;
    if (!wounded || per_mille < least_per_mille) {
      wounded = index;
      least_per_mille = per_mille;
    }
  }
  return wounded;
}

// Rend `heal_pct` % de ses PV max au héros `hero_index`, au moins un PV.
void Heal(SurvivorsField& field, std::size_t hero_index, std::int32_t heal_pct) {
  SurvivorsHero& hero = HeroRef(field, hero_index);
  const std::int64_t amount =
      std::max<std::int64_t>(std::int64_t{hero.max_hp} * heal_pct / kPercent, 1);
  hero.hp = static_cast<std::int32_t>(std::min<std::int64_t>(hero.max_hp, hero.hp + amount));
}

// Vrai si l'allié se tient sur une case où le héros peut se poser.
[[nodiscard]] bool StandsOnPassable(const SurvivorsField& field, FieldPoint at) {
  return field.ground.Passable(CellOf(at.x), CellOf(at.y));
}

// LA CIBLE D'UN ALLIÉ : un monstre vivant et visible, à portée de l'allié qui
// ne marche pas, dans la laisse du héros pour celui qui marche. Le plus proche
// de l'allié, le premier né à égalité. Au hasard, par un Below du flux du
// combat sur les candidats, pour un skill qui vise au hasard et un allié prêt à
// frapper (`ready`) : seul le coup tire, jamais la marche.
[[nodiscard]] SurvivorsFoe* TargetOf(SurvivorsField& field, const SurvivorsSummon& summon,
                                     const SurvivorsSkillRule& rule, const SkillLevel& level,
                                     bool ready) {
  const bool walks = level.speed_units_per_s > 0;
  const FieldPoint center = walks ? field.hero.position : summon.position;
  const std::int32_t radius = walks ? level.leash_units : level.reach_units;
  GatherNearby(field, center, radius + LargestFoeRadius(*field.rules));
  std::vector<std::uint32_t>& candidates = field.around;
  candidates.clear();
  for (const std::uint32_t index : field.nearby) {
    const SurvivorsFoe& foe = field.foes[index];
    if (foe.hp <= 0 || !Tangible(field, foe)) continue;
    const std::int64_t reach = std::int64_t{radius} + FoeRadius(*field.rules, foe);
    if (DistanceSquared(center, foe.position) >= reach * reach) continue;
    candidates.push_back(index);
  }
  if (candidates.empty()) return nullptr;
  if (rule.target == TargetRule::kRandom && ready) {
    const auto pick = field.combat_random.Below(static_cast<std::uint32_t>(candidates.size()));
    return &field.foes[candidates[pick]];
  }
  std::uint32_t best = candidates.front();
  for (const std::uint32_t index : candidates) {
    const std::int64_t squared = DistanceSquared(field.foes[index].position, summon.position);
    const std::int64_t best_squared = DistanceSquared(field.foes[best].position, summon.position);
    if (squared < best_squared ||
        (squared == best_squared && field.foes[index].serial < field.foes[best].serial)) {
      best = index;
    }
  }
  return &field.foes[best];
}

// L'allié qui marche avance vers sa cible tant qu'elle est hors de sa portée,
// sinon revient près du héros.
void Walk(SurvivorsField& field, SurvivorsSummon& summon, const SkillLevel& level,
          const SurvivorsFoe* target) {
  const std::int32_t pace = level.speed_units_per_s / static_cast<std::int32_t>(kStepsPerSecond);
  if (pace <= 0) return;
  FieldPoint goal = summon.position;
  if (target != nullptr) {
    const std::int64_t reach = std::int64_t{level.reach_units} + FoeRadius(*field.rules, *target);
    if (DistanceSquared(summon.position, target->position) >= reach * reach) {
      goal = target->position;
    }
  } else {
    const std::int64_t heel = Cells(kSummonHeelCells);
    if (DistanceSquared(summon.position, field.hero.position) > heel * heel) {
      goal = field.hero.position;
    }
  }
  if (goal == summon.position) return;
  summon.facing = HeadingOf(goal - summon.position, summon.facing);
  summon.position = summon.position + StepToward(summon.position, goal, pace);
}

// L'ÉCHANGE DE PLACE : le héros frappé sous le seuil de l'allié prend sa place,
// s'il peut s'y tenir, et l'allié la sienne ; le héros ne prend aucun coup un
// court instant.
void Swap(SurvivorsField& field, SurvivorsSummon& summon, const SkillLevel& level) {
  SurvivorsHero& hero = field.hero;
  if (level.reflex_below_hp_pct <= 0 || field.step < summon.swap_ready_step) return;
  if (std::int64_t{hero.hp} * kPercent >= std::int64_t{hero.max_hp} * level.reflex_below_hp_pct) {
    return;
  }
  if (!StandsOnPassable(field, summon.position)) return;
  PublishCast(field.news, SurvivorsCast{.skill_id = summon.skill_id,
                                        .kind = CastKind::kSummonSwap,
                                        .from = hero.position,
                                        .to = summon.position,
                                        .summon_serial = summon.serial});
  std::swap(hero.position, summon.position);
  hero.previous_position = hero.position;
  summon.previous_position = summon.position;
  hero.invulnerable_until_step =
      std::max(hero.invulnerable_until_step, field.step + 1 + StepsOf(kSwapVeilMs));
  summon.swap_ready_step = ReadyAfter(field, level);
}

// Le coup de l'allié sur `foe`, et son éclat autour de lui.
void Strike(SurvivorsField& field, SurvivorsSummon& summon, const SurvivorsSkillRule& rule,
            const SkillLevel& level, SurvivorsFoe& foe) {
  const std::int32_t base =
      ScaledUnits(level.damage, field.hero.stats.Of(SurvivorsStat::kSummonDamagePct));
  summon.facing = HeadingOf(foe.position - summon.position, summon.facing);
  PublishCast(field.news, SurvivorsCast{.skill_id = rule.skill_id,
                                        .kind = CastKind::kSummonStrike,
                                        .from = summon.position,
                                        .to = foe.position,
                                        .target_serial = foe.serial,
                                        .land_step = field.step,
                                        .summon_serial = summon.serial});
  const FieldPoint center = foe.position;
  const std::uint32_t serial = foe.serial;
  const SurvivorsStatus status{.freeze_ms = level.freeze_ms,
                               .knockback_units = level.knockback_units,
                               .knock_heading = HeadingAway(summon.position, foe, summon.facing),
                               .poison_ms = level.poison_ms,
                               .poison_tick_damage = level.poison_tick_damage};
  HurtFoe(field, foe, DamageAgainst(field, base, rule.element, foe), rule.skill_id, status);
  if (level.area_radius_units > 0 && level.splash_pct > 0) {
    SplashAround(field, SurvivorsSplash{.center = center,
                                        .spared_serial = serial,
                                        .damage = base,
                                        .radius_units = AreaUnits(field, level.area_radius_units),
                                        .splash_pct = level.splash_pct,
                                        .skill_id = rule.skill_id,
                                        .element = rule.element});
  }
}

// Le soin d'un allié qui soigne : le plus blessé à sa portée.
[[nodiscard]] bool Mend(SurvivorsField& field, const SurvivorsSummon& summon,
                        const SkillLevel& level) {
  if (level.heal_pct <= 0) return false;
  const std::optional<std::size_t> wounded = MostWounded(field, summon.position, level.reach_units);
  if (!wounded) return false;
  Heal(field, *wounded, level.heal_pct);
  PublishCast(field.news, SurvivorsCast{.skill_id = summon.skill_id,
                                        .kind = CastKind::kMend,
                                        .from = summon.position,
                                        .to = HeroAt(field, *wounded).position,
                                        .summon_serial = summon.serial});
  return true;
}

// Vrai si l'allié a perdu son skill ou sa vie : il part.
[[nodiscard]] bool Dismissed(const SurvivorsField& field, const SurvivorsSummon& summon) {
  const std::vector<OwnedSkill>& skills = field.hero.skills;
  if (summon.skill_slot >= skills.size() || skills[summon.skill_slot].skill_id != summon.skill_id) {
    return true;
  }
  return summon.expires_step != 0 && field.step >= summon.expires_step;
}

}  // namespace

bool FireSummon(SurvivorsField& field, std::uint8_t slot, const SurvivorsSkillRule& rule,
                const SkillLevel& level) {
  // Un allié d'un groupe exclusif (l'homoncule) reste seul : le bonus de
  // quantité ne le double pas.
  const std::uint32_t wanted = rule.exclusive_group != 0 ? level.amount : AmountOf(field, level);
  const auto live = static_cast<std::uint32_t>(
      std::ranges::count_if(field.summons, [&](const SurvivorsSummon& summon) {
        return summon.skill_slot == slot && summon.skill_id == rule.skill_id;
      }));
  if (wanted == 0 || live >= wanted) return false;
  if (TeamSummons(field) >= field.rules->caps.most_summons) return false;
  // Chacun à sa place autour du héros, à partir de son cap.
  const auto heading = static_cast<std::uint8_t>(
      (field.hero.facing + live * kHeadingSteps / wanted) % kHeadingSteps);
  FieldPoint at = field.hero.position + TurnVector(heading, Cells(kSummonPoseCells));
  if (!StandsOnPassable(field, at)) at = field.hero.position;
  SurvivorsSummon summon{.serial = field.next_serial++,
                         .skill_slot = slot,
                         .skill_id = rule.skill_id,
                         .summon_class = level.summon_class,
                         .position = at,
                         .previous_position = at,
                         .facing = field.hero.facing,
                         .ready_step = field.step,
                         .swap_ready_step = field.step};
  if (level.life_ms > 0) {
    summon.expires_step =
        field.step +
        std::max<std::uint32_t>(
            ScaledSteps(level.life_ms, field.hero.stats.Of(SurvivorsStat::kDurationPct)), 1);
  }
  PublishCast(field.news, SurvivorsCast{.skill_id = rule.skill_id,
                                        .kind = CastKind::kSummonOn,
                                        .from = at,
                                        .to = at,
                                        .summon_serial = summon.serial});
  field.summons.push_back(summon);
  return true;
}

bool FireMend(SurvivorsField& field, const SurvivorsSkillRule& rule, const SkillLevel& level) {
  const std::optional<std::size_t> wounded =
      MostWounded(field, field.hero.position, level.reach_units);
  if (!wounded) return false;
  Heal(field, *wounded, level.heal_pct);
  PublishCast(field.news, SurvivorsCast{.skill_id = rule.skill_id,
                                        .kind = CastKind::kMend,
                                        .from = field.hero.position,
                                        .to = HeroAt(field, *wounded).position,
                                        .target_serial = 0,
                                        .land_step = field.step});
  return true;
}

void AdvanceSummons(SurvivorsField& field) {
  if (field.rules == nullptr || field.summons.empty()) return;
  std::erase_if(field.summons, [&](const SurvivorsSummon& summon) {
    if (!Dismissed(field, summon)) return false;
    PublishCast(field.news, SurvivorsCast{.skill_id = summon.skill_id,
                                          .kind = CastKind::kSummonOff,
                                          .from = summon.position,
                                          .to = summon.position,
                                          .summon_serial = summon.serial});
    return true;
  });
  // Par index : un coup peut faire tomber un monstre, jamais un allié.
  for (std::size_t index = 0; index < field.summons.size(); ++index) {
    SurvivorsSummon& summon = field.summons[index];
    summon.previous_position = summon.position;
    // Le héros à terre : ses alliés attendent qu'il se relève.
    if (field.hero.down) continue;
    const SurvivorsSkillRule* rule = FindSkill(*field.rules, summon.skill_id);
    if (rule == nullptr) continue;
    const SkillLevel* level = LevelOf(*rule, field.hero.skills[summon.skill_slot]);
    if (level == nullptr) continue;
    Swap(field, summon, *level);
    const bool ready = field.step >= summon.ready_step;
    SurvivorsFoe* target = TargetOf(field, summon, *rule, *level, ready && level->damage > 0);
    if (level->speed_units_per_s > 0) Walk(field, summon, *level, target);
    if (!ready) continue;
    bool acted = Mend(field, summon, *level);
    if (!acted && level->damage > 0 && target != nullptr) {
      const std::int64_t reach =
          std::int64_t{level->reach_units} + FoeRadius(*field.rules, *target);
      if (DistanceSquared(summon.position, target->position) < reach * reach) {
        Strike(field, summon, *rule, *level, *target);
        acted = true;
      }
    }
    if (acted)
      summon.ready_step = field.step + std::max<std::uint32_t>(StepsOf(level->interval_ms), 1);
  }
}

}  // namespace ro::sim::survivors
