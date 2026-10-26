#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <vector>

#include "ro/sim/survivors_content.h"
#include "ro/sim/survivors_geometry.h"
#include "ro/sim/survivors_ground.h"
#include "ro/sim/survivors_news.h"
#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_run.h"
#include "ro/sim/survivors_state.h"

#include "survivors_space.h"
#include "survivors_systems.h"

namespace ro::sim::survivors {

namespace {

// La phase de départ d'un acte, et celle de l'oscillation d'un volant, se
// tirent du serial : 7 décale deux serials voisins de 7 pas, sans aléa, pour
// que deux monstres nés ensemble ne jouent pas ensemble.
constexpr std::uint64_t kPhaseFactor = 7;
// Un tireur se tient à sa portée, à une case près : il s'éloigne en deçà,
// approche au-delà, et ne tire qu'entre les deux.
constexpr std::int32_t kRangedSlackUnits = kSubunitsPerCell;
// Un volant oscille de ±8 caps autour du cap du héros, côté changé toutes les
// 500 ms.
constexpr std::uint32_t kSwayMs = 500;
constexpr std::uint8_t kSwayHeadings = 8;
constexpr std::uint64_t kSwaySides = 2;
// La ligne d'un monstre à son but se sonde de demi-case en demi-case.
constexpr std::int64_t kLineSampleUnits = kSubunitsPerCell / 2;

[[nodiscard]] std::int64_t Distance(FieldPoint a, FieldPoint b) noexcept {
  return IntegerSqrt(static_cast<std::uint64_t>(DistanceSquared(a, b)));
}

// `step` + `steps`, saturé dans l'uint32.
[[nodiscard]] std::uint32_t Later(std::uint32_t step, std::uint64_t steps) noexcept {
  return static_cast<std::uint32_t>(std::min<std::uint64_t>(
      std::uint64_t{step} + steps, std::numeric_limits<std::uint32_t>::max()));
}

// Le pas `delta` d'un monstre aux traits `traits`, et son cap.
void Stride(SurvivorsField& field, SurvivorsFoe& foe, std::uint8_t traits, FieldPoint delta) {
  foe.position = FoeSlide(field.ground, traits, foe.position, delta);
  foe.facing = HeadingOf(delta, foe.facing);
}

// Vrai si aucun mur ne coupe le segment de `from` à `to` : ses points, pris de
// demi-case en demi-case au plus le long de son axe le plus long, tombent tous
// sur des cases praticables.
[[nodiscard]] bool ClearLine(const SurvivorsGround& ground, FieldPoint from, FieldPoint to) {
  const std::int64_t dx = std::int64_t{to.x} - from.x;
  const std::int64_t dy = std::int64_t{to.y} - from.y;
  const std::int64_t span = std::max(dx < 0 ? -dx : dx, dy < 0 ? -dy : dy);
  const std::int64_t samples = (span + kLineSampleUnits - 1) / kLineSampleUnits;
  for (std::int64_t sample = 1; sample <= samples; ++sample) {
    const auto x = static_cast<std::int32_t>(from.x + dx * sample / samples);
    const auto y = static_cast<std::int32_t>(from.y + dy * sample / samples);
    if (!ground.Passable(CellOf(x), CellOf(y))) return false;
  }
  return true;
}

// Le pas droit `delta` d'un monstre aux traits `traits` vers son but `goal`,
// s'il passe : faux, sans bouger le monstre, quand un mur coupe la ligne du
// but ou refuse un axe du pas. Le monstre retombe alors sur la poursuite, que
// le champ guide autour du mur et qui rappelle un monstre qu'aucun champ ne
// guide. Un monstre qui traverse les murs passe toujours.
[[nodiscard]] bool StrideIfClear(SurvivorsField& field, SurvivorsFoe& foe, std::uint8_t traits,
                                 FieldPoint goal, FieldPoint delta) {
  const FieldPoint at = FoeSlide(field.ground, traits, foe.position, delta);
  if ((traits & kFoePassesWalls) == 0 &&
      (!ClearLine(field.ground, foe.position, goal) || SlideBlocked(foe.position, delta, at))) {
    return false;
  }
  foe.position = at;
  foe.facing = HeadingOf(delta, foe.facing);
  return true;
}

// ── Les actes ──────────────────────────────────────────────────────────────

[[nodiscard]] std::int32_t DashPerStep(const FoeBehaviour& behaviour) noexcept {
  return behaviour.dash_units_per_s / static_cast<std::int32_t>(kStepsPerSecond);
}

// Les pas d'une charge : `active_ms`, ou à défaut le temps de couvrir sa
// portée ; 0 pour une charge qui ne bouge pas.
[[nodiscard]] std::uint32_t DashSteps(const FoeBehaviour& behaviour) noexcept {
  const std::int32_t per_step = DashPerStep(behaviour);
  if (per_step <= 0) return 0;
  if (behaviour.active_ms != 0) return StepsOf(behaviour.active_ms);
  const std::int64_t reach = std::max(behaviour.reach_units, 0);
  return static_cast<std::uint32_t>((reach + per_step - 1) / per_step);
}

// La condition d'un acte, sur la distance au héros : un tir à sa portée, à
// une case près ; une charge à sa portée au plus ; un bond au-delà de sa
// longueur ; une cachette toujours.
[[nodiscard]] bool ActHolds(const SurvivorsField& field, const SurvivorsFoe& foe,
                            const FoeBehaviour& behaviour, FoeAct act) noexcept {
  const std::int64_t distance = Distance(foe.position, field.hero.position);
  const std::int64_t reach = behaviour.reach_units;
  switch (act) {
    case FoeAct::kShoot:
      return distance >= reach - kRangedSlackUnits && distance <= reach + kRangedSlackUnits;
    case FoeAct::kCharge:
      return distance <= reach;
    case FoeAct::kBlink:
      return distance > reach;
    case FoeAct::kHide:
      return true;
    case FoeAct::kNone:
      break;
  }
  return false;
}

// L'ANNONCE d'un acte : son but est fixé (point visé, fin de charge, point de
// chute) et publié, sauf pour une cachette, qui ne s'annonce pas. Faux, sans
// rien publier, pour un acte impossible : une charge qui ne bouge pas, un bond
// sans point de chute.
[[nodiscard]] bool Announce(SurvivorsField& field, SurvivorsFoe& foe, const FoeBehaviour& behaviour,
                            FoeAct act, std::uint32_t strike_step) {
  const FieldPoint hero = field.hero.position;
  SurvivorsWarning warning{.kind = WarningKind::kShot,
                           .source_serial = foe.serial,
                           .from = foe.position,
                           .to = hero,
                           .radius_units = 0,
                           .strike_step = strike_step};
  switch (act) {
    case FoeAct::kShoot:
      foe.act_target = hero;
      warning.radius_units = behaviour.shot_radius_units;
      break;
    case FoeAct::kCharge: {
      const std::uint32_t steps = DashSteps(behaviour);
      if (steps == 0) return false;
      const std::int64_t length = std::int64_t{DashPerStep(behaviour)} * steps;
      foe.act_heading = HeadingOf(hero - foe.position, foe.facing);
      foe.act_target = foe.position +
                       TurnVector(foe.act_heading, static_cast<std::int32_t>(std::min<std::int64_t>(
                                                       length, Cells(kMostArenaSideCells))));
      warning.kind = WarningKind::kCharge;
      warning.to = foe.act_target;
      warning.radius_units = FoeRadius(*field.rules, foe);
      break;
    }
    case FoeAct::kBlink: {
      const std::optional<FieldPoint> landing = LeapLanding(
          field.ground, foe.position, field.hero.position, foe.facing, behaviour.reach_units);
      if (!landing) return false;
      foe.act_target = *landing;
      warning.kind = WarningKind::kLeap;
      warning.to = *landing;
      warning.radius_units = FoeRadius(*field.rules, foe);
      break;
    }
    case FoeAct::kHide:
      return true;
    case FoeAct::kNone:
      return false;
  }
  foe.facing = HeadingOf(foe.act_target - foe.position, foe.facing);
  PublishWarning(field.news, warning);
  return true;
}

// Un pas de charge, au cap fixé à l'annonce ; un mur l'arrête net.
void Dash(SurvivorsField& field, SurvivorsFoe& foe, const FoeBehaviour& behaviour) {
  const FieldPoint from = foe.position;
  const FieldPoint delta = TurnVector(foe.act_heading, DashPerStep(behaviour));
  Stride(field, foe, behaviour.traits, delta);
  if (SlideBlocked(from, delta, foe.position)) foe.act_until_step = field.step;
}

// La flèche d'un tireur, vers le point visé à l'annonce : elle vole sur sa
// portée plus une case, puis s'éteint. Rien au plafond des tirs de monstres.
void Loose(SurvivorsField& field, SurvivorsFoe& foe, const FoeBehaviour& behaviour) {
  const std::int32_t per_step =
      behaviour.shot_units_per_s / static_cast<std::int32_t>(kStepsPerSecond);
  if (per_step <= 0 || field.foe_shots.size() >= field.rules->caps.most_foe_shots) return;
  FieldPoint velocity = Toward(foe.position, foe.act_target, per_step);
  if (velocity == FieldPoint{}) velocity = TurnVector(foe.facing, per_step);
  const std::int64_t range = std::int64_t{std::max(behaviour.reach_units, 0)} + kRangedSlackUnits;
  field.foe_shots.push_back(SurvivorsFoeShot{
      .position = foe.position,
      .previous_position = foe.position,
      .velocity = velocity,
      .damage = behaviour.shot_damage,
      .radius_units = behaviour.shot_radius_units,
      .expires_step = Later(
          field.step, static_cast<std::uint64_t>(std::max<std::int64_t>(range / per_step, 1))),
      .from_serial = foe.serial});
}

// L'acte JOUÉ, au pas que son annonce a dit. Vrai quand il tient le monstre ce
// pas : un tir, une charge, un bond ; une cachette le laisse marcher.
[[nodiscard]] bool Play(SurvivorsField& field, SurvivorsFoe& foe, const FoeBehaviour& behaviour,
                        FoeAct act) {
  switch (act) {
    case FoeAct::kShoot:
      Loose(field, foe, behaviour);
      return true;
    case FoeAct::kCharge:
      foe.act_until_step = Later(field.step, DashSteps(behaviour));
      Dash(field, foe, behaviour);
      return true;
    case FoeAct::kBlink:
      foe.position = foe.act_target;
      return true;
    case FoeAct::kHide:
      foe.act_until_step = Later(field.step, StepsOf(behaviour.active_ms));
      return false;
    case FoeAct::kNone:
      break;
  }
  return false;
}

// LE CYCLE D'UN ACTE : une charge en cours file ; un acte annoncé tient le
// monstre immobile jusqu'à son pas, puis joue ; sinon, l'acte part si son pas
// est venu et que sa condition tient, s'annonce, et repart pour une période.
// La première fois, il s'arme à `born_step + (serial × 7) % période`. Vrai
// quand l'acte tient le monstre ce pas.
[[nodiscard]] bool Act(SurvivorsField& field, SurvivorsFoe& foe, const FoeBehaviour& behaviour,
                       FoeAct act) {
  if (act == FoeAct::kCharge && foe.act_until_step > field.step) {
    Dash(field, foe, behaviour);
    return true;
  }
  if (foe.telegraph_until_step != 0) {
    if (field.step < foe.telegraph_until_step) return act != FoeAct::kHide;
    foe.telegraph_until_step = 0;
    return Play(field, foe, behaviour, act);
  }
  const std::uint32_t period_steps = StepsOf(behaviour.period_ms);
  if (period_steps == 0) return false;
  if (foe.act_ready_step == 0) {
    foe.act_ready_step = Later(foe.born_step, (foe.serial * kPhaseFactor) % period_steps);
  }
  if (foe.act_ready_step > field.step || !ActHolds(field, foe, behaviour, act)) return false;
  const std::uint32_t strike_step = Later(field.step, StepsOf(behaviour.telegraph_ms));
  if (!Announce(field, foe, behaviour, act, strike_step)) return false;
  foe.act_ready_step = Later(field.step, period_steps);
  if (strike_step > field.step) {
    foe.telegraph_until_step = strike_step;
    return act != FoeAct::kHide;
  }
  return Play(field, foe, behaviour, act);
}

// ── Les marches ────────────────────────────────────────────────────────────

// LE VOLANT : droit sur le héros, le cap décalé de ±8, côté tiré de la phase
// de son serial ; jamais au-delà du héros. Vrai quand ce vol le mène ; un mur
// qui coupe sa ligne ou arrête son pas le rend à la poursuite (faux).
[[nodiscard]] bool Fly(SurvivorsField& field, SurvivorsFoe& foe, std::uint8_t traits) {
  const FieldPoint hero = field.hero.position;
  const std::int64_t distance = Distance(foe.position, hero);
  if (distance == 0) return true;
  const std::uint64_t sway_phase =
      (field.step + foe.serial * kPhaseFactor) / StepsOf(kSwayMs) % kSwaySides;
  const std::uint8_t sway = sway_phase == 0 ? kSwayHeadings : kHeadingSteps - kSwayHeadings;
  const auto heading = static_cast<std::uint8_t>(
      (HeadingOf(hero - foe.position, foe.facing) + sway) % kHeadingSteps);
  const auto stride =
      static_cast<std::int32_t>(std::min<std::int64_t>(PaceOf(field, foe), distance));
  return StrideIfClear(field, foe, traits, hero, TurnVector(heading, stride));
}

// LE VOLEUR : il marche vers la gemme libre (que l'aimant n'a pas saisie) la
// plus proche, la première née à égalité, et l'absorbe à son contact. Vrai
// quand une gemme le mène ; sans gemme libre, ou quand un mur coupe sa ligne
// ou arrête son pas, il poursuit (faux).
[[nodiscard]] bool Pilfer(SurvivorsField& field, SurvivorsFoe& foe, std::uint8_t traits) {
  std::size_t nearest = field.gems.size();
  std::int64_t nearest_distance = std::numeric_limits<std::int64_t>::max();
  for (std::size_t index = 0; index < field.gems.size(); ++index) {
    const SurvivorsGem& gem = field.gems[index];
    if (gem.pull_units != 0) continue;
    const std::int64_t distance = DistanceSquared(gem.position, foe.position);
    if (distance < nearest_distance) {
      nearest = index;
      nearest_distance = distance;
    }
  }
  if (nearest == field.gems.size()) return false;
  const FieldPoint goal = field.gems[nearest].position;
  if (!StrideIfClear(field, foe, traits, goal,
                     StepToward(foe.position, goal, PaceOf(field, foe)))) {
    return false;
  }
  const std::int64_t reach = FoeRadius(*field.rules, foe);
  if (DistanceSquared(foe.position, goal) < reach * reach) {
    foe.carried_exp = static_cast<std::uint32_t>(
        std::min<std::uint64_t>(std::uint64_t{foe.carried_exp} + field.gems[nearest].exp,
                                std::numeric_limits<std::uint32_t>::max()));
    field.gems.erase(field.gems.begin() + static_cast<std::ptrdiff_t>(nearest));
  }
  return true;
}

// LE TIREUR : en deçà de sa portée moins une case, il s'éloigne du héros ;
// au-delà de sa portée plus une case, il poursuit (faux) ; entre les deux, il
// se tient.
[[nodiscard]] bool KeepRange(SurvivorsField& field, SurvivorsFoe& foe,
                             const FoeBehaviour& behaviour) {
  const FieldPoint hero = field.hero.position;
  const std::int64_t distance = Distance(foe.position, hero);
  const std::int64_t reach = behaviour.reach_units;
  if (distance > reach + kRangedSlackUnits) return false;
  if (distance < reach - kRangedSlackUnits) {
    const std::int32_t pace = PaceOf(field, foe);
    FieldPoint away = Toward(hero, foe.position, pace);
    if (away == FieldPoint{}) {
      away = TurnVector(static_cast<std::uint8_t>((foe.facing + kHeadingSteps / 2) % kHeadingSteps),
                        pace);
    }
    Stride(field, foe, behaviour.traits, away);
  }
  return true;
}

// Le pas d'un monstre selon son comportement.
void Steer(SurvivorsField& field, SurvivorsFoe& foe) {
  // Un coup de boss mène ce corps : RunBossScripts a déjà joué son pas.
  if (foe.scripted) {
    foe.steered = true;
    return;
  }
  foe.previous_position = foe.position;
  foe.steered = false;
  const FoeRule* rule = FindFoe(*field.rules, foe.foe_id);
  if (rule == nullptr) return;
  const FoeBehaviour& behaviour = rule->behaviour;
  const FoeAct act = ActOf(behaviour);
  // Gelé ou repoussé : l'acte annoncé se perd et la charge s'arrête ; le recul
  // et le gel se jouent dans MoveHorde.
  if (Frozen(field, foe) || foe.knock_left_units > 0) {
    foe.telegraph_until_step = 0;
    if (act == FoeAct::kCharge) foe.act_until_step = std::min(foe.act_until_step, field.step);
    return;
  }
  // Un essaim ou un raid file droit, sans acte.
  if (foe.drift_heading != kNoHeading) {
    Stride(field, foe, behaviour.traits, TurnVector(foe.drift_heading, PaceOf(field, foe)));
    foe.steered = true;
    return;
  }
  if (act != FoeAct::kNone && Act(field, foe, behaviour, act)) {
    foe.steered = true;
    return;
  }
  switch (behaviour.move) {
    case FoeMove::kStill:
      foe.steered = true;
      break;
    case FoeMove::kFlyer:
      foe.steered = Fly(field, foe, behaviour.traits);
      break;
    case FoeMove::kThief:
      foe.steered = Pilfer(field, foe, behaviour.traits);
      break;
    case FoeMove::kRanged:
      foe.steered = KeepRange(field, foe, behaviour);
      break;
    case FoeMove::kChase:
    case FoeMove::kCharger:
    case FoeMove::kBlinker:
      break;
  }
}

}  // namespace

void SteerFoes(SurvivorsField& field) {
  if (field.rules == nullptr) return;
  for (SurvivorsFoe& foe : field.foes) Steer(field, foe);
  // Un essaim ou un raid au-delà du rappel quitte la partie, sans mort ni
  // gemme ; un rappel nul est éteint.
  const std::int64_t recall = field.rules->tuning.recall_units;
  if (recall <= 0) return;
  const FieldPoint hero = field.hero.position;
  std::erase_if(field.foes, [&](const SurvivorsFoe& foe) {
    return foe.drift_heading != kNoHeading && DistanceSquared(foe.position, hero) > recall * recall;
  });
}

}  // namespace ro::sim::survivors
