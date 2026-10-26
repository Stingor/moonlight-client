#include "ro/sim/survivors_rules.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

#include "survivors_shipped.h"

namespace ro::sim {

namespace {

// ── LE RULESET 3 : la première arène complète, quinze minutes et Baphomet ───
// Il porte aussi ce que la partie interprète de la méta : la chute des cartes,
// la Cursed Water et la relève.
constexpr std::uint32_t kShippedRunMs = 900000;
constexpr SpawnFrame kShippedSpawnFrame{
    .half_width_units = Cells(30),
    .north_units = Cells(20),
    .south_units = Cells(17),
    .margin_units = Cells(3),
};
constexpr SurvivorsCaps kShippedCaps{
    .most_foes = 400,
    .most_shots = 256,
    .most_strikes = 512,
    .most_gems = 400,
    .skill_slots = 6,
    .card_slots = 6,
    .offers = 3,
};
constexpr SurvivorsTuning kShippedTuning{
    .foe_walk_scale_pct = 180,
    .foe_hp_growth_pct_per_minute = 4,
    .chill_slow_pct = 30,
    .boss_chill_share_pct = 50,
    .hero_radius_units = CellTenths(4),
    .foe_radius_units = {358, 460, 716},
    .touch_reach_units = 128,
    .direct_chase_units = Cells(3),
    .recall_units = Cells(48),
    .flow_radius_cells = 44,
    .flow_refresh_steps = 5,
    .separation_max_push_units = 128,
    .hero_shove_pct = 20,
    .gem_pull_start_units = 64,
    .gem_pull_accel_units = 16,
    .offer_weight_owned = 100,
    .offer_weight_fresh = 25,
    .heal_offer_hp = 30,
    .zeny_offer = 25,
    .most_floor_spawns_per_step = 10,
    .spawn_tries_per_foe = 4,
    .spawn_search_ring = 4,
    .most_cooldown_pct = 60,
    // Les boss gardent leurs PV et leur propre marche : seule la horde croît
    // et ralentit.
    .boss_hp_growth_pct_per_minute = 0,
    .boss_walk_scale_pct = 100,
    .knock_units_per_step = CellTenths(2),
    .plant_block_cells = 12,
    .plant_cells_per_plant = 150,
    .pickup_radius_units = CellTenths(5),
    // Baphomet jamais tué : la partie finit en survie cinq minutes après lui.
    .final_overtime_ms = 300000,
};

// LA CURSED WATER : par rang, PV de naissance des monstres +20 % et contact
// +10 %. Le score et les Zeny d'une partie libre suivent le rang ailleurs
// (FreeRunScore, carnet).
constexpr std::int32_t kCursedFoeHpPctPerRank = 20;
constexpr std::int32_t kCursedTouchPctPerRank = 10;
[[nodiscard]] constexpr CursedWaterRank CursedRank(std::int32_t rank) noexcept {
  return CursedWaterRank{.foe_hp_pct = kCursedFoeHpPctPerRank * rank,
                         .touch_pct = kCursedTouchPctPerRank * rank};
}
constexpr std::array<CursedWaterRank, kMostCursedWater + 1> kShippedCursedWater{
    {CursedRank(0), CursedRank(1), CursedRank(2), CursedRank(3), CursedRank(4), CursedRank(5)}};

// Sans table, une attaque ne gagne ni ne perd rien.
constexpr std::int32_t kNeutralElementPct = 100;
constexpr std::uint32_t kNoLevelUp = std::numeric_limits<std::uint32_t>::max();

// Le premier élément de `table` dont `id_of` vaut `id`, ou nullptr.
template <typename Rule, typename IdOf>
[[nodiscard]] const Rule* FindById(std::span<const Rule> table, std::uint16_t id, IdOf id_of) {
  const auto found =
      std::find_if(table.begin(), table.end(), [&](const Rule& rule) { return id_of(rule) == id; });
  return found == table.end() ? nullptr : &*found;
}

}  // namespace

const SurvivorsRules& ShippedSurvivorsRules() {
  // Une table IMMUABLE, assemblée au premier appel : pas un contexte.
  static const SurvivorsRules kRules{
      .ruleset_version = kSurvivorsRulesetVersion,
      .run_ms = kShippedRunMs,
      .classes = survivors::ShippedClasses(),
      .skills = survivors::ShippedSkills(),
      .cards = survivors::ShippedCards(),
      .foes = survivors::ShippedFoes(),
      .waves = survivors::ShippedWaves(),
      .bosses = survivors::ShippedBosses(),
      .arenas = survivors::ShippedArenas(),
      .exp_curve = survivors::ShippedExpCurve(),
      .elements = survivors::ShippedElements(),
      .spawn_frame = kShippedSpawnFrame,
      .caps = kShippedCaps,
      .tuning = kShippedTuning,
      .events = survivors::ShippedEvents(),
      .evolutions = survivors::ShippedEvolutions(),
      .plants = survivors::ShippedPlants(),
      .pickups = survivors::ShippedPickups(),
      .elites = survivors::ShippedElites(),
      .chests = survivors::ShippedChest(),
      .card_drops = CardDropRule{.common_odds = kCommonCardOdds},
      .cursed_water = kShippedCursedWater,
      .revive = ReviveRule{.hp_pct = kReviveHpPct,
                           .invulnerable_ms = kReviveInvulnerableMs,
                           .push_units = Cells(kRevivePushCells)},
  };
  return kRules;
}

const SurvivorsSkillRule* FindSkill(const SurvivorsRules& rules, std::uint16_t skill_id) {
  return FindById(rules.skills, skill_id,
                  [](const SurvivorsSkillRule& rule) { return rule.skill_id; });
}

const CardRule* FindCard(const SurvivorsRules& rules, std::uint16_t card_item_id) {
  return FindById(rules.cards, card_item_id,
                  [](const CardRule& rule) { return rule.card_item_id; });
}

const FoeRule* FindFoe(const SurvivorsRules& rules, std::uint16_t foe_id) {
  return FindById(rules.foes, foe_id, [](const FoeRule& rule) { return rule.foe_id; });
}

const ClassRule* FindClass(const SurvivorsRules& rules, std::uint16_t class_id) {
  return FindById(rules.classes, class_id, [](const ClassRule& rule) { return rule.class_id; });
}

const ArenaRule* FindArena(const SurvivorsRules& rules, std::uint16_t arena_id) {
  return FindById(rules.arenas, arena_id, [](const ArenaRule& rule) { return rule.arena_id; });
}

const TerrainRule* FindTerrain(const SurvivorsRules& rules, std::uint16_t terrain_id) {
  return FindById(rules.terrains, terrain_id,
                  [](const TerrainRule& rule) { return rule.terrain_id; });
}

const EvolutionRule* FindEvolutionOf(const SurvivorsRules& rules, std::uint16_t skill_id) {
  return FindById(rules.evolutions, skill_id,
                  [](const EvolutionRule& rule) { return rule.skill_id; });
}

const EvolutionRule* FindEvolutionTo(const SurvivorsRules& rules, std::uint16_t evolved_skill_id) {
  return FindById(rules.evolutions, evolved_skill_id,
                  [](const EvolutionRule& rule) { return rule.evolved_skill_id; });
}

const PickupRule* FindPickup(const SurvivorsRules& rules, PickupKind kind) {
  const auto found = std::find_if(rules.pickups.begin(), rules.pickups.end(),
                                  [kind](const PickupRule& rule) { return rule.kind == kind; });
  return found == rules.pickups.end() ? nullptr : &*found;
}

bool HasFinalBoss(const SurvivorsRules& rules) {
  return std::any_of(rules.bosses.begin(), rules.bosses.end(),
                     [](const BossRule& boss) { return boss.final; });
}

std::uint32_t ExpToNext(const SurvivorsRules& rules, std::uint16_t level) {
  const ExpSegment* segment = nullptr;
  for (const ExpSegment& candidate : rules.exp_curve) {
    if (candidate.from_level <= level) segment = &candidate;
  }
  if (segment == nullptr || level == 0) return kNoLevelUp;
  const std::uint64_t needed =
      std::uint64_t{segment->base} +
      std::uint64_t{segment->per_level} * static_cast<std::uint64_t>(level - segment->from_level);
  return static_cast<std::uint32_t>(std::clamp<std::uint64_t>(needed, 1, kNoLevelUp));
}

std::int32_t ElementPct(const SurvivorsRules& rules, SurvivorsElement attack,
                        SurvivorsElement defence) {
  const auto row = static_cast<std::size_t>(attack);
  const auto column = static_cast<std::size_t>(defence);
  if (rules.elements.size() != kSurvivorsElementCount || row >= kSurvivorsElementCount ||
      column >= kSurvivorsElementCount) {
    return kNeutralElementPct;
  }
  return std::max<std::int32_t>(rules.elements[row][column], 0);
}

}  // namespace ro::sim
