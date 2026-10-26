#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <optional>
#include <span>

#include "ro/sim/packed_position.h"
#include "ro/sim/survivors_geometry.h"
#include "ro/sim/survivors_ground.h"
#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_run.h"
#include "ro/sim/survivors_state.h"

#include "survivors_space.h"
#include "survivors_systems.h"

namespace ro::sim::survivors {

namespace {

// Deux monstres exactement superposés s'écartent selon un cap tiré de leur
// numéro de naissance : 23 est premier avec 64, donc des numéros voisins
// partent dans des directions éloignées, sans aléa.
constexpr std::uint32_t kOverlapHeadingFactor = 23;

// `value` × pct / 100, tronqué, en int64.
[[nodiscard]] std::int32_t PercentOf(std::int64_t value, std::int64_t pct) noexcept {
  return static_cast<std::int32_t>(value * pct / kPercent);
}

// Le cap d'écart d'un monstre exactement superposé à un autre.
[[nodiscard]] std::uint8_t OverlapHeading(std::uint32_t serial) noexcept {
  return static_cast<std::uint8_t>((std::uint64_t{serial} * kOverlapHeadingFactor) % kHeadingSteps);
}

// Le vecteur de `length` qui éloigne `moving` de `still` ; pour deux points
// confondus, le cap `overlap_heading`.
[[nodiscard]] FieldPoint AwayFrom(FieldPoint still, FieldPoint moving, std::int32_t length,
                                  std::uint8_t overlap_heading) noexcept {
  if (still == moving) return TurnVector(overlap_heading, length);
  return Toward(still, moving, length);
}

// `foe_points` et `foe_grid` reflètent `foes`, dans le même ordre : l'index
// d'un point est celui de son monstre. Sur un sol généré, l'aire de la grille
// se centre sur le héros avant chaque reconstruction ; celle d'un sol borné
// reste sur la carte.
void FillFoeGrid(SurvivorsField& field) {
  if (!field.ground.bounded()) {
    constexpr std::int32_t kHalfSide = kFollowingGridCells / 2;
    field.foe_grid.Recenter(CellOf(field.hero.position.x) - kHalfSide,
                            CellOf(field.hero.position.y) - kHalfSide);
  }
  field.foe_points.clear();
  for (const SurvivorsFoe& foe : field.foes) field.foe_points.push_back(foe.position);
  field.foe_grid.Rebuild(field.foe_points);
}

// Les côtés du cadre d'apparition, dans le sens horaire : un quart de tour
// ajoute 1, un demi-tour 2.
enum class FrameSide : std::uint8_t { kNorth, kEast, kSouth, kWest };
constexpr std::uint8_t kFrameSideCount = 4;
// L'ordre des côtés essayés par un rappel, en quarts de tour depuis le côté que
// le héros regarde : ce côté, l'opposé, puis les deux autres.
constexpr std::array<std::uint8_t, kFrameSideCount> kRecallSideTurns = {0, 2, 1, 3};

// Le côté vers lequel regarde le cap `facing` : sa composante dominante, le
// nord ou le sud à égalité.
[[nodiscard]] FrameSide FacedSide(std::uint8_t facing) noexcept {
  const FieldPoint ahead = TurnVector(facing, kSubunitsPerCell);
  if (std::abs(ahead.x) > std::abs(ahead.y))
    return ahead.x > 0 ? FrameSide::kEast : FrameSide::kWest;
  return ahead.y >= 0 ? FrameSide::kNorth : FrameSide::kSouth;
}

// Le point de rappel sur le côté `side`, à l'extérieur de la marge, au décalage
// latéral `offset` borné à la longueur du côté.
[[nodiscard]] FieldPoint RecallTarget(const SpawnFrame& frame, FieldPoint hero, FieldPoint offset,
                                      FrameSide side) noexcept {
  FieldPoint target = hero;
  switch (side) {
    case FrameSide::kNorth:
      target.y += frame.north_units + frame.margin_units;
      target.x += std::clamp(offset.x, -frame.half_width_units, frame.half_width_units);
      break;
    case FrameSide::kSouth:
      target.y -= frame.south_units + frame.margin_units;
      target.x += std::clamp(offset.x, -frame.half_width_units, frame.half_width_units);
      break;
    case FrameSide::kEast:
      target.x += frame.half_width_units + frame.margin_units;
      target.y += std::clamp(offset.y, -frame.south_units, frame.north_units);
      break;
    case FrameSide::kWest:
      target.x -= frame.half_width_units + frame.margin_units;
      target.y += std::clamp(offset.y, -frame.south_units, frame.north_units);
      break;
  }
  return target;
}

// LE RAPPEL : un monstre, de tout rang, renaît sur le côté du cadre
// d'apparition vers lequel le héros regarde, à l'extérieur de la marge, en
// gardant son décalage latéral borné au côté. Un côté sans case praticable
// proche, dont la case retombe dans le cadre (le héros collé au bord de
// l'arène) ou que le champ tient pour une impasse, cède à l'opposé, puis aux
// deux autres dans le sens horaire. Aucun côté possible : le monstre reste où
// il est et réessaie au pas suivant.
void Recall(SurvivorsField& field, SurvivorsFoe& foe) {
  const SurvivorsRules& rules = *field.rules;
  const SpawnFrame& frame = rules.spawn_frame;
  const FieldPoint hero = field.hero.position;
  const FieldPoint offset = foe.position - hero;
  const auto faced = static_cast<std::uint8_t>(FacedSide(field.hero.facing));
  for (const std::uint8_t turns : kRecallSideTurns) {
    const auto side = static_cast<FrameSide>((faced + turns) % kFrameSideCount);
    const FieldPoint target = RecallTarget(frame, hero, offset, side);
    const std::optional<CellPosition> cell = NearestPassable(
        field.ground, CellOf(target.x), CellOf(target.y), rules.tuning.spawn_search_ring);
    if (!cell) continue;
    const FieldPoint landing = CellCenter(cell->x, cell->y);
    if (InsideSpawnFrame(frame, hero, landing) || Stranded(field.flow, landing)) continue;
    foe.position = landing;
    foe.previous_position = landing;
    return;
  }
}

// Vrai si le champ mène de la case (x, y) à sa source sans détour : son coût
// vaut la marche octile, aucun mur ne s'interpose sur le plus court chemin.
[[nodiscard]] bool OpenGround(const HordeFlow& flow, std::int32_t cell_x, std::int32_t cell_y) {
  const std::optional<CellPosition> source = flow.source();
  const std::optional<std::uint32_t> cost = flow.CostFrom(cell_x, cell_y);
  return source && cost && *cost == OctileCost(cell_x, cell_y, source->x, source->y);
}

// LA POURSUITE d'un monstre. Le champ le mène de case en case, vers le centre
// de la case qu'il rend. Le monstre va droit sur le héros quand le champ ne
// mène nulle part (sur sa source, hors de sa fenêtre), ou de près quand le
// champ ne fait aucun détour ; un pas droit qu'un mur arrête cède alors au pas
// du champ. Jamais au-delà du but. Vrai quand le monstre, hors de la fenêtre
// du champ, a buté contre un mur : rien ne peut le guider, et MoveHorde le
// rappelle. Un monstre gelé ne bouge pas. Un monstre qui traverse les murs va
// toujours droit sur le héros, sans que rien ne l'arrête.
[[nodiscard]] bool Chase(SurvivorsField& field, SurvivorsFoe& foe) {
  if (!Walks(field, foe)) return false;
  const SurvivorsTuning& tuning = field.rules->tuning;
  const FieldPoint hero = field.hero.position;
  const std::int32_t speed = PaceOf(field, foe);
  if ((TraitsOf(*field.rules, foe) & kFoePassesWalls) != 0) {
    const FieldPoint delta = StepToward(foe.position, hero, speed);
    foe.position = Glide(field.ground, foe.position, delta);
    foe.facing = HeadingOf(delta, foe.facing);
    return false;
  }
  const std::int32_t cell_x = CellOf(foe.position.x);
  const std::int32_t cell_y = CellOf(foe.position.y);
  const std::optional<CellPosition> next = field.flow.NextCellFrom(cell_x, cell_y);
  const std::int64_t direct = tuning.direct_chase_units;
  const bool straight = !next || (DistanceSquared(foe.position, hero) <= direct * direct &&
                                  OpenGround(field.flow, cell_x, cell_y));
  const FieldPoint from = foe.position;
  FieldPoint delta = StepToward(from, straight ? hero : CellCenter(next->x, next->y), speed);
  FieldPoint at = Slide(field.ground, from, delta);
  const bool blocked = SlideBlocked(from, delta, at);
  if (straight && blocked && next) {
    delta = StepToward(from, CellCenter(next->x, next->y), speed);
    at = Slide(field.ground, from, delta);
  }
  foe.position = at;
  foe.facing = HeadingOf(delta, foe.facing);
  return straight && blocked && !next && !field.flow.Covers(cell_x, cell_y);
}

// LE RECUL : le monstre glisse de `knock_units_per_step` au plus, au cap de son
// recul, axe par axe contre les murs comme toute marche, et le reste du recul
// décroît de ce pas, qu'un mur l'arrête ou non. Gelé ou non : un recul pousse,
// il ne marche pas.
void Knock(SurvivorsField& field, SurvivorsFoe& foe) {
  const std::int32_t stride =
      std::min(foe.knock_left_units, std::max(field.rules->tuning.knock_units_per_step, 0));
  if (stride <= 0) {
    foe.knock_left_units = 0;
    return;
  }
  foe.position = Slide(field.ground, foe.position, TurnVector(foe.knock_heading, stride));
  foe.knock_left_units -= stride;
}

// La distance de `foe` au point que la grille tient pour lui, plafonnée à
// l'int32.
[[nodiscard]] std::int32_t DriftOf(const SurvivorsFoe& foe, FieldPoint gridded) noexcept {
  const std::uint32_t drift =
      IntegerSqrt(static_cast<std::uint64_t>(DistanceSquared(foe.position, gridded)));
  return static_cast<std::int32_t>(
      std::min<std::uint32_t>(drift, std::numeric_limits<std::int32_t>::max()));
}

// LA SÉPARATION : chaque paire de monstres qui se chevauchent, i croissant puis
// j > i croissant, s'écarte du recouvrement, moitié chacun, ou tout pour le
// mobile si l'autre est immobile ; chaque poussée plafonnée.
void Separate(SurvivorsField& field) {
  const SurvivorsTuning& tuning = field.rules->tuning;
  const std::int32_t most_push = std::max(tuning.separation_max_push_units, 0);
  if (most_push == 0) return;
  const std::int32_t widest = LargestFoeRadius(*field.rules);
  FillFoeGrid(field);
  const std::span<const FieldPoint> points = field.foe_points;
  // Le plus grand écart, depuis le début de la passe, entre un monstre et le
  // point que la grille tient pour lui. Un voisin qui chevauche i est à moins
  // de r_i + widest de sa position courante, donc à moins de r_i + widest +
  // most_drift de son point de grille : la requête de i, élargie de cet écart,
  // rend toutes les paires de i au moment où elle est faite. Les poussées que
  // i subit pendant sa propre passe ne relancent pas sa requête.
  std::int32_t most_drift = 0;
  for (std::size_t i = 0; i < field.foes.size(); ++i) {
    const std::int32_t radius_i = FoeRadius(*field.rules, field.foes[i]);
    const std::int64_t query = std::int64_t{radius_i} + widest + most_drift;
    field.foe_grid.Near(field.foes[i].position,
                        static_cast<std::int32_t>(std::min<std::int64_t>(
                            query, std::numeric_limits<std::int32_t>::max())),
                        points, field.nearby);
    for (const std::uint32_t j : field.nearby) {
      if (j <= i) continue;
      SurvivorsFoe& first = field.foes[i];
      SurvivorsFoe& second = field.foes[j];
      const std::int64_t reach = std::int64_t{radius_i} + FoeRadius(*field.rules, second);
      const std::int64_t distance_squared = DistanceSquared(first.position, second.position);
      if (distance_squared >= reach * reach) continue;
      // Un monstre au-dessus de la horde ne la pousse ni ne s'en écarte.
      if (((TraitsOf(*field.rules, first) | TraitsOf(*field.rules, second)) & kFoeOverHorde) != 0) {
        continue;
      }
      const std::int64_t overlap =
          reach - IntegerSqrt(static_cast<std::uint64_t>(distance_squared));
      const bool first_moves = Walks(field, first);
      const bool second_moves = Walks(field, second);
      if (!first_moves && !second_moves) continue;
      std::int64_t second_push = overlap;
      if (first_moves && second_moves) second_push = overlap / 2;
      if (!second_moves) second_push = 0;
      const std::int64_t first_push = first_moves ? overlap - second_push : 0;
      const std::uint8_t heading = OverlapHeading(second.serial);
      const FieldPoint second_from = second.position;
      if (second_push > 0) {
        const auto length =
            static_cast<std::int32_t>(std::min<std::int64_t>(second_push, most_push));
        second.position = Slide(field.ground, second.position,
                                AwayFrom(first.position, second.position, length, heading));
        most_drift = std::max(most_drift, DriftOf(second, points[j]));
      }
      if (first_push > 0) {
        const auto length =
            static_cast<std::int32_t>(std::min<std::int64_t>(first_push, most_push));
        const auto opposite =
            static_cast<std::uint8_t>((heading + kHeadingSteps / 2) % kHeadingSteps);
        first.position = Slide(field.ground, first.position,
                               AwayFrom(second_from, first.position, length, opposite));
        most_drift = std::max(most_drift, DriftOf(first, points[i]));
      }
    }
  }
}

// LA POUSSÉE DU HÉROS : un monstre qui le chevauche recule de (100 − part du
// héros) % du recouvrement, le héros de sa part ; contre un monstre immobile,
// le héros prend tout.
void ShoveHero(SurvivorsField& field) {
  const SurvivorsTuning& tuning = field.rules->tuning;
  const std::int64_t hero_pct = std::clamp<std::int64_t>(tuning.hero_shove_pct, 0, kPercent);
  SurvivorsHero& hero = field.hero;
  for (SurvivorsFoe& foe : field.foes) {
    const std::int64_t reach =
        std::int64_t{tuning.hero_radius_units} + FoeRadius(*field.rules, foe);
    const std::int64_t distance_squared = DistanceSquared(hero.position, foe.position);
    if (distance_squared >= reach * reach) continue;
    const std::int64_t overlap = reach - IntegerSqrt(static_cast<std::uint64_t>(distance_squared));
    const bool foe_moves = Walks(field, foe);
    const std::int32_t foe_push = foe_moves ? PercentOf(overlap, kPercent - hero_pct) : 0;
    const std::int32_t hero_push =
        foe_moves ? PercentOf(overlap, hero_pct) : static_cast<std::int32_t>(overlap);
    const std::uint8_t heading = OverlapHeading(foe.serial);
    const FieldPoint foe_from = foe.position;
    if (foe_push > 0) {
      foe.position = Slide(field.ground, foe.position,
                           AwayFrom(hero.position, foe.position, foe_push, heading));
    }
    if (hero_push > 0) {
      const auto opposite =
          static_cast<std::uint8_t>((heading + kHeadingSteps / 2) % kHeadingSteps);
      hero.position = Slide(field.ground, hero.position,
                            AwayFrom(foe_from, hero.position, hero_push, opposite));
    }
  }
}

}  // namespace

void RefreshFlow(SurvivorsField& field) {
  const std::int32_t cell_x = CellOf(field.hero.position.x);
  const std::int32_t cell_y = CellOf(field.hero.position.y);
  // Le héros ne quitte pas l'arène, que ses murs bornent ; une case hors de
  // l'arène laisse le champ tel quel.
  if (cell_x < 0 || cell_y < 0 || cell_x >= field.ground.width() ||
      cell_y >= field.ground.height()) {
    return;
  }
  const SurvivorsTuning& tuning = field.rules->tuning;
  field.flow.Refresh(
      field.ground,
      CellPosition{static_cast<std::uint16_t>(cell_x), static_cast<std::uint16_t>(cell_y), 0},
      field.step, tuning.flow_radius_cells, tuning.flow_refresh_steps);
}

void MoveHorde(SurvivorsField& field) {
  // Un rappel nul est éteint.
  const std::int64_t recall = field.rules->tuning.recall_units;
  for (SurvivorsFoe& foe : field.foes) {
    // Un recul en cours tient lieu de tout le pas : ni rappel ni poursuite.
    if (foe.knock_left_units > 0) {
      Knock(field, foe);
      continue;
    }
    // Un essaim ou un raid file droit, sans rappel : SteerFoes le retire.
    if (foe.drift_heading != kNoHeading) continue;
    // Trop loin du héros, ou dans une impasse que ses murs lui imposent : le
    // monstre est rappelé, puis poursuit dans le même pas.
    if (recall > 0 && (DistanceSquared(foe.position, field.hero.position) > recall * recall ||
                       (Stranded(field.flow, foe.position) &&
                        (TraitsOf(*field.rules, foe) & kFoePassesWalls) == 0))) {
      Recall(field, foe);
    }
    // Le comportement a mené son pas : aucune poursuite.
    if (foe.steered) continue;
    // Arrêté par un mur hors du champ : rappelé, sans autre pas.
    if (Chase(field, foe) && recall > 0) Recall(field, foe);
  }
  Separate(field);
  ShoveHero(field);
}

void RebuildGrid(SurvivorsField& field) { FillFoeGrid(field); }

}  // namespace ro::sim::survivors
