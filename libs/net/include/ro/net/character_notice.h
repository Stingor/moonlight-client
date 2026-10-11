#pragma once

#include <cstdint>

#include "ro/net/world_update.h"
#include "ro/sim/world_fwd.h"

namespace ro::net {

// CE QUE LE SERVEUR DE CARTE DIT DU PERSONNAGE ET QUE LE MONDE ÉCRIT.
//
// L'IDENTIFIANT DE COMPTE (`ZC_AID 0x0283`), redit dès la connexion : il ne
// sert qu'à vérifier celui que la phase de connexion a donné. Un désaccord se
// journalise et ne change rien — c'est celui-là qui désigne le joueur dans
// tous les paquets déjà lus.
//
// L'EXPÉRIENCE (`ZC_NOTIFY_EXP2 0x0ACC`), gagnée ou perdue : une ligne de la
// conversation par gain, rangée sous `ChatKind::kExp` : « '1200' d'expérience
// de base obtenus. » Le début suit le paramètre (base ou job), la fin le sens
// (« obtenus » ou « perdus »). Jaune pour un gain ordinaire, violette pour
// celui d'une quête. Un gain nul et celui d'un autre
// personnage ne s'écrivent pas ; un paramètre qui n'est ni la base ni le
// métier n'écrit que la fin de phrase.
[[nodiscard]] FrameOutcome ApplyCharacterNoticeFrame(const Frame& frame, sim::World& world,
                                                     std::uint32_t now_ms);

}  // namespace ro::net
