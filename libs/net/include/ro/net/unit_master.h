#pragma once

#include "ro/net/world_update.h"
#include "ro/sim/world_fwd.h"

namespace ro::net {

// LE MAÎTRE D'UN MONSTRE INVOQUÉ : `ZC_MOONLIGHT_UNIT_MASTER 0x0F37`.
//
//     0f37 <len>.W <monstre>.L <maître>.L
//
// Le serveur ne l'envoie qu'à qui a annoncé savoir le lire — voir
// `kUiCapUnitMaster` —, après chaque apparition d'un monstre qui a un maître, et
// de nouveau si ce maître change ; un maître nul dit « sans maître ». Un paquet
// qui nomme un acteur inconnu est lu et ignoré : voir `sim::TellSummonMaster`.
//
// À qui annonce aussi `kUiCapCompanionMaster`, la même trame suit l'apparition
// d'un compagnon — pet, homoncule, mercenaire, élémentaire — et nomme le joueur
// à qui il appartient ; elle se range de même.
[[nodiscard]] FrameOutcome ApplyUnitMasterFrame(const Frame& frame, sim::World& world);

}  // namespace ro::net
