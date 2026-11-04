#include <algorithm>
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

// LES ÉCLATS : une aire qui tombe sur un point, un pas au moins après son
// lancer. Un éclat de skill vise un monstre et frappe plus fort son centre ;
// une nova est posée sur le héros et frappe tout son disque à plein. Aucun ne
// promet de dégâts : une aire ne se répartit pas entre ses monstres, et la
// visée des frappes n'en tient pas compte.

namespace {

// La portée de requête d'un éclat : son rayon, plus le plus grand monstre.
[[nodiscard]] std::int32_t QueryUnits(const SurvivorsField& field,
                                      std::int32_t radius_units) noexcept {
  return ClampedUnits(std::int64_t{radius_units} + LargestFoeRadius(*field.rules));
}

// Vrai si `foe` chevauche le disque de `radius_units` autour de `center`.
[[nodiscard]] bool Overlaps(const SurvivorsField& field, const SurvivorsFoe& foe, FieldPoint center,
                            std::int32_t radius_units) noexcept {
  const std::int64_t reach = std::int64_t{radius_units} + FoeRadius(*field.rules, foe);
  return DistanceSquared(foe.position, center) < reach * reach;
}

// La chute d'un éclat : chaque monstre vivant qu'il chevauche, par index
// croissant, prend `damage × pct / 100` avant bonus et élément — 100 % pour le
// monstre visé s'il vit encore, `splash_pct` pour les autres, aucun coup à
// 0 % — puis le gel, et un recul qui l'écarte du centre.
void LandBlast(SurvivorsField& field, const SurvivorsBlast& blast) {
  BreakPlantsWithin(field, blast.center, blast.center, blast.radius_units);
  GatherNearby(field, blast.center, QueryUnits(field, blast.radius_units));
  for (const std::uint32_t index : field.nearby) {
    SurvivorsFoe& foe = field.foes[index];
    if (foe.hp <= 0 || !Overlaps(field, foe, blast.center, blast.radius_units)) continue;
    const bool center = blast.center_serial != 0 && foe.serial == blast.center_serial;
    const std::int64_t pct = center ? kPercent : std::clamp(blast.splash_pct, 0, kMostStatBonus);
    if (pct == 0) continue;
    const auto base = static_cast<std::int32_t>(
        std::min<std::int64_t>(std::int64_t{std::max(blast.damage, 0)} * pct / kPercent,
                               std::numeric_limits<std::int32_t>::max()));
    const SurvivorsStatus status{
        .freeze_ms = blast.freeze_ms,
        .knockback_units = blast.knockback_units,
        .knock_heading = HeadingAway(blast.center, foe, field.hero.facing)};
    HurtFoe(field, foe, DamageAgainst(field, base, blast.element, foe), blast.skill_id, status);
  }
}

}  // namespace

// L'ÉCLAT VISÉ : les monstres à portée du héros, rangés par la règle de visée
// une fois pour la salve ; le i-ième éclat se pose sur le i-ième, à la
// position qu'il a au lancer, et la salve reprend au premier quand ils sont
// moins nombreux que les éclats. Il tombe à `step + 1 + i × intervalle`.
bool FireClusterBlast(SurvivorsField& field, const SurvivorsSkillRule& rule,
                      const SkillLevel& level) {
  const std::size_t count = std::min<std::size_t>(
      AmountOf(field, level), RoomUnder(field.blasts.size(), field.rules->caps.most_blasts));
  if (count == 0) return false;
  GatherNearby(field, field.hero.position, level.reach_units);
  if (field.nearby.empty()) return false;
  const std::int32_t radius = AreaUnits(field, level.area_radius_units);
  if (radius <= 0) return false;
  OrderTargets(field, rule.target, radius);

  const std::uint32_t interval_steps = StepsOf(level.interval_ms);
  for (std::size_t blast = 0; blast < count; ++blast) {
    const SurvivorsFoe& target = field.foes[field.nearby[blast % field.nearby.size()]];
    const std::uint32_t land_step =
        field.step + 1 + static_cast<std::uint32_t>(blast) * interval_steps;
    field.blasts.push_back(SurvivorsBlast{.center = target.position,
                                          .land_step = land_step,
                                          .damage = level.damage,
                                          .radius_units = radius,
                                          .splash_pct = level.splash_pct,
                                          .knockback_units = level.knockback_units,
                                          .freeze_ms = level.freeze_ms,
                                          .center_serial = target.serial,
                                          .skill_id = rule.skill_id,
                                          .element = rule.element});
    PublishCast(field.news, SurvivorsCast{.skill_id = rule.skill_id,
                                          .kind = CastKind::kBlast,
                                          .from = field.hero.position,
                                          .to = target.position,
                                          .target_serial = target.serial,
                                          .land_step = land_step});
  }
  return true;
}

// LA NOVA : un éclat sans centre, posé sur le héros au pas du lancer, qui
// frappe tout son disque à plein ; la i-ième de la salve tombe à `step + 1 +
// i × intervalle`, au même point. Elle attend qu'un monstre soit dans l'aire.
bool FireNova(SurvivorsField& field, const SurvivorsSkillRule& rule, const SkillLevel& level) {
  const std::size_t count = std::min<std::size_t>(
      AmountOf(field, level), RoomUnder(field.blasts.size(), field.rules->caps.most_blasts));
  if (count == 0) return false;
  const std::int32_t radius = AreaUnits(field, level.area_radius_units);
  if (radius <= 0) return false;
  const FieldPoint hero = field.hero.position;
  GatherNearby(field, hero, QueryUnits(field, radius));
  if (std::none_of(field.nearby.begin(), field.nearby.end(), [&](std::uint32_t index) {
        return Overlaps(field, field.foes[index], hero, radius);
      })) {
    return false;
  }
  const std::uint32_t interval_steps = StepsOf(level.interval_ms);
  for (std::size_t nova = 0; nova < count; ++nova) {
    const std::uint32_t land_step =
        field.step + 1 + static_cast<std::uint32_t>(nova) * interval_steps;
    field.blasts.push_back(SurvivorsBlast{.center = hero,
                                          .land_step = land_step,
                                          .damage = level.damage,
                                          .radius_units = radius,
                                          .splash_pct = static_cast<std::int32_t>(kPercent),
                                          .knockback_units = level.knockback_units,
                                          .freeze_ms = level.freeze_ms,
                                          .center_serial = 0,
                                          .skill_id = rule.skill_id,
                                          .element = rule.element});
    PublishCast(field.news, SurvivorsCast{.skill_id = rule.skill_id,
                                          .kind = CastKind::kNova,
                                          .from = hero,
                                          .to = hero,
                                          .target_serial = 0,
                                          .land_step = land_step});
  }
  return true;
}

// LE CÔNE : devant le héros, dans son cap, long de `reach_units` majoré de
// l'aire du héros et ouvert de `cone_half_steps` caps de part et d'autre.
// Chaque monstre vivant dont le centre y tombe (à la portée près de son
// rayon), par index croissant, prend son coup au pas du lancer, et un recul
// qui l'écarte du héros. Il casse les plantes le long de son axe. Il attend
// un monstre dans le cône.
bool FireCone(SurvivorsField& field, const SurvivorsSkillRule& rule, const SkillLevel& level) {
  const std::int32_t length = AreaUnits(field, level.reach_units);
  if (length <= 0 || level.cone_half_steps == 0) return false;
  const FieldPoint hero = field.hero.position;
  const std::uint8_t facing = field.hero.facing;
  GatherNearby(field, hero, QueryUnits(field, length));
  std::erase_if(field.nearby, [&](std::uint32_t index) {
    const SurvivorsFoe& foe = field.foes[index];
    if (foe.hp <= 0 || !Overlaps(field, foe, hero, length)) return true;
    const auto turn = static_cast<std::uint32_t>(
        (std::uint32_t{HeadingOf(foe.position - hero, facing)} + kHeadingSteps - facing) %
        kHeadingSteps);
    return std::min(turn, kHeadingSteps - turn) > level.cone_half_steps;
  });
  if (field.nearby.empty()) return false;
  const FieldPoint tip = hero + TurnVector(facing, length);
  // Les plantes : celles de l'axe, à la demi-largeur du cône à mi-longueur.
  const std::int32_t half_width =
      ClampedUnits(IntegerSqrt(static_cast<std::uint64_t>(DistanceSquared(
          TurnVector(facing, length / 2),
          TurnVector(static_cast<std::uint8_t>((facing + level.cone_half_steps) % kHeadingSteps),
                     length / 2)))));
  BreakPlantsWithin(field, hero, tip, half_width);
  for (const std::uint32_t index : field.nearby) {
    SurvivorsFoe& foe = field.foes[index];
    const SurvivorsStatus status{.freeze_ms = level.freeze_ms,
                                 .knockback_units = level.knockback_units,
                                 .knock_heading = HeadingAway(hero, foe, facing)};
    HurtFoe(field, foe, DamageAgainst(field, level.damage, rule.element, foe), rule.skill_id,
            status);
  }
  PublishCast(field.news, SurvivorsCast{.skill_id = rule.skill_id,
                                        .kind = CastKind::kConeOn,
                                        .from = hero,
                                        .to = tip,
                                        .target_serial = 0,
                                        .land_step = 0,
                                        .spread_steps = level.cone_half_steps});
  return true;
}

void SplashAround(SurvivorsField& field, const SurvivorsSplash& splash) {
  if (splash.radius_units <= 0) return;
  const std::int64_t pct = std::clamp(splash.splash_pct, 0, kMostStatBonus);
  if (pct == 0) return;
  const auto base = static_cast<std::int32_t>(
      std::min<std::int64_t>(std::int64_t{std::max(splash.damage, 0)} * pct / kPercent,
                             std::numeric_limits<std::int32_t>::max()));
  GatherNearby(field, splash.center, QueryUnits(field, splash.radius_units));
  for (const std::uint32_t index : field.nearby) {
    SurvivorsFoe& foe = field.foes[index];
    if (foe.hp <= 0 || foe.serial == splash.spared_serial ||
        !Overlaps(field, foe, splash.center, splash.radius_units)) {
      continue;
    }
    HurtFoe(field, foe, DamageAgainst(field, base, splash.element, foe), splash.skill_id);
  }
}

void LandBlasts(SurvivorsField& field) {
  if (field.rules == nullptr) return;
  // Par index, dans l'ordre d'émission : une chute n'ajoute aucun éclat.
  for (std::size_t index = 0; index < field.blasts.size(); ++index) {
    const SurvivorsBlast blast = field.blasts[index];
    if (blast.land_step <= field.step) LandBlast(field, blast);
  }
  std::erase_if(field.blasts,
                [&](const SurvivorsBlast& blast) { return blast.land_step <= field.step; });
}

}  // namespace ro::sim::survivors
