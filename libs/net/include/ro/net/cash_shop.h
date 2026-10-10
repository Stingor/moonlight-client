#pragma once

#include <cstdint>
#include <span>

#include "ro/core/error.h"
#include "ro/net/connection.h"
#include "ro/net/world_update.h"
#include "ro/sim/cash_shop.h"
#include "ro/sim/hat_effect_items.h"

namespace ro::net {

// LA BOUTIQUE DE VOTE : trois paquets reçus, quatre paquets émis. Voir
// `ro/sim/cash_shop.h` pour la bascule et pour la fermeture qu'on doit au
// serveur.
//
// Ce traducteur lit aussi les deux changements de carte, SANS LES CONSOMMER :
// le monde les applique, et lui en tire la fermeture de la boutique. Il rend
// donc `kUnhandled` pour eux.
[[nodiscard]] FrameOutcome ApplyCashShopFrame(const Frame& frame, sim::CashShop& shop);

// LA CARTE DES ITEMS À EFFET DE CHAPEAU, que la boutique lit pour ranger à part
// les costumes qui se montrent par un effet. Voir `ro/sim/hat_effect_items.h`.
[[nodiscard]] FrameOutcome ApplyHatEffectItemsFrame(const Frame& frame, sim::HatEffectItems& items);

// ── LES VERBES ──────────────────────────────────────────────────────────────
//
// ⚠ ILS ÉMETTENT, ET RIEN DE PLUS : les points ne bougent qu'au résultat du
// serveur, qui revalide tout.

// « Ouvre-moi la boutique », sur cet onglet. ⚠ LE MÊME PAQUET LA REFERME si elle
// est ouverte : c'est la réponse qui bascule.
[[nodiscard]] core::Result<void> RequestCashShopOpen(Connection& link, std::uint32_t tab);

// « Envoie-moi le catalogue. » ⚠ LE SERVEUR NE RÉPOND QU'UNE FOIS PAR SESSION.
[[nodiscard]] core::Result<void> RequestCashShopCatalogue(Connection& link);

// « J'ai fermé la boutique. » Voir `sim::CashShop::TakeFarewell`.
[[nodiscard]] core::Result<void> SayCashShopClosed(Connection& link);

// Une ligne d'achat : l'item, sa quantité, et l'onglet où on l'a pris — le
// serveur y cherche le prix.
struct CashShopPurchaseLine {
  std::uint32_t item_id = 0;
  std::uint32_t amount = 0;
  std::uint16_t tab = 0;
};

// « J'achète tout cela. »
//
// 🔴 `event_points` EST CE QU'ON DÉPENSE EN POINTS D'ÉVÉNEMENT, pas le solde :
// le serveur prélève le reste du total en points de vote, sans borner ce
// montant au prix. Envoyer tout le solde viderait les points d'événement et
// fausserait le compte des points de vote.
[[nodiscard]] core::Result<void> RequestCashShopPurchase(
    Connection& link, std::uint32_t event_points, std::span<const CashShopPurchaseLine> lines);

}  // namespace ro::net
