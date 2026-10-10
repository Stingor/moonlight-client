#pragma once

#include "ro/core/error.h"
#include "ro/net/connection.h"
#include "ro/net/world_update.h"
#include "ro/sim/barter.h"

namespace ro::net {

// LE TROC : deux listes et un résultat reçus, deux achats et deux fermetures
// émis. Voir `ro/sim/barter.h` pour la fermeture due et pour l'achat qui ferme.
//
// 🔴 LE RÉSULTAT EST PARTAGÉ avec la boutique de PNJ à zeny : même opcode, même
// octet. Ce traducteur ne le prend QUE si le troc attend un résultat — un achat
// est parti — et rend sinon `kUnhandled`. Il doit donc être consulté AVANT celui
// de la boutique à zeny, qui prendrait tout.
//
// Il lit aussi les deux changements de carte, SANS LES CONSOMMER : le monde les
// applique, et lui en tire la fermeture du troc. Il rend donc `kUnhandled` pour
// eux.
[[nodiscard]] FrameOutcome ApplyBarterFrame(const Frame& frame, sim::Barter& barter);

// ── LES VERBES ──────────────────────────────────────────────────────────────

// « J'échange tout le panier. » La ligne dépend de la génération :
//
//   · simple : objet, quantité, index d'inventaire, index de boutique ;
//   · étendu : objet, index de boutique, quantité.
//
// ⚠ LE SERVEUR NE LIT QUE L'INDEX DE BOUTIQUE ET LA QUANTITÉ
// (`clif_parse_barter_buy`, `clif_parse_barter_extended_buy`). L'objet envoyé
// est celui de la liste ; l'index d'inventaire du troc simple, que rien ne lit
// et dont le sens n'a pas été établi, part à ZÉRO.
//
// Refuse d'émettre ce que le serveur rejetterait en silence
// (`sim::CheckBarterCartShape`) et une trame au-delà du plafond du client
// d'origine. Au départ, le troc se ferme et attend son résultat
// (`sim::Barter::MarkPurchaseSent`) : c'est aussi ce qui empêche d'émettre deux
// fois. La fermeture due part ensuite par `SayBarterFarewells`, APRÈS l'achat —
// arrivée avant, elle ferait ignorer l'achat.
[[nodiscard]] core::Result<void> RequestBarterPurchase(Connection& link, sim::Barter& barter);

// « J'ai fermé le troc de cette génération. »
[[nodiscard]] core::Result<void> SayBarterClosed(Connection& link,
                                                 sim::BarterGeneration generation);

// Émet toutes les fermetures dues (`sim::Barter::TakeFarewell`).
[[nodiscard]] core::Result<void> SayBarterFarewells(Connection& link, sim::Barter& barter);

}  // namespace ro::net
