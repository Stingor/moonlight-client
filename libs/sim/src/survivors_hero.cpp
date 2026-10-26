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

// La marche du héros à ce pas : celle de ses statistiques, ou, sous une Speed
// Potion, celle de sa classe majorée de sa statistique de marche PLUS la
// majoration de l'objet, sommées avant une seule troncature.
[[nodiscard]] std::int32_t HeroPace(const SurvivorsField& field) noexcept {
  const SurvivorsHero& hero = field.hero;
  if (field.step >= hero.haste_until_step || field.hero_class == nullptr) {
    return hero.units_per_step;
  }
  const PickupRule* haste = FindPickup(*field.rules, PickupKind::kHaste);
  if (haste == nullptr) return hero.units_per_step;
  return ScaledUnits(UnitsPerStep(field.hero_class->walk_ms_per_cell),
                     std::int64_t{hero.stats.Of(SurvivorsStat::kWalkPct)} + haste->amount);
}

// Le contact d'un monstre : celui de sa règle, à `touch_pct` % pour une élite,
// puis majoré du rang de Cursed Water du départ.
[[nodiscard]] std::int32_t TouchDamageOf(const SurvivorsField& field, const SurvivorsFoe& foe,
                                         const FoeRule& rule) noexcept {
  std::int32_t touch = rule.touch_damage;
  if (foe.elite) {
    touch = ClampedUnits(std::int64_t{touch} *
                         std::clamp(field.rules->elites.touch_pct, 0, kMostStatBonus) / kPercent);
  }
  const std::uint8_t cursed = field.start.cursed_water;
  if (cursed == 0 || cursed >= field.rules->cursed_water.size()) return touch;
  const std::int32_t raise_pct =
      std::clamp(field.rules->cursed_water[cursed].touch_pct, 0, kMostStatBonus);
  return ClampedUnits(std::int64_t{touch} * (kPercent + raise_pct) / kPercent);
}

// LA RELÈVE : PV à `hp_pct` % des PV max (un au moins), invulnérable dès ce
// pas et pendant `invulnerable_ms`, et l'onde qui écarte du héros, jusqu'à
// `push_units`, chaque monstre tangible plus proche qu'un recul peut déplacer.
void Revive(SurvivorsField& field) {
  const ReviveRule& revive = field.rules->revive;
  SurvivorsHero& hero = field.hero;
  --field.lives_left;
  hero.hp = static_cast<std::int32_t>(std::clamp<std::int64_t>(
      std::int64_t{hero.max_hp} * std::clamp<std::int64_t>(revive.hp_pct, 0, kPercent) / kPercent,
      1, hero.max_hp));
  const std::uint64_t until = std::uint64_t{field.step} + StepsOf(revive.invulnerable_ms) + 1;
  hero.invulnerable_until_step = static_cast<std::uint32_t>(
      std::min<std::uint64_t>(until, std::numeric_limits<std::uint32_t>::max()));
  const std::int64_t reach = std::max(revive.push_units, 0);
  for (SurvivorsFoe& foe : field.foes) {
    if (!Tangible(field, foe)) continue;
    const std::int64_t distance_squared = DistanceSquared(foe.position, hero.position);
    if (distance_squared >= reach * reach) continue;
    const auto distance =
        static_cast<std::int64_t>(IntegerSqrt(static_cast<std::uint64_t>(distance_squared)));
    (void)ApplyStatus(
        field, foe,
        SurvivorsStatus{.freeze_ms = 0,
                        .knockback_units = static_cast<std::int32_t>(reach - distance),
                        .knock_heading = HeadingAway(hero.position, foe, hero.facing)});
  }
  field.news.revives.push_back(
      SurvivorsReviveNews{.at = hero.position, .lives_left = field.lives_left});
}

}  // namespace

void MoveHero(SurvivorsField& field, SurvivorsInput input) {
  SurvivorsHero& hero = field.hero;
  hero.previous_position = hero.position;
  if (input.heading == kNoHeading) return;
  const auto heading = static_cast<std::uint8_t>(input.heading % kHeadingSteps);
  hero.facing = heading;
  hero.position = Slide(field.ground, hero.position, TurnVector(heading, HeroPace(field)));
}

bool HurtHero(SurvivorsField& field, std::int32_t damage) {
  SurvivorsHero& hero = field.hero;
  if (field.step < hero.invulnerable_until_step) return false;
  const std::int32_t armor = hero.stats.Of(SurvivorsStat::kArmor);
  // En 64 bits : une armure négative ne fait pas déborder la soustraction.
  const std::int32_t taken = static_cast<std::int32_t>(std::clamp<std::int64_t>(
      std::int64_t{damage} - armor, 1, std::numeric_limits<std::int32_t>::max()));
  hero.hp = SaturatedAdd(hero.hp, -std::int64_t{taken});
  field.news.hero_hurt = SaturatedAdd(field.news.hero_hurt, taken);
  if (hero.hp > 0) return false;
  // La partie n'est perdue qu'à la dernière vie : une relève ne compte pas de
  // chute au bilan.
  if (field.lives_left > 0) {
    Revive(field);
    return false;
  }
  hero.hp = 0;
  field.phase = SurvivorsPhase::kOver;
  field.tally.outcome = SurvivorsOutcome::kFell;
  return true;
}

void TouchHero(SurvivorsField& field) {
  const SurvivorsRules& rules = *field.rules;
  const SurvivorsHero& hero = field.hero;
  for (SurvivorsFoe& foe : field.foes) {
    // Défense seulement : BuryFoes, qui passe avant dans le pas, a déjà retiré
    // tout monstre à PV nuls. C'est cet ordre qui empêche un monstre tué ce pas
    // de frapper.
    if (foe.hp <= 0 || foe.touch_ready_step > field.step) continue;
    // Un monstre gelé ou caché ne touche pas.
    if (Frozen(field, foe) || Hidden(field, foe)) continue;
    const FoeRule* rule = FindFoe(rules, foe.foe_id);
    // Un monstre sans dégâts de contact ne blesse pas : le minimum d'un PV ne
    // vaut que pour un contact que l'armure absorbe.
    if (rule == nullptr || rule->touch_damage <= 0) continue;
    const std::int64_t reach = std::int64_t{rules.tuning.hero_radius_units} +
                               FoeRadius(rules, foe) + rules.tuning.touch_reach_units;
    if (DistanceSquared(foe.position, hero.position) >= reach * reach) continue;
    foe.touch_ready_step = field.step + StepsOf(rule->touch_interval_ms);
    if (HurtHero(field, TouchDamageOf(field, foe, *rule))) return;
  }
}

void Regenerate(SurvivorsField& field) {
  SurvivorsHero& hero = field.hero;
  const std::int32_t per_minute = hero.stats.Of(SurvivorsStat::kRegenPerMinute);
  if (per_minute <= 0) return;
  // Les PV par minute s'accumulent pas à pas ; un PV naît chaque fois que la
  // réserve atteint le nombre de pas d'une minute, perdu si le héros est plein.
  hero.regen_carry += static_cast<std::uint32_t>(per_minute);
  const std::uint32_t gained = hero.regen_carry / kStepsPerMinute;
  hero.regen_carry %= kStepsPerMinute;
  if (gained == 0 || hero.hp >= hero.max_hp) return;
  hero.hp = static_cast<std::int32_t>(
      std::min<std::int64_t>(hero.max_hp, std::int64_t{hero.hp} + gained));
}

}  // namespace ro::sim::survivors
