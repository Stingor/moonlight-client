#pragma once

#include "ro/net/world_update.h"
#include "ro/sim/character_ledger.h"

namespace ro::net {

// CE QUE LE SERVEUR DIT DU PERSONNAGE ET QU'AUCUNE FENÊTRE NE MONTRE ENCORE,
// gardé dans `sim::CharacterLedger` :
//
// - les packs d'émotions payants possédés (`ZC_ACK_BUY_CASH_EMOTION 0x0BED`),
//   un paquet par pack, redits à chaque connexion ;
// - le jeu d'équipement de rechange (`ZC_EQUIPSWITCH_LIST 0x0A9B`) ;
// - les points de réputation (`ZC_REPUTE_INFO 0x0B8D`), toutes les factions à
//   la connexion puis la seule qui change. Un paquet marqué en échec est lu et
//   ignoré.
[[nodiscard]] FrameOutcome ApplyLedgerFrame(const Frame& frame, sim::CharacterLedger& ledger);

}  // namespace ro::net
