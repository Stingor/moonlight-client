#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>

#include "ro/sim/packed_position.h"
#include "ro/sim/survivors_geometry.h"
#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_run.h"
#include "ro/sim/survivors_state.h"
#include "ro/sim/walk_field.h"

#include "survivors_space.h"
#include "survivors_systems.h"

namespace ro::sim::survivors {

namespace {

constexpr std::int64_t kMostFoeHp = std::numeric_limits<std::int32_t>::max();

// `hp` × `factor_pct` / 100, dans [1, INT32_MAX], saturé AVANT la
// multiplication : le produit ne quitte jamais les 64 bits. Un facteur nul ou
// négatif laisse un PV.
[[nodiscard]] std::int32_t PercentOfHp(std::int64_t hp, std::int64_t factor_pct) noexcept {
  const std::int64_t base = std::max<std::int64_t>(hp, 1);
  std::int64_t scaled = 1;
  if (factor_pct > kMostFoeHp * kPercent / base) {
    scaled = kMostFoeHp;
  } else if (factor_pct > 0) {
    scaled = base * factor_pct / kPercent;
  }
  return static_cast<std::int32_t>(std::clamp<std::int64_t>(scaled, 1, kMostFoeHp));
}

// La marche d'un monstre qui naît, en ms par case : celle de son BossRule s'il
// en pose une, telle quelle ; sinon celle du mob_db, à l'échelle des boss pour
// un mini-boss ou un boss, à celle de la horde pour un commun.
[[nodiscard]] std::uint32_t WalkMsOf(const SurvivorsTuning& tuning, const FoeRule& rule,
                                     const HatchOptions& options) noexcept {
  if (options.boss != nullptr && options.boss->walk_ms_per_cell != 0) {
    return options.boss->walk_ms_per_cell;
  }
  const std::int32_t scale_pct =
      rule.rank != FoeRank::kCommon ? tuning.boss_walk_scale_pct : tuning.foe_walk_scale_pct;
  const auto walk_scale_pct = static_cast<std::uint64_t>(std::max<std::int32_t>(scale_pct, 0));
  const std::uint64_t walk_ms = std::uint64_t{rule.ro_walk_ms_per_cell} * walk_scale_pct / kPercent;
  return static_cast<std::uint32_t>(
      std::min<std::uint64_t>(walk_ms, std::numeric_limits<std::uint32_t>::max()));
}

// L'issue des naissances d'une vague : les tirages nés, et si une naissance a
// échoué (aucune place, règle incohérente).
struct WaveHatch {
  std::size_t born = 0;
  bool failed = false;
};

// Jusqu'à `count` tirages de la vague, dans la limite de `room` monstres :
// l'espèce par les poids, puis la place, puis la meute entière sur ce point,
// tronquée à la place qui reste. Un tirage compte pour un, quelle que soit sa
// meute. Les apparitions de ce pas s'arrêtent à la première naissance qui
// échoue, et reprennent au pas suivant.
[[nodiscard]] WaveHatch HatchFromWave(SurvivorsField& field, const WaveRule& wave,
                                      std::size_t count, std::size_t room) {
  WaveHatch hatch;
  std::size_t hatched = 0;
  for (; hatch.born < count && hatched < room; ++hatch.born) {
    hatch.failed = true;
    if (wave.weights.size() != wave.foe_ids.size()) break;
    const std::size_t pick = field.spawn_random.PickWeighted(wave.weights);
    if (pick >= wave.foe_ids.size()) break;
    const FoeRule* rule = FindFoe(*field.rules, wave.foe_ids[pick]);
    if (rule == nullptr) break;
    const std::optional<FieldPoint> at = DrawSpawnPoint(field);
    if (!at) break;
    hatch.failed = false;
    const std::size_t pack =
        std::min<std::size_t>(std::max<std::uint8_t>(rule->behaviour.pack, 1), room - hatched);
    for (std::size_t member = 0; member < pack; ++member) (void)HatchFoe(field, *rule, *at);
    hatched += pack;
  }
  return hatch;
}

}  // namespace

FieldPoint PerimeterPoint(const SpawnFrame& frame, FieldPoint hero, std::int64_t along,
                          std::int32_t outward) noexcept {
  const std::int64_t width = std::int64_t{frame.half_width_units} * 2;
  const std::int64_t height = std::int64_t{frame.north_units} + frame.south_units;
  const std::int64_t west = std::int64_t{hero.x} - frame.half_width_units;
  const std::int64_t east = std::int64_t{hero.x} + frame.half_width_units;
  const std::int64_t north = std::int64_t{hero.y} + frame.north_units;
  const std::int64_t south = std::int64_t{hero.y} - frame.south_units;
  std::int64_t x = 0;
  std::int64_t y = 0;
  if (along < width) {
    x = west + along;
    y = north + outward;
  } else if (along < width + height) {
    x = east + outward;
    y = north - (along - width);
  } else if (along < 2 * width + height) {
    x = east - (along - width - height);
    y = south - outward;
  } else {
    x = west - outward;
    y = south + (along - 2 * width - height);
  }
  return {static_cast<std::int32_t>(x), static_cast<std::int32_t>(y)};
}

std::optional<FieldPoint> DrawSpawnPoint(SurvivorsField& field) {
  const SurvivorsRules& rules = *field.rules;
  const SpawnFrame& frame = rules.spawn_frame;
  const std::int64_t perimeter =
      (std::int64_t{frame.half_width_units} * 2 + frame.north_units + frame.south_units) * 2;
  if (perimeter <= 0 || perimeter > std::numeric_limits<std::uint32_t>::max() ||
      frame.margin_units < 0) {
    return std::nullopt;
  }
  const FieldPoint hero = field.hero.position;
  for (std::uint8_t attempt = 0; attempt < rules.tuning.spawn_tries_per_foe; ++attempt) {
    const std::uint32_t along = field.spawn_random.Below(static_cast<std::uint32_t>(perimeter));
    const auto outward = static_cast<std::int32_t>(
        field.spawn_random.Below(static_cast<std::uint32_t>(frame.margin_units) + 1U));
    const FieldPoint point = PerimeterPoint(frame, hero, along, outward);
    const std::optional<CellPosition> cell = NearestPassable(
        field.arena, CellOf(point.x), CellOf(point.y), rules.tuning.spawn_search_ring);
    if (!cell) continue;
    const FieldPoint landing = CellCenter(cell->x, cell->y);
    if (InsideSpawnFrame(frame, hero, landing) || Stranded(field.flow, landing)) continue;
    return landing;
  }
  return std::nullopt;
}

std::uint32_t HatchFoe(SurvivorsField& field, const FoeRule& rule, FieldPoint at,
                       const HatchOptions& options) {
  const SurvivorsRules& rules = *field.rules;
  const SurvivorsTuning& tuning = rules.tuning;
  const std::int64_t minutes = field.step / kStepsPerMinute;
  const std::int32_t growth_pct = rule.rank != FoeRank::kCommon
                                      ? tuning.boss_hp_growth_pct_per_minute
                                      : tuning.foe_hp_growth_pct_per_minute;
  std::int32_t hp = PercentOfHp(rule.hp, kPercent + std::int64_t{growth_pct} * minutes);
  if (options.elite) hp = PercentOfHp(hp, rules.elites.hp_pct);
  // La Cursed Water du départ majore les PV après la croissance et l'élite ;
  // le rang 0 n'y touche pas.
  if (const std::uint8_t cursed = field.start.cursed_water;
      cursed > 0 && cursed < rules.cursed_water.size()) {
    hp = PercentOfHp(hp, kPercent + std::int64_t{rules.cursed_water[cursed].foe_hp_pct});
  }

  SurvivorsFoe foe;
  foe.serial = field.next_serial++;
  foe.foe_id = rule.foe_id;
  foe.position = at;
  foe.previous_position = at;
  foe.hp = hp;
  foe.max_hp = hp;
  foe.units_per_step = UnitsPerStep(WalkMsOf(tuning, rule, options));
  foe.born_step = field.step;
  foe.touch_ready_step = field.step;
  foe.facing = HeadingOf(field.hero.position - at, 0);
  foe.drift_heading = options.drift_heading;
  foe.elite = options.elite;
  field.foes.push_back(foe);
  return foe.serial;
}

void SpawnFoes(SurvivorsField& field) {
  const SurvivorsRules& rules = *field.rules;
  const std::uint64_t elapsed_ms = std::uint64_t{field.step} * kSurvivorsStepMs;

  if (!rules.waves.empty()) {
    while (field.wave_index + 1 < rules.waves.size() &&
           rules.waves[field.wave_index + 1].from_ms <= elapsed_ms) {
      ++field.wave_index;
    }
    field.wave_index = std::min(field.wave_index, rules.waves.size() - 1);
    const WaveRule& wave = rules.waves[field.wave_index];
    const std::size_t most_foes = rules.caps.most_foes;
    bool hatching = true;
    // Le lot de la vague, à chaque intervalle, dans la limite du plafond. Un lot
    // qu'une place introuvable interrompt n'arme pas l'intervalle suivant : il
    // reprend au pas suivant pour son SEUL reste, compté quelle que soit la
    // vague en vigueur. Un lot que le plafond réduit est accompli.
    if (field.step - field.wave_spawn_step >= StepsOf(wave.interval_ms)) {
      const std::size_t owed =
          wave.batch - std::min<std::size_t>(wave.batch, field.wave_batch_born);
      const std::size_t room = most_foes - std::min(most_foes, field.foes.size());
      const std::size_t wanted = std::min(owed, room);
      const WaveHatch hatch = HatchFromWave(field, wave, wanted, room);
      hatching = !hatch.failed;
      if (hatching) {
        field.wave_spawn_step = field.step;
        field.wave_batch_born = 0;
      } else {
        // born < wanted <= batch : le compte tient dans l'octet.
        field.wave_batch_born = static_cast<std::uint8_t>(field.wave_batch_born + hatch.born);
      }
    }
    // Sous le plancher, des naissances de plus, bornées par pas.
    const std::size_t alive = field.foes.size();
    if (hatching && alive < wave.floor) {
      const std::size_t room = most_foes - std::min(most_foes, alive);
      const std::size_t missing = std::min<std::size_t>(wave.floor - alive, room);
      (void)HatchFromWave(field, wave,
                          std::min<std::size_t>(missing, rules.tuning.most_floor_spawns_per_step),
                          room);
    }
  }

  // Un boss naît à son heure, hors plafond ; sans place ce pas, il réessaie au
  // suivant.
  while (field.next_boss < rules.bosses.size() &&
         rules.bosses[field.next_boss].at_ms <= elapsed_ms) {
    const BossRule& boss = rules.bosses[field.next_boss];
    const FoeRule* rule = FindFoe(rules, boss.foe_id);
    if (rule == nullptr) {
      ++field.next_boss;
      continue;
    }
    const std::optional<FieldPoint> at = DrawSpawnPoint(field);
    if (!at) return;
    const std::uint32_t serial = HatchFoe(field, *rule, *at, HatchOptions{.boss = &boss});
    field.news.boss_arrivals.push_back(serial);
    TrackBoss(field, serial, field.next_boss);
    ++field.next_boss;
  }
}

}  // namespace ro::sim::survivors
