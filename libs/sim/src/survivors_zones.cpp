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

// LES ZONES : des aires posées au sol autour du héros, qui touchent tant
// qu'elles vivent. Un mur touche chaque monstre qui le chevauche, une fois par
// `rehit_ms` et par emplacement de skill ; un pilier se déclenche dès qu'un
// monstre entre dans son aire, frappe toute l'aire, puis se réarme. Une zone à
// coups comptés s'éteint à son dernier coup, toute zone à la fin de sa vie. Un
// orage frappe tout son disque à chaque vague.

namespace {

inline constexpr std::uint8_t kQuarterTurn = kHeadingSteps / 4;
// Les murs d'une salve, en quarts de tour depuis le cap du héros : devant,
// derrière, à gauche (le sens trigonométrique), à droite.
inline constexpr std::array<std::uint8_t, 4> kWallTurns = {0, 2 * kQuarterTurn, kQuarterTurn,
                                                           3 * kQuarterTurn};

[[nodiscard]] std::uint8_t Turned(std::uint8_t heading, std::uint32_t turns) noexcept {
  return static_cast<std::uint8_t>((std::uint32_t{heading} + turns) % kHeadingSteps);
}

// UN MONSTRE AU HASARD À MI-DISTANCE, parmi `field.nearby` : tiré parmi ceux
// au-delà de la moitié de `reach_units`, et parmi tous quand aucun n'y est.
// Un orage posé là laisse la horde venir à lui au lieu de tomber sur le héros.
[[nodiscard]] std::uint32_t RandomAtMidReach(SurvivorsField& field, std::int32_t reach_units) {
  const std::int64_t half = std::int64_t{reach_units} / 2;
  const auto far = [&](std::uint32_t index) {
    const FieldPoint offset = field.foes[index].position - field.hero.position;
    return std::int64_t{offset.x} * offset.x + std::int64_t{offset.y} * offset.y >= half * half;
  };
  const auto beyond = static_cast<std::uint32_t>(std::ranges::count_if(field.nearby, far));
  if (beyond == 0) {
    return field.nearby[field.combat_random.Below(static_cast<std::uint32_t>(field.nearby.size()))];
  }
  std::uint32_t skipped = field.combat_random.Below(beyond);
  for (const std::uint32_t index : field.nearby) {
    if (!far(index)) continue;
    if (skipped == 0) return index;
    --skipped;
  }
  return field.nearby.front();
}

[[nodiscard]] bool Touches(const SurvivorsField& field, const SurvivorsZone& zone,
                           const SurvivorsFoe& foe) noexcept {
  const std::int64_t reach = std::int64_t{zone.radius_units} + FoeRadius(*field.rules, foe);
  return SegmentDistanceSquared(foe.position, zone.from, zone.to) < reach * reach;
}

// Les monstres vivants près de la zone, dans `field.nearby` : une requête
// autour du milieu, de la demi-longueur plus les portées.
void GatherAround(SurvivorsField& field, const SurvivorsZone& zone) {
  const FieldPoint middle{static_cast<std::int32_t>((std::int64_t{zone.from.x} + zone.to.x) / 2),
                          static_cast<std::int32_t>((std::int64_t{zone.from.y} + zone.to.y) / 2)};
  const std::int64_t half_length =
      IntegerSqrt(static_cast<std::uint64_t>(DistanceSquared(zone.from, zone.to))) / 2 + 1;
  GatherNearby(field, middle,
               ClampedUnits(half_length + zone.radius_units + LargestFoeRadius(*field.rules)));
}

void PublishZone(SurvivorsNews& news, const SurvivorsZone& zone, CastKind kind) {
  PublishCast(news, SurvivorsCast{.skill_id = zone.skill_id,
                                  .kind = kind,
                                  .from = zone.from,
                                  .to = zone.to,
                                  .target_serial = 0,
                                  .land_step = 0});
}

[[nodiscard]] bool Spent(const SurvivorsZone& zone) noexcept {
  return zone.limited && zone.hits_left == 0;
}

// Un bloc de barrière éclate à zéro PV.
[[nodiscard]] bool Over(const SurvivorsField& field, const SurvivorsZone& zone) noexcept {
  return field.step >= zone.expires_step || Spent(zone) ||
         (zone.archetype == SkillArchetype::kBarrier && zone.block_hp <= 0);
}

// Un mur : chaque monstre vivant qu'il chevauche, par index croissant, prend
// son coup si l'emplacement de skill peut le retoucher ; le recul le pousse au
// cap du mur.
void SweepWall(SurvivorsField& field, SurvivorsZone& zone) {
  if (zone.skill_slot >= kMostSkillSlots) return;
  GatherAround(field, zone);
  for (const std::uint32_t index : field.nearby) {
    if (Spent(zone)) return;
    SurvivorsFoe& foe = field.foes[index];
    if (foe.hp <= 0 ||
        foe.rehit_ready_step[RehitIndex(field.stage, zone.skill_slot)] > field.step) {
      continue;
    }
    if (!Touches(field, zone, foe)) continue;
    const SurvivorsStatus status{.freeze_ms = zone.freeze_ms,
                                 .knockback_units = zone.knockback_units,
                                 .knock_heading = zone.push_heading};
    HurtFoe(field, foe, DamageAgainst(field, zone.damage, zone.element, foe), zone.skill_id,
            status);
    foe.rehit_ready_step[RehitIndex(field.stage, zone.skill_slot)] = field.step + zone.rehit_steps;
    if (zone.limited) --zone.hits_left;
  }
}

// Un pilier armé : un monstre dans son aire le déclenche, et il frappe alors
// chaque monstre vivant de l'aire, par index croissant, avec un recul qui
// l'écarte du pilier ; il se réarme `rehit_steps` plus tard. Un déclenchement
// compte pour un coup.
void TriggerPillar(SurvivorsField& field, SurvivorsZone& zone) {
  if (field.step < zone.armed_step) return;
  GatherAround(field, zone);
  std::erase_if(field.nearby,
                [&](std::uint32_t index) { return !Touches(field, zone, field.foes[index]); });
  if (field.nearby.empty()) return;
  for (const std::uint32_t index : field.nearby) {
    SurvivorsFoe& foe = field.foes[index];
    if (foe.hp <= 0) continue;
    const SurvivorsStatus status{.freeze_ms = zone.freeze_ms,
                                 .knockback_units = zone.knockback_units,
                                 .knock_heading = HeadingAway(zone.from, foe, zone.push_heading)};
    HurtFoe(field, foe, DamageAgainst(field, zone.damage, zone.element, foe), zone.skill_id,
            status);
  }
  zone.armed_step = field.step + zone.rehit_steps;
  if (zone.limited) --zone.hits_left;
}

// Une vague d'orage, à son pas : chaque monstre vivant du disque, par index
// croissant, prend son coup, avec l'aveuglement et la malédiction de l'orage ;
// à partir de `freeze_from_wave`, il gèle. Le recul l'écarte du centre. Une
// vague compte pour un coup, même sans monstre dans le disque.
void StrikeStorm(SurvivorsField& field, SurvivorsZone& zone) {
  if (field.step < zone.armed_step) return;
  ++zone.waves_done;
  const bool freezes = zone.freeze_ms > 0 && zone.waves_done >= zone.freeze_from_wave;
  GatherAround(field, zone);
  for (const std::uint32_t index : field.nearby) {
    SurvivorsFoe& foe = field.foes[index];
    if (foe.hp <= 0 || !Touches(field, zone, foe)) continue;
    const SurvivorsStatus status{.freeze_ms = freezes ? zone.freeze_ms : 0,
                                 .knockback_units = zone.knockback_units,
                                 .knock_heading = HeadingAway(zone.from, foe, zone.push_heading),
                                 .blind_ms = zone.blind_ms,
                                 .curse_ms = zone.curse_ms};
    HurtFoe(field, foe, DamageAgainst(field, zone.damage, zone.element, foe), zone.skill_id,
            status);
  }
  zone.armed_step = field.step + zone.rehit_steps;
  if (zone.limited && zone.hits_left > 0) --zone.hits_left;
}

// Une zone neuve d'un niveau, sans sa forme.
[[nodiscard]] SurvivorsZone ZoneOf(SurvivorsField& field, std::uint8_t slot,
                                   const SurvivorsSkillRule& rule, const SkillLevel& level) {
  return SurvivorsZone{
      .serial = field.next_serial++,
      .skill_slot = slot,
      .skill_id = rule.skill_id,
      .archetype = rule.archetype,
      .from = {},
      .to = {},
      .radius_units = 0,
      .damage = level.damage,
      .knockback_units = level.knockback_units,
      .freeze_ms = level.freeze_ms,
      .expires_step =
          field.step + ScaledSteps(level.life_ms, field.hero.stats.Of(SurvivorsStat::kDurationPct)),
      .rehit_steps = StepsOf(level.rehit_ms),
      .armed_step = field.step,
      .hits_left = level.zone_hits,
      .limited = level.zone_hits > 0,
      .push_heading = 0,
      .element = rule.element};
}

}  // namespace

// LE MUR : `amount` murs, au plus quatre (devant, derrière, à gauche, à droite
// du cap du héros), chacun centré à `reach_units` du héros dans sa direction
// et perpendiculaire à elle, long de `length × (100 + aire) / 100`. Le recul
// d'un mur pousse dans sa direction : celle du cap du héros pour le premier.
bool FireWall(SurvivorsField& field, std::uint8_t slot, const SurvivorsSkillRule& rule,
              const SkillLevel& level) {
  const std::size_t count = std::min({std::size_t{AmountOf(field, level)}, kWallTurns.size(),
                                      RoomUnder(field.zones.size(), field.rules->caps.most_zones)});
  if (count == 0) return false;
  const std::int32_t half_length = AreaUnits(field, level.length_units) / 2;
  if (half_length <= 0) return false;
  for (std::size_t wall = 0; wall < count; ++wall) {
    const std::uint8_t heading = Turned(field.hero.facing, kWallTurns[wall]);
    const FieldPoint middle = field.hero.position + TurnVector(heading, level.reach_units);
    SurvivorsZone zone = ZoneOf(field, slot, rule, level);
    zone.from = middle + TurnVector(Turned(heading, kQuarterTurn), half_length);
    zone.to = middle + TurnVector(Turned(heading, 3 * kQuarterTurn), half_length);
    zone.radius_units = level.hit_radius_units;
    zone.push_heading = heading;
    field.zones.push_back(zone);
    PublishZone(field.news, zone, CastKind::kWallOn);
  }
  return true;
}

// LE CERCLE DE PILIERS : `amount` piliers, le k-ième au cap `k × 64 /
// amount`, sur un cercle de `reach_units` autour du héros ; chacun frappe une
// aire de `area_radius_units`. Cercle et aire sont majorés de l'aire du héros.
// Faute de place sous le plafond, les premiers seulement sont posés.
bool FireRing(SurvivorsField& field, std::uint8_t slot, const SurvivorsSkillRule& rule,
              const SkillLevel& level) {
  const std::uint32_t pillars = AmountOf(field, level);
  const std::size_t count =
      std::min<std::size_t>(pillars, RoomUnder(field.zones.size(), field.rules->caps.most_zones));
  if (count == 0) return false;
  const std::int32_t radius = AreaUnits(field, level.area_radius_units);
  if (radius <= 0) return false;
  const std::int32_t circle = AreaUnits(field, level.reach_units);
  for (std::size_t pillar = 0; pillar < count; ++pillar) {
    const auto heading =
        static_cast<std::uint8_t>(std::uint64_t{pillar} * kHeadingSteps / pillars % kHeadingSteps);
    SurvivorsZone zone = ZoneOf(field, slot, rule, level);
    zone.from = field.hero.position + TurnVector(heading, circle);
    zone.to = zone.from;
    zone.radius_units = radius;
    zone.push_heading = heading;
    field.zones.push_back(zone);
    PublishZone(field.news, zone, CastKind::kRingOn);
  }
  return true;
}

// L'ORAGE : une aire de `area_radius_units`, majorée de l'aire du héros, posée
// sur le monstre que la règle du skill vise à `reach_units`, ou sur le héros
// (kHero). Sa première vague tombe au pas qui suit, puis une tous les
// `interval_ms`, `amount` vagues en tout (majorées de la quantité du héros).
// Elle attend un monstre à portée — dans son aire, centrée sur le héros — et
// sa place sous le plafond des zones.
bool FireStorm(SurvivorsField& field, std::uint8_t slot, const SurvivorsSkillRule& rule,
               const SkillLevel& level) {
  if (RoomUnder(field.zones.size(), field.rules->caps.most_zones) == 0) return false;
  const std::int32_t radius = AreaUnits(field, level.area_radius_units);
  const std::uint32_t waves =
      std::min<std::uint32_t>(AmountOf(field, level), std::numeric_limits<std::uint8_t>::max());
  if (radius <= 0 || waves == 0) return false;
  FieldPoint center = field.hero.position;
  if (rule.target == TargetRule::kHero) {
    GatherNearby(field, center, radius);
    if (field.nearby.empty()) return false;
  } else if (rule.target == TargetRule::kRandom) {
    GatherNearby(field, field.hero.position, level.reach_units);
    if (field.nearby.empty()) return false;
    center = field.foes[RandomAtMidReach(field, level.reach_units)].position;
  } else {
    GatherNearby(field, field.hero.position, level.reach_units);
    if (field.nearby.empty()) return false;
    OrderTargets(field, rule.target, radius);
    center = field.foes[field.nearby.front()].position;
  }
  SurvivorsZone zone = ZoneOf(field, slot, rule, level);
  zone.from = center;
  zone.to = center;
  zone.radius_units = radius;
  zone.rehit_steps = std::max<std::uint32_t>(StepsOf(level.interval_ms), 1);
  zone.armed_step = field.step + 1;
  zone.hits_left = static_cast<std::uint8_t>(waves);
  zone.limited = true;
  // La vie couvre toutes les vagues : la dernière l'éteint.
  zone.expires_step = zone.armed_step + waves * zone.rehit_steps;
  zone.freeze_from_wave = level.freeze_from_wave;
  zone.blind_ms = level.blind_ms;
  zone.curse_ms = level.curse_ms;
  field.zones.push_back(zone);
  PublishZone(field.news, zone, CastKind::kStormOn);
  return true;
}

void AdvanceZones(SurvivorsField& field) {
  if (field.rules == nullptr) return;
  // Par index, dans l'ordre de pose : une zone n'en pose aucune autre.
  for (std::size_t index = 0; index < field.zones.size(); ++index) {
    SurvivorsZone& zone = field.zones[index];
    // Un bloc ne frappe rien : HoldBarriers retient la horde.
    if (Over(field, zone) || zone.archetype == SkillArchetype::kBarrier) continue;
    // Une zone vivante casse les plantes de son aire à chaque pas, monstres
    // ou non ; un pilier dès qu'il est armé, un orage à chaque vague.
    const bool waits =
        (zone.archetype == SkillArchetype::kRing || zone.archetype == SkillArchetype::kStorm) &&
        field.step < zone.armed_step;
    if (!waits) BreakPlantsWithin(field, zone.from, zone.to, zone.radius_units);
    if (zone.archetype == SkillArchetype::kRing) {
      TriggerPillar(field, zone);
    } else if (zone.archetype == SkillArchetype::kStorm) {
      StrikeStorm(field, zone);
    } else {
      SweepWall(field, zone);
    }
  }
  for (const SurvivorsZone& zone : field.zones) {
    if (Over(field, zone)) PublishZone(field.news, zone, CastKind::kZoneOff);
  }
  std::erase_if(field.zones, [&](const SurvivorsZone& zone) { return Over(field, zone); });
}

}  // namespace ro::sim::survivors
