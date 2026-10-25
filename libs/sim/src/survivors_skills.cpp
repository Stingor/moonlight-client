#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

#include "ro/sim/survivors_geometry.h"
#include "ro/sim/survivors_news.h"
#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_run.h"
#include "ro/sim/survivors_state.h"

#include "survivors_space.h"
#include "survivors_systems.h"
#include "survivors_volley.h"

namespace ro::sim::survivors {

namespace {

// ── Les frappes ────────────────────────────────────────────────────────────

[[nodiscard]] bool Covered(const SurvivorsFoe& foe) noexcept {
  return foe.pending_damage >= foe.hp;
}

// L'index du monstre que vise la frappe suivante. Un monstre déjà couvert par
// ses promesses n'est visé qu'une fois tous les autres couverts : les
// surnuméraires repassent alors sur l'ordre de visée, un par un.
[[nodiscard]] std::uint32_t NextTarget(SurvivorsField& field, TargetRule target,
                                       std::size_t& surplus_cursor) {
  const auto& order = field.nearby;
  const auto& foes = field.foes;
  if (target == TargetRule::kRandom) {
    const auto open = static_cast<std::uint32_t>(std::count_if(
        order.begin(), order.end(), [&](std::uint32_t index) { return !Covered(foes[index]); }));
    if (open == 0) {
      return order[field.combat_random.Below(static_cast<std::uint32_t>(order.size()))];
    }
    std::uint32_t skipped = field.combat_random.Below(open);
    for (const std::uint32_t index : order) {
      if (Covered(foes[index])) continue;
      if (skipped == 0) return index;
      --skipped;
    }
  }
  for (const std::uint32_t index : order) {
    if (!Covered(foes[index])) return index;
  }
  const std::uint32_t index = order[surplus_cursor % order.size()];
  ++surplus_cursor;
  return index;
}

// Une salve de frappes, chacune promise à son monstre dès l'émission : la
// frappe suivante ne vise pas un monstre que les promesses tuent déjà. Un
// monstre que l'élément du skill annule n'est pas candidat : sa promesse nulle
// ne le couvrirait jamais, et il prendrait toutes les frappes. Faux, sans rien
// émettre, quand aucun candidat n'est à portée ou que le plafond des frappes
// est atteint.
[[nodiscard]] bool FireStrikes(SurvivorsField& field, const SurvivorsSkillRule& rule,
                               const SkillLevel& level) {
  const std::size_t count = std::min<std::size_t>(
      AmountOf(field, level), RoomUnder(field.strikes.size(), field.rules->caps.most_strikes));
  if (count == 0) return false;
  GatherNearby(field, field.hero.position, level.reach_units);
  std::erase_if(field.nearby, [&](std::uint32_t index) {
    const SurvivorsFoe& foe = field.foes[index];
    return Covered(foe) || ElementPct(*field.rules, rule.element, ElementOf(field, foe)) <= 0;
  });
  if (field.nearby.empty()) return false;
  OrderTargets(field, rule.target, AreaUnits(field, level.area_radius_units));

  const std::uint32_t interval_steps = StepsOf(level.interval_ms);
  std::size_t surplus_cursor = 0;
  for (std::size_t strike = 0; strike < count; ++strike) {
    SurvivorsFoe& foe = field.foes[NextTarget(field, rule.target, surplus_cursor)];
    const SurvivorsDamage damage = DamageAgainst(field, level.damage, rule.element, foe);
    foe.pending_damage = SaturatedAdd(foe.pending_damage, damage.amount);
    const std::uint32_t land_step =
        field.step + 1 + static_cast<std::uint32_t>(strike) * interval_steps;
    field.strikes.push_back(SurvivorsStrike{.foe_serial = foe.serial,
                                            .land_step = land_step,
                                            .base_damage = level.damage,
                                            .promised = damage.amount,
                                            .skill_id = rule.skill_id,
                                            .element = rule.element,
                                            .chill_ms = level.chill_ms,
                                            .freeze_ms = level.freeze_ms,
                                            .knockback_units = level.knockback_units,
                                            .bounces = level.bounces,
                                            .bounce_pct = level.bounce_pct,
                                            .bounce_reach_units = level.bounce_reach_units});
    PublishCast(field.news, SurvivorsCast{.skill_id = rule.skill_id,
                                          .kind = CastKind::kStrike,
                                          .from = field.hero.position,
                                          .to = foe.position,
                                          .target_serial = foe.serial,
                                          .land_step = land_step});
  }
  return true;
}

// ── Les tirs chercheurs ────────────────────────────────────────────────────

// Une salve de tirs, sans promesse, RÉPARTIE : le k-ième tir vise le k-ième
// monstre le plus proche du héros à portée (distance, puis naissance), et la
// salve reprend au plus proche quand les monstres sont moins nombreux que les
// tirs. Faux, sans rien émettre, sans monstre à portée, plafond atteint, ou
// vitesse ou vie nulle.
[[nodiscard]] bool FireShots(SurvivorsField& field, std::uint8_t slot,
                             const SurvivorsSkillRule& rule, const SkillLevel& level) {
  const std::int32_t speed_per_step = ShotUnitsPerStep(level);
  if (speed_per_step <= 0 || level.life_ms == 0) return false;
  const std::size_t count = std::min<std::size_t>(
      AmountOf(field, level), RoomUnder(field.shots.size(), field.rules->caps.most_shots));
  if (count == 0) return false;

  const FieldPoint hero = field.hero.position;
  GatherNearby(field, hero, level.reach_units);
  if (field.nearby.empty()) return false;
  OrderTargets(field, TargetRule::kNearest, 0);
  const std::uint32_t expires_step =
      field.step + ScaledSteps(level.life_ms, field.hero.stats.Of(SurvivorsStat::kDurationPct));
  for (std::size_t shot = 0; shot < count; ++shot) {
    const SurvivorsFoe& target = field.foes[field.nearby[shot % field.nearby.size()]];
    field.shots.push_back(SurvivorsShot{.serial = field.next_serial++,
                                        .skill_slot = slot,
                                        .skill_id = rule.skill_id,
                                        .position = hero,
                                        .previous_position = hero,
                                        .velocity = Toward(hero, target.position, speed_per_step),
                                        .damage = level.damage,
                                        .hit_radius_units = level.hit_radius_units,
                                        .expires_step = expires_step,
                                        .pierce_left = level.pierce,
                                        .target_serial = target.serial,
                                        .hit_serials = {},
                                        .hit_count = 0,
                                        .freeze_ms = level.freeze_ms,
                                        .knockback_units = level.knockback_units,
                                        .straight = false});
    PublishCast(field.news, SurvivorsCast{.skill_id = rule.skill_id,
                                          .kind = CastKind::kShot,
                                          .from = hero,
                                          .to = target.position,
                                          .target_serial = target.serial,
                                          .land_step = 0});
  }
  return true;
}

// ── Les orbites ────────────────────────────────────────────────────────────

// Où en est une orbite à `step` : une phase active de life_ms, puis une phase
// éteinte de rest_ms (aucune si rest_ms est nul), comptées depuis phase_step.
struct OrbitPhase {
  bool active = false;
  bool turning_on = false;
  bool turning_off = false;
  std::uint64_t elapsed_steps = 0;
};

[[nodiscard]] OrbitPhase PhaseOf(const OwnedSkill& owned, const SkillLevel& level,
                                 std::uint32_t step) noexcept {
  OrbitPhase phase;
  const std::uint32_t active_steps = StepsOf(level.life_ms);
  if (active_steps == 0 || StepsOf(level.period_ms) == 0 || step < owned.phase_step) return phase;
  phase.elapsed_steps = step - owned.phase_step;
  const std::uint32_t rest_steps = StepsOf(level.rest_ms);
  if (rest_steps == 0) {
    phase.active = true;
    phase.turning_on = phase.elapsed_steps == 0;
    return phase;
  }
  const std::uint64_t in_cycle = phase.elapsed_steps % (std::uint64_t{active_steps} + rest_steps);
  phase.active = in_cycle < active_steps;
  phase.turning_on = in_cycle == 0;
  phase.turning_off = in_cycle == active_steps;
  return phase;
}

// L'orbe `orb` d'une orbite de `orb_count` orbes, à `elapsed_steps` pas de son
// origine : le cap tourne d'un tour par période, les orbes également espacées.
[[nodiscard]] FieldPoint OrbPosition(const SurvivorsField& field, const SkillLevel& level,
                                     std::uint64_t elapsed_steps, std::uint32_t orb,
                                     std::uint32_t orb_count) noexcept {
  const std::uint64_t turn = elapsed_steps * kHeadingSteps / StepsOf(level.period_ms) +
                             std::uint64_t{orb} * kHeadingSteps / orb_count;
  return field.hero.position + TurnVector(static_cast<std::uint8_t>(turn % kHeadingSteps),
                                          AreaUnits(field, level.reach_units));
}

// Les orbes allumées touchent chaque monstre qu'elles chevauchent, une fois par
// rehit_ms et par emplacement de skill ; un recul l'écarte du héros.
void SpinOrbit(SurvivorsField& field, std::uint8_t slot, const SurvivorsSkillRule& rule,
               const SkillLevel& level) {
  const OrbitPhase phase = PhaseOf(field.hero.skills[slot], level, field.step);
  if (phase.turning_on || phase.turning_off) {
    PublishCast(field.news,
                SurvivorsCast{.skill_id = rule.skill_id,
                              .kind = phase.turning_on ? CastKind::kOrbitOn : CastKind::kOrbitOff,
                              .from = field.hero.position,
                              .to = field.hero.position});
  }
  const std::uint32_t orb_count = AmountOf(field, level);
  if (!phase.active || orb_count == 0) return;
  const std::int32_t query_units = level.hit_radius_units + LargestFoeRadius(*field.rules);
  const std::uint32_t rehit_steps = StepsOf(level.rehit_ms);
  for (std::uint32_t orb = 0; orb < orb_count; ++orb) {
    const FieldPoint center = OrbPosition(field, level, phase.elapsed_steps, orb, orb_count);
    BreakPlantsWithin(field, center, center, level.hit_radius_units);
    GatherNearby(field, center, query_units);
    for (const std::uint32_t index : field.nearby) {
      SurvivorsFoe& foe = field.foes[index];
      if (foe.hp <= 0 || foe.rehit_ready_step[slot] > field.step) continue;
      const std::int64_t reach =
          std::int64_t{level.hit_radius_units} + FoeRadius(*field.rules, foe);
      if (DistanceSquared(center, foe.position) >= reach * reach) continue;
      const SurvivorsStatus status{
          .freeze_ms = level.freeze_ms,
          .knockback_units = level.knockback_units,
          .knock_heading = HeadingAway(field.hero.position, foe, field.hero.facing)};
      HurtFoe(field, foe, DamageAgainst(field, level.damage, rule.element, foe), rule.skill_id,
              status);
      foe.rehit_ready_step[slot] = field.step + rehit_steps;
    }
  }
}

// Ce qu'une frappe inflige à `foe` en plus des dégâts : le recul l'écarte du
// héros.
[[nodiscard]] SurvivorsStatus StrikeStatus(const SurvivorsField& field,
                                           const SurvivorsStrike& strike,
                                           const SurvivorsFoe& foe) noexcept {
  return SurvivorsStatus{.freeze_ms = strike.freeze_ms,
                         .knockback_units = strike.knockback_units,
                         .knock_heading = HeadingAway(field.hero.position, foe, field.hero.facing)};
}

// Un glaçage compte au récapitulatif quand il fait passer un monstre encore en
// vie de non ralenti à ralenti : la frappe qui prolonge un glaçage en cours ne
// recompte pas, et une salve de plusieurs frappes sur le même monstre ne compte
// qu'une fois.
void Chill(SurvivorsField& field, SurvivorsFoe& foe, std::uint32_t chill_ms) {
  if (chill_ms == 0) return;
  const bool was_chilled = foe.chill_until_step > field.step;
  if (foe.hp > 0 && !was_chilled &&
      field.recap.chills < std::numeric_limits<std::uint64_t>::max()) {
    ++field.recap.chills;
  }
  foe.chill_until_step = std::max(
      foe.chill_until_step,
      field.step + ScaledSteps(chill_ms, field.hero.stats.Of(SurvivorsStat::kDurationPct)));
}

// LES REBONDS d'une frappe tombée sur `first` : au même pas, chacun frappe le
// monstre vivant le plus proche du précédent (le premier né à égalité), à
// `bounce_reach_units` au plus, qu'aucun coup de la chaîne n'a encore touché,
// de `base × bounce_pct / 100` avant bonus et élément. Sans candidat, la
// chaîne s'arrête. Aucune promesse : le coup est immédiat.
void Bounce(SurvivorsField& field, const SurvivorsStrike& strike, const SurvivorsFoe& first) {
  if (strike.bounces == 0) return;
  std::array<std::uint32_t, kMostShotHits> struck = {};
  std::size_t struck_count = 0;
  struck[struck_count++] = first.serial;
  FieldPoint from = first.position;
  const auto base =
      static_cast<std::int32_t>(std::int64_t{std::max(strike.base_damage, 0)} *
                                std::clamp(strike.bounce_pct, 0, kMostStatBonus) / kPercent);
  const std::size_t bounces = std::min<std::size_t>(strike.bounces, kMostShotHits - 1);
  for (std::size_t bounce = 0; bounce < bounces; ++bounce) {
    GatherNearby(field, from, strike.bounce_reach_units);
    SurvivorsFoe* next = nullptr;
    std::int64_t next_distance = std::numeric_limits<std::int64_t>::max();
    const auto struck_end = struck.begin() + static_cast<std::ptrdiff_t>(struck_count);
    for (const std::uint32_t index : field.nearby) {
      SurvivorsFoe& foe = field.foes[index];
      if (std::find(struck.begin(), struck_end, foe.serial) != struck_end) continue;
      const std::int64_t distance = DistanceSquared(foe.position, from);
      if (distance < next_distance) {
        next = &foe;
        next_distance = distance;
      }
    }
    if (next == nullptr) return;
    PublishCast(field.news, SurvivorsCast{.skill_id = strike.skill_id,
                                          .kind = CastKind::kStrike,
                                          .from = from,
                                          .to = next->position,
                                          .target_serial = next->serial,
                                          .land_step = field.step});
    HurtFoe(field, *next, DamageAgainst(field, base, strike.element, *next), strike.skill_id,
            StrikeStatus(field, strike, *next));
    Chill(field, *next, strike.chill_ms);
    struck[struck_count++] = next->serial;
    from = next->position;
  }
}

// La recharge d'une salve, raccourcie par la statistique jusqu'au plafond des
// réglages.
[[nodiscard]] std::uint32_t ReadyAfter(const SurvivorsField& field,
                                       const SkillLevel& level) noexcept {
  const std::int64_t cut_pct = std::min<std::int64_t>(
      field.hero.stats.Of(SurvivorsStat::kCooldownPct), field.rules->tuning.most_cooldown_pct);
  return field.step + ScaledSteps(level.cooldown_ms, -cut_pct);
}

}  // namespace

void FireSkills(SurvivorsField& field) {
  if (field.rules == nullptr) return;
  const std::size_t slots = std::min(field.hero.skills.size(), kMostSkillSlots);
  for (std::size_t index = 0; index < slots; ++index) {
    const auto slot = static_cast<std::uint8_t>(index);
    const OwnedSkill& owned = field.hero.skills[index];
    const SurvivorsSkillRule* rule = FindSkill(*field.rules, owned.skill_id);
    if (rule == nullptr) continue;
    const SkillLevel* level = LevelOf(*rule, owned);
    if (level == nullptr || level->amount == 0) continue;
    // Une orbite tourne selon sa phase ; ni salve ni recharge.
    if (rule->archetype == SkillArchetype::kOrbit) {
      SpinOrbit(field, slot, *rule, *level);
      continue;
    }
    if (field.step < owned.ready_step) continue;
    bool fired = false;
    switch (rule->archetype) {
      case SkillArchetype::kHoming:
        fired = FireShots(field, slot, *rule, *level);
        break;
      case SkillArchetype::kLinear:
        fired = FireLinear(field, slot, *rule, *level);
        break;
      case SkillArchetype::kClusterBlast:
        fired = FireClusterBlast(field, *rule, *level);
        break;
      case SkillArchetype::kNova:
        fired = FireNova(field, *rule, *level);
        break;
      case SkillArchetype::kWall:
        fired = FireWall(field, slot, *rule, *level);
        break;
      case SkillArchetype::kRing:
        fired = FireRing(field, slot, *rule, *level);
        break;
      case SkillArchetype::kTargetStrike:
        fired = FireStrikes(field, *rule, *level);
        break;
      case SkillArchetype::kOrbit:  // tournée plus haut
        break;
    }
    // Sans salve, le skill attend : il tire au premier pas où il le peut.
    if (fired) field.hero.skills[index].ready_step = ReadyAfter(field, *level);
  }
}

void LandStrikes(SurvivorsField& field) {
  if (field.rules == nullptr) return;
  // Par index : un rebond ne touche pas la liste des frappes.
  for (std::size_t index = 0; index < field.strikes.size(); ++index) {
    const SurvivorsStrike strike = field.strikes[index];
    if (strike.land_step > field.step) continue;
    SurvivorsFoe* foe = FoeBySerial(field, strike.foe_serial);
    // Un monstre parti : la frappe se perd.
    if (foe == nullptr) continue;
    foe->pending_damage = std::max(foe->pending_damage - strike.promised, 0);
    // Un monstre déjà mort ce pas, ou caché depuis le lancer : la frappe se
    // perd aussi.
    if (!Tangible(field, *foe)) continue;
    HurtFoe(field, *foe, DamageAgainst(field, strike.base_damage, strike.element, *foe),
            strike.skill_id, StrikeStatus(field, strike, *foe));
    Chill(field, *foe, strike.chill_ms);
    Bounce(field, strike, *foe);
  }
  std::erase_if(field.strikes,
                [&](const SurvivorsStrike& strike) { return strike.land_step <= field.step; });
}

SurvivorsDamage DamageAgainst(const SurvivorsField& field, std::int32_t base,
                              SurvivorsElement attack, const SurvivorsFoe& foe) {
  if (field.rules == nullptr) return {};
  const std::int32_t efficiency_pct = ElementPct(*field.rules, attack, ElementOf(field, foe));
  std::int64_t level_bonus_pct = 0;
  if (field.hero_class != nullptr) {
    level_bonus_pct = std::min<std::int64_t>(
        std::int64_t{field.hero.level} * field.hero_class->damage_pct_per_level,
        field.hero_class->damage_pct_cap);
  }
  const std::int64_t bonus_pct = field.hero.stats.Of(SurvivorsStat::kDamagePct) + level_bonus_pct;
  // Ramené à l'int32 avant l'élément : le produit reste loin des 64 bits.
  std::int64_t amount =
      std::int64_t{ScaledUnits(std::max(base, 0), bonus_pct)} * efficiency_pct / kPercent;
  if (efficiency_pct > 0) amount = std::max<std::int64_t>(amount, 1);
  return SurvivorsDamage{.amount = ClampedUnits(amount), .efficiency_pct = efficiency_pct};
}

void HurtFoe(SurvivorsField& field, SurvivorsFoe& foe, SurvivorsDamage damage,
             std::uint16_t skill_id, const SurvivorsStatus& status) {
  foe.hp -= damage.amount;
  foe.hurt_step = field.step;
  const auto dealt = static_cast<std::uint64_t>(std::max(damage.amount, 0));
  field.tally.damage_dealt += dealt;
  CountInRecap(field.recap.damage_by_skill, skill_id, dealt);
  const StatusApplied applied = ApplyStatus(field, foe, status);
  if (field.news.hits.size() < kMostNewsHits) {
    field.news.hits.push_back(SurvivorsHit{.foe_serial = foe.serial,
                                           .at = foe.position,
                                           .amount = damage.amount,
                                           .efficiency_pct = damage.efficiency_pct,
                                           .skill_id = skill_id,
                                           .froze = applied.froze,
                                           .knocked = applied.knocked});
  } else {
    ++field.news.dropped;
  }
}

void CollectOrbs(const SurvivorsField& field, std::vector<SurvivorsOrb>& out) {
  if (field.rules == nullptr) return;
  // Le dernier pas exécuté : `field.step` a déjà été compté par CloseStep.
  const std::uint32_t drawn_step = field.step > 0 ? field.step - 1 : 0;
  for (const OwnedSkill& owned : field.hero.skills) {
    const SurvivorsSkillRule* rule = FindSkill(*field.rules, owned.skill_id);
    if (rule == nullptr || rule->archetype != SkillArchetype::kOrbit) continue;
    const SkillLevel* level = LevelOf(*rule, owned);
    if (level == nullptr || level->amount == 0) continue;
    const OrbitPhase phase = PhaseOf(owned, *level, drawn_step);
    const std::uint32_t orb_count = AmountOf(field, *level);
    if (!phase.active) continue;
    for (std::uint32_t orb = 0; orb < orb_count; ++orb) {
      out.push_back(SurvivorsOrb{
          .skill_id = rule->skill_id,
          .position = OrbPosition(field, *level, phase.elapsed_steps, orb, orb_count)});
    }
  }
}

}  // namespace ro::sim::survivors
