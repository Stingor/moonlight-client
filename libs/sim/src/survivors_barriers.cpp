#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>

#include "ro/sim/survivors_geometry.h"
#include "ro/sim/survivors_news.h"
#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_run.h"
#include "ro/sim/survivors_state.h"

#include "survivors_space.h"
#include "survivors_systems.h"
#include "survivors_volley.h"

namespace ro::sim::survivors {

// LA BARRIÈRE : une rangée de blocs posée entre le héros et la foule la plus
// dense, perpendiculaire à la direction de cette foule. Chaque bloc est une
// zone, qui ne frappe rien : la horde ne le traverse pas, glisse le long de
// son bord, ou le frappe et gèle. Un bloc éclate à zéro PV ou à la fin de sa
// vie. Le héros passe au travers : une barrière ne l'enferme jamais ; un
// monstre qui passe les murs aussi.

namespace {

inline constexpr std::uint8_t kQuarterTurn = kHeadingSteps / 4;

[[nodiscard]] std::uint8_t Turned(std::uint8_t heading, std::uint32_t turns) noexcept {
  return static_cast<std::uint8_t>((std::uint32_t{heading} + turns) % kHeadingSteps);
}

[[nodiscard]] bool IsBlock(const SurvivorsField& field, const SurvivorsZone& zone) noexcept {
  return zone.archetype == SkillArchetype::kBarrier && zone.block_hp > 0 &&
         field.step < zone.expires_step;
}

// Le point du bord du bloc, de rayon `reach`, sur la droite qui va de son
// centre à `at` ; à défaut de direction (au centre même), au cap `fallback`.
[[nodiscard]] FieldPoint EdgeToward(FieldPoint center, FieldPoint at, std::int64_t reach,
                                    std::uint8_t fallback) noexcept {
  const FieldPoint away = at - center;
  const std::uint64_t length = IntegerSqrt(static_cast<std::uint64_t>(DistanceSquared(at, center)));
  if (length == 0) return center + TurnVector(fallback, ClampedUnits(reach));
  return FieldPoint{static_cast<std::int32_t>(center.x + std::int64_t{away.x} * reach /
                                                             static_cast<std::int64_t>(length)),
                    static_cast<std::int32_t>(center.y + std::int64_t{away.y} * reach /
                                                             static_cast<std::int64_t>(length))};
}

// Un monstre contre un bloc : un boss le brise ; un autre en est repoussé
// sur son bord, puis le frappe s'il peut frapper, et gèle.
void Hold(SurvivorsField& field, SurvivorsFoe& foe, SurvivorsZone& block) {
  const SurvivorsRules& rules = *field.rules;
  if ((TraitsOf(rules, foe) & kFoePassesWalls) != 0) return;
  const std::int64_t reach = std::int64_t{block.radius_units} + FoeRadius(rules, foe);
  if (DistanceSquared(foe.position, block.from) >= reach * reach) return;
  if (IsBoss(rules, foe)) {
    block.block_hp = 0;
    return;
  }
  const FieldPoint edge =
      EdgeToward(block.from, foe.position, reach,
                 HeadingOf(foe.previous_position - block.from, field.hero.facing));
  foe.position = Slide(field.ground, foe.position, edge - foe.position);
  if (field.step < foe.touch_ready_step) return;
  const FoeRule* rule = FoeRuleOf(rules, foe);
  if (rule == nullptr) return;
  block.block_hp -= std::max(rule->touch_damage, 1);
  foe.touch_ready_step = field.step + std::max<std::uint32_t>(StepsOf(rule->touch_interval_ms), 1);
  static_cast<void>(ApplyStatus(field, foe, SurvivorsStatus{.freeze_ms = block.freeze_ms}));
}

}  // namespace

// LA BARRIÈRE : la foule la plus dense à `reach_units` du héros (densité dans
// `area_radius_units`) donne la direction ; `amount` blocs de rayon
// `hit_radius_units`, côte à côte, centrés à `length_units` du héros dans
// cette direction et rangés perpendiculairement à elle. Elle attend une foule
// à portée et sa place sous le plafond des zones : faute de place pour tous,
// les blocs du milieu passent d'abord.
bool FireBarrier(SurvivorsField& field, std::uint8_t slot, const SurvivorsSkillRule& rule,
                 const SkillLevel& level) {
  const std::uint32_t blocks = AmountOf(field, level);
  const std::size_t count =
      std::min<std::size_t>(blocks, RoomUnder(field.zones.size(), field.rules->caps.most_zones));
  if (count == 0 || level.hit_radius_units <= 0 || level.block_hp <= 0) return false;
  const FieldPoint hero = field.hero.position;
  GatherNearby(field, hero, level.reach_units);
  if (field.nearby.empty()) return false;
  OrderTargets(field, rule.target, AreaUnits(field, level.area_radius_units));
  const FieldPoint pack = field.foes[field.nearby.front()].position;
  const std::uint8_t heading = HeadingOf(pack - hero, field.hero.facing);
  const FieldPoint middle = hero + TurnVector(heading, level.length_units);
  const std::uint32_t expires_step =
      field.step + ScaledSteps(level.life_ms, field.hero.stats.Of(SurvivorsStat::kDurationPct));
  // Le k-ième bloc, de la rangée de `count`, à `(2k - count + 1) × rayon` du
  // milieu : côte à côte, symétriques.
  for (std::size_t block = 0; block < count; ++block) {
    const std::int64_t offset =
        (2 * static_cast<std::int64_t>(block) - static_cast<std::int64_t>(count) + 1) *
        level.hit_radius_units;
    const std::uint8_t side = Turned(heading, offset >= 0 ? kQuarterTurn : 3 * kQuarterTurn);
    const FieldPoint at = middle + TurnVector(side, ClampedUnits(offset >= 0 ? offset : -offset));
    SurvivorsZone zone{.serial = field.next_serial++,
                       .skill_slot = slot,
                       .skill_id = rule.skill_id,
                       .archetype = SkillArchetype::kBarrier,
                       .from = at,
                       .to = at,
                       .radius_units = level.hit_radius_units,
                       .freeze_ms = level.freeze_ms,
                       .expires_step = expires_step,
                       .push_heading = heading,
                       .element = rule.element,
                       .block_hp = level.block_hp};
    field.zones.push_back(zone);
    PublishCast(field.news, SurvivorsCast{.skill_id = rule.skill_id,
                                          .kind = CastKind::kBlockOn,
                                          .from = at,
                                          .to = at,
                                          .target_serial = zone.serial,
                                          .land_step = 0});
  }
  return true;
}

void HoldBarriers(SurvivorsField& field) {
  if (field.rules == nullptr) return;
  if (std::none_of(field.zones.begin(), field.zones.end(),
                   [&](const SurvivorsZone& zone) { return IsBlock(field, zone); })) {
    return;
  }
  // Par bloc, dans l'ordre de pose ; par monstre, par index croissant. La
  // grille des voisins date d'avant la marche : on parcourt la horde entière,
  // une dizaine de blocs au plus.
  for (SurvivorsZone& block : field.zones) {
    if (!IsBlock(field, block)) continue;
    for (SurvivorsFoe& foe : field.foes) {
      if (foe.hp <= 0 || !Tangible(field, foe)) continue;
      Hold(field, foe, block);
      if (block.block_hp <= 0) break;
    }
  }
}

}  // namespace ro::sim::survivors
