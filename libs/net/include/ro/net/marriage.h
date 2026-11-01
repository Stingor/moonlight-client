#pragma once

#include <cstdint>

#include "ro/net/world_update.h"
#include "ro/sim/world.h"

namespace ro::net {

// ── LE MARIAGE : CE QUE LE SERVEUR EN DIT ───────────────────────────────────
//
// Le mariage se fait par le script d'un PNJ : le client n'y demande rien. Il
// reçoit trois trames.
//
//   ZC 0x01EA [op][acteur:4]   la cérémonie : l'effet 347 sur cet acteur, le
//                              joueur compris
//   ZC 0x0205 [op][nom:24]     le divorce, à chacun des deux : une ligne jaune
//                              qui nomme le joueur et son ex-conjoint
//   ZC 0x01E6 [op][nom:24]     le nom du conjoint, à l'appel du conjoint :
//                              gardé par le client sans être montré, donc
//                              ignoré
//
// Même contrat que `ApplyAdoptionFrame` : ce que ce traducteur ne reconnaît pas
// rend `kUnhandled`.
[[nodiscard]] FrameOutcome ApplyMarriageFrame(const Frame& frame, sim::World& world,
                                              std::uint32_t now_ms);

}  // namespace ro::net
