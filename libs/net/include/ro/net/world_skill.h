#pragma once

#include <cstdint>

#include "ro/net/connection.h"
#include "ro/net/world_update.h"
#include "ro/sim/world_fwd.h"

namespace ro::net {

// CE QU'UNE COMPÉTENCE MONTRE, traduit dans le monde.
//
// Le client fait paraître une compétence par SIX chemins, et ce sont six
// paquets différents : l'incantation qui commence, le coup qui porte, la
// compétence sans dégâts, l'unité posée au sol, l'effet nu, et ce que le corps
// du lanceur en fait. Les confondre fait chercher au mauvais endroit — c'est
// l'erreur qui a coûté des heures sur Sonic Blow.
//
// Ils vivent à part de `ApplyFrame` pour deux raisons, et la seconde est la
// vraie : ils forment une FAMILLE, celle que les lots d'effets font grandir,
// et `world_update.cpp` approchait son plafond de lignes. Un fichier qui ne
// porte qu'un sujet se relit ; un fichier qui porte tout se parcourt.
//
// Même contrat que `ApplyFrame` : un acteur inconnu n'est pas inventé, et
// `now_ms` est l'horloge de l'APPELANT.
//
// ⚠ Ce n'est PAS le grimoire. Ce que le personnage sait lancer passe par
// `ApplySkillFrame` (ro/net/skill.h) : cela ne parle ni d'acteurs ni de cases.
[[nodiscard]] FrameOutcome ApplyWorldSkillFrame(const Frame& frame, sim::World& world,
                                                std::uint32_t now_ms);

// LE PIÈGE QUI SE DÉCLENCHE : le serveur change l'apparence de BASE de l'UNITÉ
// posée au sol par le paquet d'apparence d'un ACTEUR, sur l'identifiant de
// l'unité. `look_type` est la sorte d'apparence que le paquet change ; toute
// autre que la base ne concerne pas une unité.
[[nodiscard]] FrameOutcome ApplySkillUnitLook(sim::World& world, std::uint32_t instance_id,
                                              std::uint8_t look_type, std::uint32_t look,
                                              std::uint32_t now_ms);

}  // namespace ro::net
