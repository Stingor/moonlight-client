#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <type_traits>

#include "ro/sim/survivors_match.h"
#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_run.h"
#include "ro/sim/survivors_terrain.h"

#include "survivors_fnv.h"

namespace ro::sim {

// L'EMPREINTE DES RÈGLES : tout ce qui change le cours d'une partie. Deux clients qui
// la partagent jouent la même chose ; une valeur oubliée ici ne se verrait qu'en
// pleine partie, par une divergence. Les textes et les apparences, que la simulation
// ne lit pas, n'y entrent pas.

namespace {

template <typename Value>
void Put(FnvDigest& digest, Value value) {
  if constexpr (std::is_enum_v<Value>) {
    Put(digest, static_cast<std::underlying_type_t<Value>>(value));
  } else if constexpr (std::is_signed_v<Value>) {
    digest.AddSigned(value);
  } else {
    digest.Add(value);
  }
}

template <typename... Values>
void PutAll(FnvDigest& digest, Values... values) {
  (Put(digest, values), ...);
}

void PutText(FnvDigest& digest, std::string_view text) {
  digest.Add(text.size());
  for (const char letter : text) Put(digest, static_cast<unsigned char>(letter));
}

template <typename Item, typename Each>
void PutEach(FnvDigest& digest, std::span<const Item> items, Each each) {
  digest.Add(items.size());
  for (const Item& item : items) each(item);
}

template <typename Number>
void PutNumbers(FnvDigest& digest, std::span<const Number> numbers) {
  PutEach(digest, numbers, [&](Number number) { Put(digest, number); });
}

void PutSkillLevel(FnvDigest& digest, const SkillLevel& level) {
  PutAll(digest, level.damage, level.cooldown_ms, level.amount, level.interval_ms,
         level.reach_units, level.speed_units_per_s, level.hit_radius_units, level.life_ms,
         level.rest_ms, level.period_ms, level.rehit_ms, level.pierce, level.chill_ms,
         level.area_radius_units, level.splash_pct, level.length_units, level.zone_hits,
         level.knockback_units, level.freeze_ms, level.bounces, level.bounce_pct,
         level.bounce_reach_units, level.freeze_from_wave, level.blind_ms, level.curse_ms,
         level.cone_half_steps, level.block_hp, level.stance_bonus);
}

void PutBehaviour(FnvDigest& digest, const FoeBehaviour& behaviour) {
  PutAll(digest, behaviour.move, behaviour.traits, behaviour.pack, behaviour.period_ms,
         behaviour.telegraph_ms, behaviour.active_ms, behaviour.reach_units,
         behaviour.dash_units_per_s, behaviour.shot_damage, behaviour.shot_units_per_s,
         behaviour.shot_radius_units, behaviour.puddle_damage, behaviour.puddle_radius_units,
         behaviour.puddle_ms, behaviour.puddle_period_ms);
}

void PutWave(FnvDigest& digest, const WaveRule& wave) {
  PutAll(digest, wave.from_ms, wave.floor, wave.interval_ms, wave.batch);
  PutNumbers(digest, wave.foe_ids);
  PutNumbers(digest, wave.weights);
}

void PutBoss(FnvDigest& digest, const BossRule& boss) {
  PutAll(digest, boss.at_ms, boss.foe_id, boss.final, boss.walk_ms_per_cell, boss.escort_foe_id,
         boss.escort_count);
  PutEach(digest, boss.moves, [&](const BossMoveRule& move) {
    PutAll(digest, move.kind, move.from_hp_pct, move.to_hp_pct, move.first_ms, move.period_ms,
           move.telegraph_ms, move.effect_ms, move.foe_id, move.count, move.reach_units,
           move.radius_units, move.damage, move.boost_pct);
  });
}

void PutEvent(FnvDigest& digest, const EventRule& event) {
  PutAll(digest, event.at_ms, event.kind, event.foe_id, event.count, event.radius_units);
}

void PutContent(FnvDigest& digest, std::span<const WaveRule> waves,
                std::span<const BossRule> bosses, std::span<const EventRule> events) {
  PutEach(digest, waves, [&](const WaveRule& wave) { PutWave(digest, wave); });
  PutEach(digest, bosses, [&](const BossRule& boss) { PutBoss(digest, boss); });
  PutEach(digest, events, [&](const EventRule& event) { PutEvent(digest, event); });
}

void PutClass(FnvDigest& digest, const ClassRule& rule) {
  PutAll(digest, rule.class_id, rule.max_hp, rule.walk_ms_per_cell, rule.magnet_units,
         rule.damage_pct_per_level, rule.damage_pct_cap, rule.mount_card_item_id, rule.reborn_level,
         rule.reborn_damage_pct, rule.melee);
  PutNumbers(digest, rule.starter_skills);
  PutNumbers(digest, rule.skill_pool);
  PutNumbers(digest, rule.card_pool);
}

void PutFoe(FnvDigest& digest, const FoeRule& rule) {
  PutAll(digest, rule.foe_id, rule.ro_level, rule.rank, rule.size, rule.element, rule.hp,
         rule.ro_walk_ms_per_cell, rule.touch_damage, rule.touch_interval_ms, rule.exp,
         rule.card_item_id, rule.armor_pct);
  PutBehaviour(digest, rule.behaviour);
}

void PutArena(FnvDigest& digest, const ArenaRule& arena) {
  PutAll(digest, arena.arena_id, arena.terrain_id, arena.ranked, arena.start.has_value());
  PutText(digest, arena.map_name);
  if (arena.start.has_value()) PutAll(digest, arena.start->x, arena.start->y);
  PutContent(digest, arena.waves, arena.bosses, arena.events);
}

void PutTerrain(FnvDigest& digest, const TerrainRule& terrain) {
  PutAll(digest, terrain.terrain_id, terrain.density_min_per_mille, terrain.density_max_per_mille,
         terrain.density_period_chunks, terrain.clearing_cells, terrain.landmark_period_chunks,
         terrain.path_period_chunks, terrain.path_width_cells, terrain.path_wiggle_cells);
  PutEach(digest, terrain.pieces, [&](const TerrainPieceRule& piece) {
    PutAll(digest, piece.piece_id, piece.weight, piece.landmark, piece.walkable);
    PutEach(digest, piece.footprint, [&](std::string_view row) { PutText(digest, row); });
  });
}

void PutCaps(FnvDigest& digest, const SurvivorsCaps& caps) {
  PutAll(digest, caps.most_foes, caps.most_shots, caps.most_strikes, caps.most_gems,
         caps.skill_slots, caps.card_slots, caps.offers, caps.most_blasts, caps.most_zones,
         caps.most_foe_shots, caps.most_hazards, caps.most_pickups);
}

void PutTuning(FnvDigest& digest, const SurvivorsTuning& tuning) {
  PutAll(digest, tuning.foe_walk_scale_pct, tuning.foe_hp_growth_pct_per_minute,
         tuning.chill_slow_pct, tuning.boss_chill_share_pct, tuning.hero_radius_units,
         tuning.touch_reach_units, tuning.direct_chase_units, tuning.recall_units,
         tuning.flow_radius_cells, tuning.flow_refresh_steps, tuning.separation_max_push_units,
         tuning.hero_shove_pct, tuning.gem_pull_start_units, tuning.gem_pull_accel_units,
         tuning.offer_weight_owned, tuning.offer_weight_fresh, tuning.heal_offer_hp,
         tuning.zeny_offer, tuning.most_floor_spawns_per_step, tuning.spawn_tries_per_foe,
         tuning.spawn_search_ring, tuning.most_cooldown_pct, tuning.boss_hp_growth_pct_per_minute,
         tuning.boss_walk_scale_pct, tuning.knock_units_per_step, tuning.plant_block_cells,
         tuning.plant_cells_per_plant, tuning.plant_kills_per_minute, tuning.plant_start_count,
         tuning.plant_start_cells, tuning.most_live_plants, tuning.plant_forget_units,
         tuning.pickup_radius_units, tuning.final_overtime_ms, tuning.curse_slow_pct,
         tuning.curse_hurt_pct, tuning.blind_turn_ms);
  PutNumbers(digest, std::span<const std::int32_t>(tuning.foe_radius_units));
}

}  // namespace

std::uint64_t SurvivorsRulesDigest(const SurvivorsRules& rules) {
  FnvDigest digest;
  PutAll(digest, rules.ruleset_version, rules.run_ms);
  PutEach(digest, rules.classes, [&](const ClassRule& rule) { PutClass(digest, rule); });
  PutEach(digest, rules.skills, [&](const SurvivorsSkillRule& rule) {
    PutAll(digest, rule.skill_id, rule.archetype, rule.target, rule.element, rule.stance_stat);
    PutEach(digest, rule.levels, [&](const SkillLevel& level) { PutSkillLevel(digest, level); });
  });
  PutEach(digest, rules.cards, [&](const CardRule& rule) {
    PutAll(digest, rule.card_item_id, rule.stat);
    PutNumbers(digest, rule.per_level);
  });
  PutEach(digest, rules.foes, [&](const FoeRule& rule) { PutFoe(digest, rule); });
  PutContent(digest, rules.waves, rules.bosses, rules.events);
  PutEach(digest, rules.arenas, [&](const ArenaRule& arena) { PutArena(digest, arena); });
  PutEach(digest, rules.exp_curve, [&](const ExpSegment& segment) {
    PutAll(digest, segment.from_level, segment.base, segment.per_level);
  });
  PutEach(digest, rules.elements,
          [&](const ElementRow& row) { PutNumbers(digest, std::span<const std::int16_t>(row)); });
  PutAll(digest, rules.spawn_frame.half_width_units, rules.spawn_frame.north_units,
         rules.spawn_frame.south_units, rules.spawn_frame.margin_units);
  PutCaps(digest, rules.caps);
  PutTuning(digest, rules.tuning);
  PutEach(digest, rules.evolutions, [&](const EvolutionRule& rule) {
    PutAll(digest, rule.skill_id, rule.card_item_id, rule.evolved_skill_id, rule.partner_skill_id);
  });
  PutEach(digest, rules.plants,
          [&](const PlantRule& rule) { PutAll(digest, rule.foe_id, rule.pickup, rule.weight); });
  PutEach(digest, rules.pickups, [&](const PickupRule& rule) {
    PutAll(digest, rule.kind, rule.item_id, rule.amount, rule.duration_ms, rule.reach_units);
  });
  PutAll(digest, rules.elites.first_ms, rules.elites.interval_ms, rules.elites.last_ms,
         rules.elites.hp_pct, rules.elites.touch_pct, rules.elites.exp_pct);
  PutAll(digest, rules.chests.item_id, rules.chests.evolution_from_ms,
         rules.chests.triple_per_10000, rules.chests.quintuple_per_10000, rules.chests.single,
         rules.chests.triple, rules.chests.quintuple);
  PutAll(digest, rules.card_drops.common_odds);
  PutEach(digest, rules.cursed_water,
          [&](const CursedWaterRank& rank) { PutAll(digest, rank.foe_hp_pct, rank.touch_pct); });
  PutAll(digest, rules.revive.hp_pct, rules.revive.invulnerable_ms, rules.revive.push_units);
  PutEach(digest, rules.terrains, [&](const TerrainRule& terrain) { PutTerrain(digest, terrain); });
  // Les règles d'équipe, écrites en constantes.
  PutAll(digest, kTeamLeashCells, kMostTeamCharges, kVoteTicks);
  for (const SurvivorsTeamScale& scale : kTeamScales) {
    PutAll(digest, scale.exp_needed_pct, scale.foe_hp_pct, scale.batch_pct);
  }
  return digest.value();
}

}  // namespace ro::sim
