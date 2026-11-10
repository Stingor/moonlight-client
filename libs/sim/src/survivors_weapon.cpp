#include "survivors_weapon.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>

#include "ro/sim/survivors_geometry.h"
#include "ro/sim/survivors_news.h"

#include "survivors_space.h"
#include "survivors_systems.h"
#include "survivors_volley.h"

namespace ro::sim::survivors {

namespace {

// Les monstres en plus que le coup de l'arme traverse, sous la mémoire d'un tir.
[[nodiscard]] std::int32_t WeaponPierceOf(const SurvivorsField& field) noexcept {
  return std::clamp(field.hero.stats.Of(SurvivorsStat::kWeaponPierce), 0,
                    static_cast<std::int32_t>(kMostShotHits - 1));
}

[[nodiscard]] std::uint8_t PiercedUnderMemory(std::int32_t own, std::int32_t more) noexcept {
  return static_cast<std::uint8_t>(
      std::min(own + more, static_cast<std::int32_t>(kMostShotHits - 1)));
}

}  // namespace

void ArmHero(const SurvivorsRules& rules, const ClassRule& hero_class, SurvivorsHero& hero) {
  const SurvivorsSkillRule* weapon = FindSkill(rules, hero_class.weapon_skill_id);
  if (weapon == nullptr || !IsWeaponSkill(*weapon)) return;
  hero.skills.insert(hero.skills.begin(), OwnedSkill{.skill_id = weapon->skill_id, .level = 1});
}

void RaiseWeapon(SurvivorsField& field) {
  if (field.rules == nullptr) return;
  const std::size_t armed_level = field.hero.evolutions > 0 ? 2 : 1;
  for (OwnedSkill& owned : field.hero.skills) {
    const SurvivorsSkillRule* rule = FindSkill(*field.rules, owned.skill_id);
    if (rule == nullptr || !IsWeaponSkill(*rule)) continue;
    const std::size_t level = std::min(armed_level, rule->levels.size());
    if (owned.level < level) owned.level = static_cast<std::uint8_t>(level);
  }
}

bool IsWeaponSkillId(const SurvivorsField& field, std::uint16_t skill_id) {
  if (field.rules == nullptr) return false;
  const SurvivorsSkillRule* rule = FindSkill(*field.rules, skill_id);
  return rule != nullptr && IsWeaponSkill(*rule);
}

SkillLevel WeaponLevelOf(const SurvivorsField& field, const SurvivorsSkillRule& rule,
                         const SkillLevel& level) {
  SkillLevel played = level;
  const std::int32_t pierce = WeaponPierceOf(field);
  if (pierce == 0) return played;
  switch (rule.archetype) {
    case SkillArchetype::kLinear:
    case SkillArchetype::kHoming:
      played.pierce = PiercedUnderMemory(level.pierce, pierce);
      break;
    // Une frappe enchaîne à plein sur les monstres qui jouxtent sa cible.
    case SkillArchetype::kTargetStrike:
      played.bounces = PiercedUnderMemory(level.bounces, pierce);
      played.bounce_pct = static_cast<std::int32_t>(kPercent);
      played.bounce_reach_units = Cells(kWeaponCleaveReachCells);
      break;
    case SkillArchetype::kCone:
      played.reach_units = level.reach_units + pierce * Cells(kWeaponCleaveReachCells);
      break;
    case SkillArchetype::kClusterBlast:
      played.area_radius_units = level.area_radius_units + pierce * CellTenths(kWeaponSpreadTenths);
      break;
    case SkillArchetype::kOrbit:
    case SkillArchetype::kWall:
    case SkillArchetype::kNova:
    case SkillArchetype::kRing:
    case SkillArchetype::kStorm:
    case SkillArchetype::kBarrier:
    case SkillArchetype::kStance:
    case SkillArchetype::kReflex:
    case SkillArchetype::kSummon:
    case SkillArchetype::kMend:
      break;
  }
  return played;
}

std::uint32_t WeaponReadyAfter(const SurvivorsField& field, const SkillLevel& level) {
  const std::int64_t cut_pct =
      std::min<std::int64_t>(std::int64_t{field.hero.stats.Of(SurvivorsStat::kCooldownPct)} +
                                 field.hero.stats.Of(SurvivorsStat::kWeaponDelayCutPct),
                             field.rules->tuning.most_cooldown_pct);
  return field.step + ScaledSteps(level.cooldown_ms, -cut_pct);
}

void SplitWeaponHit(SurvivorsField& field, std::uint16_t skill_id, std::int32_t base_damage,
                    SurvivorsElement element, const SurvivorsFoe& struck) {
  const std::int32_t chance_pct = field.hero.stats.Of(SurvivorsStat::kWeaponSplitChancePct);
  if (chance_pct <= 0 || !IsWeaponSkillId(field, skill_id)) return;
  if (field.combat_random.Below(static_cast<std::uint32_t>(kWholeChancePct)) >=
      static_cast<std::uint32_t>(chance_pct)) {
    return;
  }
  const std::uint32_t struck_serial = struck.serial;
  const FieldPoint from = struck.position;
  GatherNearby(field, from, Cells(kWeaponSplitReachCells));
  SurvivorsFoe* next = nullptr;
  std::int64_t next_distance = std::numeric_limits<std::int64_t>::max();
  for (const std::uint32_t index : field.nearby) {
    SurvivorsFoe& foe = field.foes[index];
    if (foe.serial == struck_serial) continue;
    const std::int64_t distance = DistanceSquared(foe.position, from);
    if (distance < next_distance) {
      next = &foe;
      next_distance = distance;
    }
  }
  if (next == nullptr) return;
  const auto split_damage = static_cast<std::int32_t>(std::int64_t{std::max(base_damage, 0)} *
                                                      kWeaponSplitDamagePct / kPercent);
  PublishCast(field.news, SurvivorsCast{.skill_id = skill_id,
                                        .kind = CastKind::kStrike,
                                        .from = from,
                                        .to = next->position,
                                        .target_serial = next->serial,
                                        .land_step = field.step});
  HurtFoe(field, *next, DamageAgainst(field, split_damage, element, *next), skill_id,
          SurvivorsStatus{});
}

}  // namespace ro::sim::survivors
