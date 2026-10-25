#pragma once

// LES AIDES QUE LES ARCHÉTYPES DE SKILL PARTAGENT : la taille d'une salve, la
// place sous un plafond, la visée d'une salve sur les monstres à portée, et les
// statuts qu'un coup inflige. Frappes, tirs, éclats et zones visent et gèlent
// ainsi de la même façon.
//
// En-tête PRIVÉ de `libs/sim/src`.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

#include "ro/sim/survivors_content.h"
#include "ro/sim/survivors_geometry.h"
#include "ro/sim/survivors_news.h"
#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_run.h"
#include "ro/sim/survivors_state.h"

#include "survivors_space.h"
#include "survivors_systems.h"

namespace ro::sim::survivors {

// ── Les mesures d'une salve ────────────────────────────────────────────────

// Une durée majorée de `bonus_pct` pour cent, en pas entiers.
[[nodiscard]] inline std::uint32_t ScaledSteps(std::uint32_t ms, std::int64_t bonus_pct) noexcept {
  const std::int64_t scaled = Scaled(ms, bonus_pct);
  return StepsOf(static_cast<std::uint32_t>(
      std::min<std::int64_t>(scaled, std::numeric_limits<std::uint32_t>::max())));
}

// Le nombre d'objets d'une salve : la quantité du niveau, plus la statistique,
// au plus kMostVolley.
[[nodiscard]] inline std::uint32_t AmountOf(const SurvivorsField& field,
                                            const SkillLevel& level) noexcept {
  const std::int64_t amount =
      std::int64_t{level.amount} + field.hero.stats.Of(SurvivorsStat::kAmount);
  return static_cast<std::uint32_t>(std::clamp<std::int64_t>(amount, 0, kMostVolley));
}

// Une mesure d'aire majorée de la statistique d'aire du héros, au plus le côté
// de la plus grande arène : ce qu'elle touche reste dans l'int32.
[[nodiscard]] inline std::int32_t AreaUnits(const SurvivorsField& field,
                                            std::int32_t units) noexcept {
  return std::min(ScaledUnits(units, field.hero.stats.Of(SurvivorsStat::kAreaPct)),
                  Cells(kMostArenaSideCells));
}

// La place qui reste sous un plafond pour `count` objets déjà en jeu.
[[nodiscard]] inline std::size_t RoomUnder(std::size_t count, std::uint16_t cap) noexcept {
  return count < cap ? std::size_t{cap} - count : 0;
}

inline void PublishCast(SurvivorsNews& news, const SurvivorsCast& cast) {
  if (news.casts.size() < kMostNewsCasts) {
    news.casts.push_back(cast);
  } else {
    ++news.dropped;
  }
}

// ── La visée ───────────────────────────────────────────────────────────────

// L'élément de défense d'un monstre : Water tant qu'il est gelé, sinon celui
// de sa règle, neutre pour un Id inconnu des règles.
[[nodiscard]] inline SurvivorsElement ElementOf(const SurvivorsField& field,
                                                const SurvivorsFoe& foe) {
  if (Frozen(field, foe)) return SurvivorsElement::kWater;
  const FoeRule* rule = FindFoe(*field.rules, foe.foe_id);
  return rule != nullptr ? rule->element : SurvivorsElement::kNeutral;
}

// `field.nearby` : les index des monstres tangibles (vivants, pas cachés) à
// `reach_units` de `center`, croissants. Un index que la grille rendrait hors
// de `foes` est écarté.
inline void GatherNearby(SurvivorsField& field, FieldPoint center, std::int32_t reach_units) {
  field.foe_grid.Near(center, reach_units, field.foe_points, field.nearby);
  const std::size_t foe_count = field.foes.size();
  std::erase_if(field.nearby, [&](std::uint32_t index) {
    return index >= foe_count || !Tangible(field, field.foes[index]);
  });
}

// Le nombre de monstres tangibles à moins (strictement) de `radius_units` de
// `center`, par la grille, dans `field.around`.
[[nodiscard]] inline std::uint32_t CrowdAround(SurvivorsField& field, FieldPoint center,
                                               std::int32_t radius_units) {
  field.foe_grid.Near(center, radius_units, field.foe_points, field.around);
  const std::int64_t radius = radius_units;
  std::uint32_t crowd = 0;
  for (const std::uint32_t index : field.around) {
    if (index >= field.foes.size() || !Tangible(field, field.foes[index])) continue;
    if (DistanceSquared(field.foes[index].position, center) < radius * radius) ++crowd;
  }
  return crowd;
}

// Un tri par insertion, sans tampon : un monstre ne passe devant un autre que
// s'il le précède strictement. Les index entrent croissants, donc par
// naissance, qui départage : l'ordre est total.
template <typename Before>
void InsertionSort(std::vector<std::uint32_t>& indexes, Before before) {
  for (std::size_t sorted = 1; sorted < indexes.size(); ++sorted) {
    const std::uint32_t moving = indexes[sorted];
    std::size_t slot = sorted;
    while (slot > 0 && before(moving, indexes[slot - 1])) {
      indexes[slot] = indexes[slot - 1];
      --slot;
    }
    indexes[slot] = moving;
  }
}

// L'ordre de visée, sur `field.nearby` : distance au héros pour kNearest, PV
// non promis décroissants pour kStrongest, et pour kDensest le nombre de
// monstres à moins de `crowd_radius_units` de chacun, décroissant, compté une
// fois pour toute la salve ; la naissance départage. kRandom garde l'ordre de
// naissance, et tire chaque frappe.
inline void OrderTargets(SurvivorsField& field, TargetRule target,
                         std::int32_t crowd_radius_units) {
  const FieldPoint hero = field.hero.position;
  const auto& foes = field.foes;
  if (target == TargetRule::kNearest) {
    InsertionSort(field.nearby, [&](std::uint32_t a, std::uint32_t b) {
      return DistanceSquared(foes[a].position, hero) < DistanceSquared(foes[b].position, hero);
    });
  } else if (target == TargetRule::kStrongest) {
    InsertionSort(field.nearby, [&](std::uint32_t a, std::uint32_t b) {
      return std::int64_t{foes[a].hp} - foes[a].pending_damage >
             std::int64_t{foes[b].hp} - foes[b].pending_damage;
    });
  } else if (target == TargetRule::kDensest) {
    field.crowd.assign(foes.size(), 0);
    for (const std::uint32_t index : field.nearby) {
      field.crowd[index] = CrowdAround(field, foes[index].position, crowd_radius_units);
    }
    const auto& crowd = field.crowd;
    InsertionSort(field.nearby,
                  [&](std::uint32_t a, std::uint32_t b) { return crowd[a] > crowd[b]; });
  }
}

// ── Les statuts ────────────────────────────────────────────────────────────

struct StatusApplied {
  bool froze = false;
  bool knocked = false;
};

// Le gel et le recul d'un coup sur un monstre vivant. Le gel le tient jusqu'à
// `step + StepsOf(freeze × (100 + durée) / 100)`, sans jamais raccourcir un
// gel en cours ; le recul remplace celui en cours. Ni l'un ni l'autre sur un
// boss, ni sur un monstre qui y est insensible par ses traits.
[[nodiscard]] inline StatusApplied ApplyStatus(SurvivorsField& field, SurvivorsFoe& foe,
                                               const SurvivorsStatus& status) {
  StatusApplied applied;
  if (foe.hp <= 0 || (status.freeze_ms == 0 && status.knockback_units <= 0)) return applied;
  const FoeRule* rule = FindFoe(*field.rules, foe.foe_id);
  if (rule == nullptr || rule->rank != FoeRank::kCommon) return applied;
  const std::uint8_t traits = rule->behaviour.traits;
  if (status.freeze_ms > 0 && (traits & kFoeFreezeImmune) == 0) {
    const std::uint32_t steps =
        ScaledSteps(status.freeze_ms, field.hero.stats.Of(SurvivorsStat::kDurationPct));
    const std::uint64_t until = std::uint64_t{field.step} + steps;
    foe.freeze_until_step =
        std::max(foe.freeze_until_step, static_cast<std::uint32_t>(std::min<std::uint64_t>(
                                            until, std::numeric_limits<std::uint32_t>::max())));
    applied.froze = true;
  }
  if (status.knockback_units > 0 && (traits & kFoeKnockbackImmune) == 0) {
    foe.knock_heading = static_cast<std::uint8_t>(status.knock_heading % kHeadingSteps);
    foe.knock_left_units = status.knockback_units;
    applied.knocked = true;
  }
  return applied;
}

// Le cap qui écarte `foe` de `from` ; pour deux points confondus, le cap
// `fallback`.
[[nodiscard]] inline std::uint8_t HeadingAway(FieldPoint from, const SurvivorsFoe& foe,
                                              std::uint8_t fallback) noexcept {
  return HeadingOf(foe.position - from, fallback);
}

// ── Les aires ──────────────────────────────────────────────────────────────

// La distance au carré de `point` au segment [from, to] : la projection de
// `point` sur la droite, ramenée dans [0, |ab|²] et divisée en tronquant, en
// int64. Un segment réduit à un point rend la distance à ce point.
[[nodiscard]] inline std::int64_t SegmentDistanceSquared(FieldPoint point, FieldPoint from,
                                                         FieldPoint to) noexcept {
  const std::int64_t ab_x = std::int64_t{to.x} - from.x;
  const std::int64_t ab_y = std::int64_t{to.y} - from.y;
  const std::int64_t length_squared = ab_x * ab_x + ab_y * ab_y;
  if (length_squared == 0) return DistanceSquared(point, from);
  const std::int64_t ap_x = std::int64_t{point.x} - from.x;
  const std::int64_t ap_y = std::int64_t{point.y} - from.y;
  const std::int64_t along = std::clamp<std::int64_t>(ap_x * ab_x + ap_y * ab_y, 0, length_squared);
  const std::int64_t dx = ap_x - ab_x * along / length_squared;
  const std::int64_t dy = ap_y - ab_y * along / length_squared;
  return dx * dx + dy * dy;
}

// LES PLANTES QU'UNE AIRE CASSE (contact du héros, éclat, nova, mur, pilier,
// orbe ; jamais une frappe ni un tir) : celles dont le centre est à moins de
// `radius_units`, plus le rayon d'un petit monstre, du segment [from, to] (un
// point si from == to). Chacune laisse l'objet de sa règle sur sa case, dans
// l'ordre de pose. Au plafond des objets, une plante ne casse pas : son objet
// attend la place.
inline void BreakPlantsWithin(SurvivorsField& field, FieldPoint from, FieldPoint to,
                              std::int32_t radius_units) {
  if (field.plants.empty()) return;
  const SurvivorsRules& rules = *field.rules;
  const std::int64_t reach =
      std::int64_t{radius_units} +
      rules.tuning.foe_radius_units[static_cast<std::size_t>(FoeSize::kSmall)];
  // Retrait stable, par index : les plantes gardent leur ordre de pose.
  std::size_t kept = 0;
  for (std::size_t index = 0; index < field.plants.size(); ++index) {
    const SurvivorsPlant plant = field.plants[index];
    if (SegmentDistanceSquared(plant.position, from, to) < reach * reach) {
      const auto rule = std::find_if(
          rules.plants.begin(), rules.plants.end(),
          [&](const PlantRule& candidate) { return candidate.foe_id == plant.foe_id; });
      const PickupRule* pickup =
          rule != rules.plants.end() ? FindPickup(rules, rule->pickup) : nullptr;
      if (pickup == nullptr) continue;
      if (DropPickup(field, pickup->kind, pickup->item_id, plant.position)) continue;
    }
    field.plants[kept++] = plant;
  }
  field.plants.resize(kept);
}

}  // namespace ro::sim::survivors
