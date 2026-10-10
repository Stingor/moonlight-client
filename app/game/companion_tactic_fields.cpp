#include "game/companion_tactic_fields.h"

namespace moonclient {
namespace {

using ro::sim::CompanionTactic;

// 🔴 LA FENÊTRE NE CONNAÎT PAS L'IA, mais elle en suit les listes et les bornes.
// Ici seulement les deux se voient : c'est donc ici qu'un écart casse la
// compilation.
static_assert(static_cast<int>(ro::sim::TacticAvoid::kAlways) + 1 == ro::ui::kTacticAvoidChoices);
static_assert(static_cast<int>(ro::sim::TacticSkillUse::kAlways) + 1 ==
              ro::ui::kTacticSkillUseChoices);
static_assert(static_cast<int>(ro::sim::TacticShare::kAlways) + 1 == ro::ui::kTacticShareChoices);
static_assert(static_cast<int>(ro::sim::TacticChase::kNever) + 1 == ro::ui::kTacticChaseChoices);
static_assert(static_cast<int>(ro::sim::TacticSkillUse::kTimes) == ro::ui::kTacticSkillUseTimes);
static_assert(static_cast<int>(ro::sim::TacticSkillUse::kOnceAtLevel) ==
              ro::ui::kTacticSkillUseOnceAtLevel);
static_assert(ro::sim::kTacticAvoidMostCells == ro::ui::kTacticAvoidMostCells);
static_assert(ro::sim::kTacticSkillUseMost == ro::ui::kTacticSkillUseMost);
static_assert(ro::sim::kTacticSpReserveMost == ro::ui::kTacticSpReserveMost);
static_assert(ro::sim::kTacticWeightMost == ro::ui::kTacticWeightMost);

}  // namespace

ro::ui::TacticAdvancedFields FieldsOf(const CompanionTactic& tactic) {
  return ro::ui::TacticAdvancedFields{
      .avoid = static_cast<int>(tactic.avoid),
      .avoid_threshold_cells = tactic.avoid_threshold_cells,
      .avoid_step_cells = tactic.avoid_step_cells,
      .skill_use = static_cast<int>(tactic.skill_use),
      .skill_use_count = tactic.skill_use_count,
      .opening_skill_id = tactic.opening_skill_id,
      .harass = tactic.harass,
      .sp_reserve_points = static_cast<int>(tactic.sp_reserve_points),
      .weight = tactic.weight,
      .share = static_cast<int>(tactic.share),
      .chase = static_cast<int>(tactic.chase),
  };
}

CompanionTactic WithFields(CompanionTactic tactic, const ro::ui::TacticAdvancedFields& fields) {
  tactic.avoid = static_cast<ro::sim::TacticAvoid>(fields.avoid);
  tactic.avoid_threshold_cells = fields.avoid_threshold_cells;
  tactic.avoid_step_cells = fields.avoid_step_cells;
  tactic.skill_use = static_cast<ro::sim::TacticSkillUse>(fields.skill_use);
  tactic.skill_use_count = fields.skill_use_count;
  tactic.opening_skill_id = fields.opening_skill_id;
  tactic.harass = fields.harass;
  // Négatif dans la fenêtre : celle du réglage général.
  tactic.sp_reserve_points =
      fields.sp_reserve_points < 0 ? ro::sim::kTacticSpReserveFromBase : fields.sp_reserve_points;
  tactic.weight = fields.weight;
  tactic.share = static_cast<ro::sim::TacticShare>(fields.share);
  tactic.chase = static_cast<ro::sim::TacticChase>(fields.chase);
  return tactic;
}

ro::ui::TacticAdvancedFields ConductFieldsOf(const ro::sim::CompanionPlayerConduct& conduct) {
  return FieldsOf(ro::sim::TacticOfConduct(conduct));
}

ro::sim::CompanionPlayerConduct WithConductFields(ro::sim::CompanionPlayerConduct conduct,
                                                  const ro::ui::TacticAdvancedFields& fields) {
  const CompanionTactic tactic = WithFields(ro::sim::TacticOfConduct(conduct), fields);
  conduct.skill_use = tactic.skill_use;
  conduct.skill_use_count = tactic.skill_use_count;
  conduct.opening_skill_id = tactic.opening_skill_id;
  conduct.sp_reserve_points = tactic.sp_reserve_points;
  conduct.chase = tactic.chase;
  return conduct;
}

}  // namespace moonclient
