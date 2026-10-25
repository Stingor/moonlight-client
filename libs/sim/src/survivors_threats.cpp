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
  if (!InsideArena(field.arena, shot.position) ||
      !field.arena.Passable(CellOf(shot.position.x), CellOf(shot.position.y))) {
    return false;
  }
  if (!HeroWithin(field, shot.position, shot.radius_units)) return true;
  // Un héros déjà tombé ce pas n'est plus blessé ; le tir s'éteint quand même.
  if (field.phase == SurvivorsPhase::kRunning && shot.damage > 0) {
    (void)HurtHero(field, shot.damage);
  }
  return false;
}

// Une menace au sol à ce pas : armée, elle blesse le héros qui la chevauche,
// puis attend sa période (une seule fois pour une période nulle). Faux quand
// elle s'éteint.
[[nodiscard]] bool AdvanceHazard(SurvivorsField& field, SurvivorsHazard& hazard) {
  if (field.step >= hazard.until_step) return false;
  if (field.step < hazard.armed_step || field.step < hazard.next_hurt_step) return true;
  if (field.phase != SurvivorsPhase::kRunning || hazard.damage <= 0) return true;
  if (!HeroWithin(field, hazard.center, hazard.radius_units)) return true;
  (void)HurtHero(field, hazard.damage);
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
