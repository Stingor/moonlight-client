#pragma once

#include "ro/sim/combat_notice.h"
#include "ro/sim/world_fwd.h"

#include "effects/effect_view.h"

namespace moonclient {

// LES CAS QU'UN PAQUET DE COMPÉTENCE TRAITE SEUL, par-dessus les tables : le
// client les écrit en dur dans ses gestionnaires, compétence par compétence,
// et rien d'autre ne pose leurs effets.
struct OwnSkillCases {
  // CE QUE LE PAQUET DES DÉGÂTS MONTRE DE LUI-MÊME, après le registre des
  // actions. Voir `effect_notify_skill.cpp`.
  static void OnNotifySkill(EffectsInPlay& effects, const ro::sim::World& world,
                            const MapStage& stage, ro::sim::WalkPlanner& planner,
                            const ro::sim::Strike& strike);

  // CE QUE L'INCANTATION MONTRE PAR COMPÉTENCE, après son effet d'incantation.
  // Vrai quand la compétence a son cas : le client ne consulte alors ni son
  // registre des actions, ni ses tables par défaut. Voir
  // `effect_cast_skill_cases.cpp`.
  static bool OnCast(EffectsInPlay& effects, const MapStage& stage, ro::sim::WalkPlanner& planner,
                     const ro::sim::Actor& caster, const ro::sim::Cast& cast);

  // LES TABLES PAR DÉFAUT DE L'INCANTATION, quand ni un cas ni le registre des
  // actions n'ont répondu.
  static void OnCastByDefault(EffectsInPlay& effects, const MapStage& stage,
                              ro::sim::WalkPlanner& planner, const ro::sim::Actor& caster,
                              const ro::sim::Cast& cast);
};

}  // namespace moonclient
