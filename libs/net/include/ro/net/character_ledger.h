#pragma once

#include <optional>

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
//
// Les accusés du jeu de rechange — pièce rangée (`0x0A98`), sortie (`0x0A9A`),
// échange complet (`0x0A9D`) — le tiennent à jour. ⚠ LE SERVEUR EN ENVOIE SANS
// DEMANDE : poser une pièce rangée dans le jeu l'en sort, et l'accusé de cette
// sortie précède celui de la pose.
[[nodiscard]] FrameOutcome ApplyLedgerFrame(const Frame& frame, sim::CharacterLedger& ledger);

// CE QU'UN ACCUSÉ DU JEU DE RECHANGE DIT dans la discussion, ou rien : autre
// trame, échange accordé — il ne s'écrit pas —, ou code que le client d'origine
// tait.
//
// ⚠ UN REFUS DE SORTIE SORT QUAND MÊME LA PIÈCE, comme chez le client d'origine.
[[nodiscard]] std::optional<sim::EquipSwitchNews> EquipSwitchNewsOf(const Frame& frame);

}  // namespace ro::net
