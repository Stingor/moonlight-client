#pragma once

#include <cstdint>

#include "ro/net/connection.h"
#include "ro/net/world_update.h"
#include "ro/sim/memorial_dungeon.h"

namespace ro::net {

// ── L'INSTANCE (« MEMORIAL DUNGEON ») ───────────────────────────────────────
//
// `ZC 0x02CB` (réservée), `0x02CC` (la file avance, ou la réservation tombe),
// `0x02CD` (ouverte), `0x02CE` (la fermeture à vide change, ou l'instance n'est
// plus) et `0x0F40` (le détail que seul ce client reçoit, sous
// `kUiCapInstanceInfo`). `now_unix_s` est l'heure de la machine : elle date
// l'écart avec l'horloge du serveur. Rend `kUnhandled` pour le reste.
[[nodiscard]] FrameOutcome ApplyMemorialDungeonFrame(const Frame& frame,
                                                     sim::MemorialDungeon& dungeon,
                                                     std::int64_t now_unix_s);

// « Détruis mon instance. » ⚠ LE SERVEUR SE TAIT S'IL REFUSE : hors du chef du
// groupe, du maître de la guilde ou du propriétaire, ou depuis une carte
// d'instance.
void RequestMemorialDungeonDestroy(Connection& link);

}  // namespace ro::net
