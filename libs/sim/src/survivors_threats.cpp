#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>

#include "ro/sim/survivors_geometry.h"
#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_run.h"
#include "ro/sim/survivors_state.h"

#include "survivors_space.h"
#include "survivors_systems.h"

namespace ro::sim::survivors {

namespace {

// Vrai si le héros chevauche le disque de centre `center` et de rayon
// `radius_units` : la distance des centres reste sous la somme des rayons.
[[nodiscard]] bool HeroWithin(const SurvivorsField& field, FieldPoint center,
                              std::int32_t radius_units) noexcept {
  const std::int64_t reach = std::int64_t{radius_units} + field.rules->tuning.hero_radius_units;
  return DistanceSquared(field.hero.position, center) < reach * reach;
}

// Un pas d'un tir de monstre : il avance de sa vitesse, s'éteint à la fin de
// sa vie, hors de l'arène ou sur une case murée, et touche le héros qu'il
// chevauche, une fois. Faux quand il s'éteint.
[[nodiscard]] bool AdvanceFoeShot(SurvivorsField& field, SurvivorsFoeShot& shot) {
  if (field.step >= shot.expires_step) return false;
  shot.previous_position = shot.position;
  shot.position = shot.position + shot.velocity;
  if (!InsideArena(field.ground, shot.position) ||
      !field.ground.Passable(CellOf(shot.position.x), CellOf(shot.position.y))) {
    return false;
  }
  // Un tir touche un seul héros, le premier qu'il chevauche.
  bool hit = false;
  ForEachStanding(field, [&] {
    if (hit || !HeroWithin(field, shot.position, shot.radius_units)) return;
    hit = true;
    // Un héros déjà tombé ce pas n'est plus blessé ; le tir s'éteint quand même.
    if (field.phase == SurvivorsPhase::kRunning && shot.damage > 0) {
      (void)HurtHero(field, shot.damage);
    }
  });
  return !hit;
}

// LE GEL : le héros marche `slow_pct` % moins vite pendant `slow_ms`, aux
// bornes du contenu. Un gel qui le trouve déjà gelé garde le plus fort et le
// plus long des deux.
void Freeze(SurvivorsHero& hero, std::uint32_t step, std::int32_t slow_pct,
            std::uint32_t slow_ms) noexcept {
  const std::uint64_t until = std::uint64_t{step} + 1 + StepsOf(std::min(slow_ms, kMostFrostMs));
  const auto until_step = static_cast<std::uint32_t>(
      std::min<std::uint64_t>(until, std::numeric_limits<std::uint32_t>::max()));
  const std::int32_t pct = std::clamp(slow_pct, 0, kMostFrostSlowPct);
  const bool frozen = step < hero.frost_until_step;
  hero.frost_slow_pct = frozen ? std::max(hero.frost_slow_pct, pct) : pct;
  hero.frost_until_step = std::max(frozen ? hero.frost_until_step : 0U, until_step);
}

// Une menace au sol à ce pas : armée, elle blesse le héros qui la chevauche,
// puis attend sa période (une seule fois pour une période nulle). Faux quand
// elle s'éteint.
[[nodiscard]] bool AdvanceHazard(SurvivorsField& field, SurvivorsHazard& hazard) {
  if (field.step >= hazard.until_step) return false;
  if (field.step < hazard.armed_step || field.step < hazard.next_hurt_step) return true;
  if (field.phase != SurvivorsPhase::kRunning || hazard.damage <= 0) return true;
  // Une menace au sol blesse tous ceux qu'elle chevauche.
  bool hit = false;
  ForEachStanding(field, [&] {
    if (field.phase != SurvivorsPhase::kRunning ||
        !HeroWithin(field, hazard.center, hazard.radius_units)) {
      return;
    }
    hit = true;
    // Le gel ne prend que sur un coup qui a porté, et pas sur le héros qui
    // vient de se relever, invulnérable.
    const std::int32_t hurt_before = field.news.hurt_by_hero[field.stage];
    (void)HurtHero(field, hazard.damage);
    const bool landed = field.news.hurt_by_hero[field.stage] != hurt_before;
    if (landed && hazard.slow_pct > 0 && !field.hero.down &&
        field.step >= field.hero.invulnerable_until_step) {
      Freeze(field.hero, field.step, hazard.slow_pct, hazard.slow_ms);
    }
  });
  if (!hit) return true;
  hazard.next_hurt_step = hazard.period_steps > 0
                              ? static_cast<std::uint32_t>(std::min<std::uint64_t>(
                                    std::uint64_t{field.step} + hazard.period_steps,
                                    std::numeric_limits<std::uint32_t>::max()))
                              : std::numeric_limits<std::uint32_t>::max();
  return true;
}

}  // namespace

void AdvanceThreats(SurvivorsField& field) {
  if (field.rules == nullptr) return;
  // Retraits stables : les tirs gardent leur ordre de tir, les menaces leur
  // ordre de pose.
  std::size_t kept = 0;
  for (std::size_t index = 0; index < field.foe_shots.size(); ++index) {
    SurvivorsFoeShot shot = field.foe_shots[index];
    if (AdvanceFoeShot(field, shot)) field.foe_shots[kept++] = shot;
  }
  field.foe_shots.resize(kept);
  kept = 0;
  for (std::size_t index = 0; index < field.hazards.size(); ++index) {
    SurvivorsHazard hazard = field.hazards[index];
    if (AdvanceHazard(field, hazard)) field.hazards[kept++] = hazard;
  }
  field.hazards.resize(kept);
}

}  // namespace ro::sim::survivors
