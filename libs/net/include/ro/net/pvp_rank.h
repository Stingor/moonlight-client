#pragma once

#include <cstddef>
#include <cstdint>

#include "ro/net/world_update.h"
#include "ro/sim/world_fwd.h"

namespace ro::net {

// LE CLASSEMENT PVP DE LA CARTE : `ZC_NOTIFY_RANKING 0x019a`.
//
//     019a <id>.L <rang>.L <total>.L
//
// 🔴 AUCUNE STRUCTURE ENGENDRÉE, et il n'y en aura pas : le serveur l'écrit
// champ par champ, en `WFIFOL`, sans déclarer de type. Le générateur ne peut
// rien en tirer, donc les décalages se nomment ici — comme pour `ZC 0x0F2D`.
//
// 🔴 IL ARRIVE EN ZONE, ET ON REÇOIT LE RANG DES AUTRES. `clif_pvpset` diffuse
// en AREA : chaque changement de rang d'un voisin nous parvient. Le client
// d'origine s'en sert pour poser son anneau sur l'acteur concerné, mais n'écrit
// ses deux champs d'AFFICHAGE que si l'identifiant est le sien. Prendre le
// dernier rang reçu pour le nôtre ferait sauter le compteur au rang du voisin
// qui vient de tuer quelqu'un.
//
// ⚠ ET LE SERVEUR SE TAIT TANT QUE RIEN NE CHANGE. `pc_calc_pvprank` bat toutes
// les secondes et ne parle que si le rang ou le nombre de joueurs a bougé : ne
// rien recevoir est le cas ordinaire.
[[nodiscard]] FrameOutcome ApplyPvpRankFrame(const Frame& frame, sim::World& world,
                                             std::uint32_t now_ms);

// ── LES DÉCALAGES, PUISQU'AUCUNE STRUCTURE NE LES PORTE ─────────────────────
//
// Taille totale sur le fil : quatorze octets, ce que `clif_packetdb` déclare.
inline constexpr std::size_t kPvpRankWireSize = 14;
inline constexpr std::size_t kPvpRankIdOffset = 2;
inline constexpr std::size_t kPvpRankRankOffset = 6;
inline constexpr std::size_t kPvpRankTotalOffset = 10;

}  // namespace ro::net
