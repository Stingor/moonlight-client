#pragma once

#include <span>

#include "ro/core/error.h"
#include "ro/net/connection.h"
#include "ro/net/world_update.h"
#include "ro/sim/npc_shop.h"
#include "ro/sim/npc_shop_cart.h"

namespace ro::net {

// LA BOUTIQUE D'UN PNJ : ce qui l'ouvre, ce qu'elle répond, et ce qu'on lui
// demande. Voir `ro/sim/npc_shop.h` pour les quatre sortes de boutiques et pour
// la fermeture qu'on doit au serveur, et `docs/commerces-protocole.md` §A.
//
// ⚠ LE RÉSULTAT D'ACHAT (`ZC_PC_PURCHASE_RESULT`) EST AUSSI CELUI DU TROC. Il
// n'est lu ici que si une boutique de PNJ l'attend ; sinon ce traducteur le
// laisse passer (`kUnhandled`).
//
// Ce traducteur lit aussi les deux changements de carte, SANS LES CONSOMMER :
// le monde les applique, et lui en tire la fermeture de la boutique.
[[nodiscard]] FrameOutcome ApplyNpcShopFrame(const Frame& frame, sim::NpcShop& shop);

// ── LES VERBES ──────────────────────────────────────────────────────────────
//
// ⚠ ILS ÉMETTENT, ET RIEN DE PLUS : les listes et les soldes ne bougent qu'aux
// réponses du serveur, qui revalide tout.

// « Montre-moi ce que tu vends » (`kBuy`) ou « ce que tu m'achètes »
// (`kSell`). ⚠ SEULEMENT POUR UNE BOUTIQUE NOMMÉE : une boutique ouverte par un
// script ne l'a jamais été, et le serveur n'accepterait pas d'identité inventée.
[[nodiscard]] core::Result<void> ChooseNpcShopSide(Connection& link, const sim::NpcShop& shop,
                                                   sim::NpcShopSide side);

// « J'achète tout cela. » Le paquet dépend de la boutique : à zeny, market, à
// points.
//
// 🔴 UNE BOUTIQUE À ZENY DÉSARMÉE SE RÉARME D'ABORD : le serveur l'oublie après
// chaque transaction, et un achat sans nouveau choix échouerait en « zeny
// insuffisant ». Le choix part donc devant, dans la même rafale — le serveur
// les lit dans l'ordre. Une boutique qu'on ne peut pas réarmer — ouverte par un
// script, et déjà servie — refuse ici, sans rien émettre.
//
// ⚠ UNE SEULE TRANSACTION À LA FOIS : une seconde avant le résultat de la
// première est refusée ici.
[[nodiscard]] core::Result<void> BuyFromNpcShop(Connection& link, sim::NpcShop& shop,
                                                std::span<const sim::NpcShopBuyLine> cart);

// « Je vends tout cela. » Même réarmement, même garde.
[[nodiscard]] core::Result<void> SellToNpcShop(Connection& link, sim::NpcShop& shop,
                                               std::span<const sim::NpcShopSellLine> cart);

// Le paquet de fermeture dû, s'il y en a un. Voir `sim::NpcShop::TakeFarewell`.
[[nodiscard]] core::Result<void> SayNpcShopFarewell(Connection& link,
                                                    sim::NpcShopFarewell farewell);

}  // namespace ro::net
