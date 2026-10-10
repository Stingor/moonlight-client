#pragma once

#include "ro/net/world_update.h"
#include "ro/sim/npc_illustration.h"

namespace ro::net {

// ZC_SHOW_IMAGE : le script montre une illustration de PNJ (`cutin`), ou l'efface.
//
// Le nom reste en CP949 : c'est un nom de fichier des archives, pas un texte à
// lire. Rend `kUnhandled` pour toute autre trame.
[[nodiscard]] FrameOutcome ApplyNpcIllustrationFrame(const Frame& frame,
                                                     sim::NpcIllustrationSlot& slot);

}  // namespace ro::net
