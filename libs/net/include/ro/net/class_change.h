#pragma once

#include "ro/net/world_update.h"
#include "ro/sim/world_fwd.h"

namespace ro::net {

// UN PNJ OU UN MONSTRE CHANGE DE CLASSE, donc de sprite : `ZC_NPCSPRITE_CHANGE
// 0x01B0`.
//
// Le serveur l'envoie quand un script change l'apparence d'un PNJ
// (`setnpcdisplay`), quand un monstre se transforme, et quand un homoncule
// évolue ou mute. La nouvelle classe remplace l'ancienne, et le sexe revient à
// zéro.
//
// ⚠ JAMAIS UN JOUEUR : le serveur refuse d'envoyer une classe de joueur, et un
// déguisement (`@disguise`) passe par une disparition suivie d'une
// réapparition. Un paquet qui nommerait un joueur, ou un acteur hors de vue,
// est lu et ignoré.
[[nodiscard]] FrameOutcome ApplyClassChangeFrame(const Frame& frame, sim::World& world);

}  // namespace ro::net
