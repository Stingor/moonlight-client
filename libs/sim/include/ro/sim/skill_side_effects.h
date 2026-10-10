#pragma once

#include <cstdint>

#include "ro/sim/actor.h"
#include "ro/sim/combat_notice.h"

namespace ro::sim {

// CE QU'UNE COMPÉTENCE FAIT À L'ACTEUR EN PLUS DE SA POSTURE, et que le client
// décide lui-même, sans que le serveur l'annonce.

// L'ANNONCE D'UNE COMPÉTENCE SANS DÉGÂTS, sur son lanceur : la barre d'Auto
// Counter sous le joueur local quand l'emploi a réussi (voir `SkillGauge`),
// l'émoticône de Wink Charm quand la cible est là (voir `kWinkCharmSkill`).
void ApplyAnnouncedSkillToCaster(Actor& caster, const SupportHit& hit, bool caster_is_player,
                                 bool target_known);

// L'IMPACT D'UN COUP arme la pose d'arrivée de son attaquant — voir
// `kArrivalPoseSkill`.
void ArmArrivalPose(Actor& attacker, const Strike& strike);

// UNE ARRIVÉE lève la pose qu'un coup a armée, à cet instant.
void NoteArrival(Actor& actor, std::uint32_t arrived_ms);

}  // namespace ro::sim
