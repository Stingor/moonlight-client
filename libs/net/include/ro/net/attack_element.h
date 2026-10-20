#pragma once

#include "ro/net/connection.h"
#include "ro/net/world_update.h"
#include "ro/sim/world_fwd.h"

namespace ro::net {

// L'ÉLÉMENT D'ATTAQUE D'UN JOUEUR : `ZC_MOONLIGHT_ATTACK_ELEMENT 0x0F4A`.
//
//     0f4a <len>.W <joueur>.L <élément>.B
//
// `élément` : 0 (neutre) à 9 (mort-vivant) dans l'ordre du serveur, 0xFF pour
// « aucun ». Le serveur ne l'envoie qu'à qui a annoncé `kUiCapAttackElement` :
// à l'apparition d'un joueur dont l'élément n'est pas neutre, à son entrée en
// vue, et à toute la zone quand il change.
//
// 🔴 LE SERVEUR TRANCHE CE QUI SE VOIT : sur une carte d'affrontement et
// pendant un duel, il ne dit l'élément qu'au joueur lui-même, et « aucun » aux
// autres quand la règle tombe sur place. Un paquet qui nomme un acteur inconnu
// est lu et ignoré : voir `sim::World::TellAttackElement`.
[[nodiscard]] FrameOutcome ApplyAttackElementFrame(const Frame& frame, sim::World& world);

}  // namespace ro::net
