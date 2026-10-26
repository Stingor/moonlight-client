#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>

#include "ro/sim/survivors_geometry.h"
#include "ro/sim/survivors_ground.h"
#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_run.h"
#include "ro/sim/survivors_state.h"

#include "survivors_space.h"
#include "survivors_systems.h"
#include "survivors_volley.h"

namespace ro::sim::survivors {

namespace {

// Un monstre qu'un tir peut viser ou toucher : tangible, et qu'il n'a encore
// jamais touché.
[[nodiscard]] bool Reachable(const SurvivorsField& field, const SurvivorsFoe& foe,
                             const SurvivorsShot& shot) noexcept {
  if (!Tangible(field, foe)) return false;
  const auto hit_end = shot.hit_serials.begin() + shot.hit_count;
  return std::find(shot.hit_serials.begin(), hit_end, foe.serial) == hit_end;
}

// Le niveau du skill qui a lancé le tir, s'il occupe encore son emplacement.
[[nodiscard]] const SkillLevel* ShotLevelOf(const SurvivorsField& field,
                                            const SurvivorsShot& shot) {
  if (shot.skill_slot >= field.hero.skills.size()) return nullptr;
  const OwnedSkill& owned = field.hero.skills[shot.skill_slot];
  if (owned.skill_id != shot.skill_id) return nullptr;
  const SurvivorsSkillRule* rule = FindSkill(*field.rules, owned.skill_id);
  return rule != nullptr ? LevelOf(*rule, owned) : nullptr;
}

// Le monstre le plus proche de `center` à `reach_units`, que le tir peut viser ;
// à distance égale, le premier né.
[[nodiscard]] SurvivorsFoe* NearestFoe(SurvivorsField& field, const SurvivorsShot& shot,
                                       FieldPoint center, std::int32_t reach_units) {
  field.foe_grid.Near(center, reach_units, field.foe_points, field.nearby);
  SurvivorsFoe* nearest = nullptr;
  std::int64_t nearest_distance = std::numeric_limits<std::int64_t>::max();
  for (const std::uint32_t index : field.nearby) {
    if (index >= field.foes.size()) continue;
    SurvivorsFoe& foe = field.foes[index];
    if (!Reachable(field, foe, shot)) continue;
    const std::int64_t distance = DistanceSquared(foe.position, center);
    if (distance < nearest_distance) {
      nearest = &foe;
      nearest_distance = distance;
    }
  }
  return nearest;
}

// Le monstre que le tir chevauche, le plus proche ; nullptr sans contact.
[[nodiscard]] SurvivorsFoe* TouchedFoe(SurvivorsField& field, const SurvivorsShot& shot) {
  field.foe_grid.Near(shot.position, shot.hit_radius_units + LargestFoeRadius(*field.rules),
                      field.foe_points, field.nearby);
  SurvivorsFoe* touched = nullptr;
  std::int64_t touched_distance = std::numeric_limits<std::int64_t>::max();
  for (const std::uint32_t index : field.nearby) {
    if (index >= field.foes.size()) continue;
    SurvivorsFoe& foe = field.foes[index];
    if (!Reachable(field, foe, shot)) continue;
    const std::int64_t reach = std::int64_t{shot.hit_radius_units} + FoeRadius(*field.rules, foe);
    const std::int64_t distance = DistanceSquared(foe.position, shot.position);
    if (distance < reach * reach && distance < touched_distance) {
      touched = &foe;
      touched_distance = distance;
    }
  }
  return touched;
}

// Le pas d'un tir chercheur : il se tourne vers son monstre.
[[nodiscard]] FieldPoint HomingStride(SurvivorsField& field, SurvivorsShot& shot) {
  const SkillLevel* level = ShotLevelOf(field, shot);
  // La vitesse du niveau en vigueur ; à défaut, celle que le tir porte.
  const std::int32_t speed = level != nullptr ? ShotUnitsPerStep(*level)
                                              : ClampedUnits(IntegerSqrt(static_cast<std::uint64_t>(
                                                    DistanceSquared(shot.velocity, FieldPoint{}))));
  const std::int32_t reach_units = level != nullptr ? level->reach_units : 0;

  // Il vise son monstre tant qu'il le peut, sinon le plus proche à portée,
  // sinon il file tout droit.
  SurvivorsFoe* target = shot.target_serial != 0 ? FoeBySerial(field, shot.target_serial) : nullptr;
  if (target == nullptr || !Reachable(field, *target, shot)) {
    target = NearestFoe(field, shot, shot.position, reach_units);
    shot.target_serial = target != nullptr ? target->serial : 0;
  }
  FieldPoint stride = shot.velocity;
  if (target != nullptr && speed > 0) {
    const FieldPoint turned = Toward(shot.position, target->position, speed);
    if (turned != FieldPoint{}) shot.velocity = turned;
    // Le pas s'arrête sur le monstre : Toward dépasserait un but plus proche
    // que sa longueur, et un tir rapide oscillerait autour sans le toucher. La
    // vitesse, elle, garde sa longueur pour la suite du vol.
    const std::uint32_t distance =
        IntegerSqrt(static_cast<std::uint64_t>(DistanceSquared(shot.position, target->position)));
    if (std::int64_t{distance} < speed) {
      stride = Toward(shot.position, target->position, static_cast<std::int32_t>(distance));
    } else {
      stride = shot.velocity;
    }
  }
  return stride;
}

// Un pas d'un tir : un tir chercheur se tourne vers son monstre, un projectile
// droit garde sa vitesse ; il avance, touche, gèle et repousse dans le sens de
// son vol. Faux quand le tir s'éteint : vie écoulée, sortie de l'arène, ou
// dernier perçage dépensé.
[[nodiscard]] bool AdvanceShot(SurvivorsField& field, SurvivorsShot& shot) {
  if (field.step >= shot.expires_step) return false;
  const FieldPoint stride = shot.straight ? shot.velocity : HomingStride(field, shot);
  shot.previous_position = shot.position;
  shot.position = shot.position + stride;
  // Un tir qui sort de l'arène s'éteint : sa position reste loin des bornes de
  // l'int32.
  if (!InsideArena(field.ground, shot.position)) return false;

  SurvivorsFoe* touched = TouchedFoe(field, shot);
  if (touched == nullptr) return true;
  const SurvivorsSkillRule* rule = FindSkill(*field.rules, shot.skill_id);
  const SurvivorsElement element = rule != nullptr ? rule->element : SurvivorsElement::kNeutral;
  const SurvivorsStatus status{.freeze_ms = shot.freeze_ms,
                               .knockback_units = shot.knockback_units,
                               .knock_heading = HeadingOf(shot.velocity, field.hero.facing)};
  HurtFoe(field, *touched, DamageAgainst(field, shot.damage, element, *touched), shot.skill_id,
          status);
  // Un tir dont la mémoire est pleine s'éteint, perçage restant ou non : il ne
  // touche jamais deux fois le même monstre. CheckSurvivorsRules borne le
  // perçage pour que la mémoire suffise.
  shot.hit_serials[shot.hit_count++] = touched->serial;
  if (shot.pierce_left == 0 || shot.hit_count >= kMostShotHits) return false;
  --shot.pierce_left;
  return true;
}

}  // namespace

void AdvanceShots(SurvivorsField& field) {
  if (field.rules == nullptr) return;
  // Retrait stable : les tirs gardent leur ordre de naissance.
  std::size_t kept = 0;
  for (std::size_t index = 0; index < field.shots.size(); ++index) {
    SurvivorsShot shot = field.shots[index];
    if (AdvanceShot(field, shot)) field.shots[kept++] = shot;
  }
  field.shots.resize(kept);
}

// LE PROJECTILE DROIT : comme une salve de tirs chercheurs, le k-ième vise le
// k-ième monstre le plus proche à portée (distance, puis naissance), mais il
// part droit sur la position qu'avait ce monstre au tir et ne se recale plus.
// Sa vie, `portée / vitesse` pas tronqué, l'arrête avant sa portée.
bool FireLinear(SurvivorsField& field, std::uint8_t slot, const SurvivorsSkillRule& rule,
                const SkillLevel& level) {
  const std::int32_t speed_per_step = ShotUnitsPerStep(level);
  if (speed_per_step <= 0) return false;
  const auto life_steps = static_cast<std::uint32_t>(level.reach_units / speed_per_step);
  if (life_steps == 0) return false;
  const std::size_t count = std::min<std::size_t>(
      AmountOf(field, level), RoomUnder(field.shots.size(), field.rules->caps.most_shots));
  if (count == 0) return false;

  const FieldPoint hero = field.hero.position;
  GatherNearby(field, hero, level.reach_units);
  if (field.nearby.empty()) return false;
  OrderTargets(field, TargetRule::kNearest, 0);
  for (std::size_t shot = 0; shot < count; ++shot) {
    const SurvivorsFoe& target = field.foes[field.nearby[shot % field.nearby.size()]];
    FieldPoint velocity = Toward(hero, target.position, speed_per_step);
    // Un monstre sur le héros : le projectile part droit devant.
    if (velocity == FieldPoint{}) velocity = TurnVector(field.hero.facing, speed_per_step);
    field.shots.push_back(SurvivorsShot{.serial = field.next_serial++,
                                        .skill_slot = slot,
                                        .skill_id = rule.skill_id,
                                        .position = hero,
                                        .previous_position = hero,
                                        .velocity = velocity,
                                        .damage = level.damage,
                                        .hit_radius_units = level.hit_radius_units,
                                        .expires_step = field.step + life_steps,
                                        .pierce_left = level.pierce,
                                        .target_serial = target.serial,
                                        .hit_serials = {},
                                        .hit_count = 0,
                                        .freeze_ms = level.freeze_ms,
                                        .knockback_units = level.knockback_units,
                                        .straight = true});
    PublishCast(field.news, SurvivorsCast{.skill_id = rule.skill_id,
                                          .kind = CastKind::kShot,
                                          .from = hero,
                                          .to = target.position,
                                          .target_serial = target.serial,
                                          .land_step = 0});
  }
  return true;
}

}  // namespace ro::sim::survivors
