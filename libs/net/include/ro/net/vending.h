#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include "ro/core/error.h"
#include "ro/net/connection.h"
#include "ro/net/world_update.h"
#include "ro/sim/inventory.h"
#include "ro/sim/vending.h"

namespace ro::net {

// L'ÉCHOPPE DE VENTE : six paquets reçus, cinq émis. Voir `ro/sim/vending.h`
// pour la préparation, le journal et la visite.
//
// Ce traducteur lit aussi, SANS LES CONSOMMER, la disparition d'une enseigne
// (le monde l'efface, lui en tire la fin d'une visite) et les deux
// changements de carte. Il rend donc `kUnhandled` pour eux.
[[nodiscard]] FrameOutcome ApplyVendingFrame(const Frame& frame, sim::Vending& vending);

// Le plafond d'une trame émise que le client d'origine s'impose.
inline constexpr std::size_t kVendingMostFrameBytes = 0x800;

// ── LES VERBES ──────────────────────────────────────────────────────────────
//
// Ils émettent, et n'appliquent au modèle que ce qui empêche d'émettre deux
// fois. Chacun REFUSE, sans rien envoyer, ce que le serveur rejetterait en
// silence ou fausserait : les contrôles sont ceux de `sim::CheckVendingSetup`
// et `sim::CheckVendingBasket`, que la fenêtre appelle d'abord pour dire
// pourquoi.

// « Ouvre mon échoppe. » Le titre part COUPÉ par `sim::FitVendingTitle`, et
// c'est ce titre coupé que le modèle retient. Refusé hors préparation, demande
// déjà en route, ou au moindre défaut. `cart` est le contenu de la charrette,
// `zeny` ce que possède le joueur.
[[nodiscard]] core::Result<void> RequestVendingOpen(Connection& link, sim::Vending& vending,
                                                    std::string_view title_utf8,
                                                    std::span<const sim::VendingSetupLine> lines,
                                                    std::span<const sim::InventoryItem> cart,
                                                    std::int64_t zeny);

// « J'abandonne la préparation. » Voir `sim::Vending::TakeFarewell` : c'est
// après `CancelPreparation`, ou après un changement de carte, que ceci part.
[[nodiscard]] core::Result<void> SayVendingPreparationCancelled(Connection& link);

// « Je ferme mon échoppe. » ⚠ LE SERVEUR NE RÉPONDANT PAS, le modèle la ferme à
// l'envoi. Refusé si aucune échoppe n'est ouverte.
[[nodiscard]] core::Result<void> RequestVendingShopClose(Connection& link, sim::Vending& vending);

// « Montre-moi l'échoppe de ce vendeur », par son identifiant de compte — celui
// de son acteur et de son enseigne. La réponse ouvre la visite ; un vendeur
// absent n'en vaut aucune.
[[nodiscard]] core::Result<void> RequestVendingList(Connection& link, std::uint32_t vendor_aid);

// « J'achète ce panier » à l'échoppe visitée : les lignes d'une même offre sont
// fusionnées, puis l'achat part, SUIVI d'une nouvelle demande de liste (voir
// `sim::Vending::NotePurchaseSent`). Refusé sans visite, avec un achat encore
// en route, ou au moindre défaut du panier.
[[nodiscard]] core::Result<void> RequestVendingPurchase(
    Connection& link, sim::Vending& vending, std::span<const sim::VendingBasketLine> basket,
    std::int64_t zeny);

}  // namespace ro::net
