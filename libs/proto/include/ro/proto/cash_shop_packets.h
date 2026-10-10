#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "ro/core/error.h"

namespace ro::proto {

// ── LA BOUTIQUE DE VOTE : les deux paquets qui n'ont pas de structure ──────────
//
// Les cinq autres paquets de la boutique (ouverture, catalogue, achat, résultat)
// sont décrits par le serveur et engendrés dans `map_packets.h`. Ces deux-là
// n'y figurent pas : le serveur ne lit que leur opcode. Ils sont donc écrits ici,
// et recoupés avec ce que sa table déclare lire.

// « Envoie-moi le catalogue. » Le serveur répond par une série de
// `ZcAckSchedulerCashitem`, un paquet par onglet — et ⚠ UNE SEULE FOIS PAR
// SESSION : une seconde demande reste sans réponse jusqu'à la prochaine entrée
// en jeu.
struct CashShopCatalogueRequest {
  static constexpr std::uint16_t kOpcode = 0x08C9;
  static constexpr std::size_t kWireSize = 2;
};

// « J'ai fermé la boutique. »
//
// 🔴 CE PAQUET N'EST PAS DÉCORATIF. L'ouverture pose au personnage l'état d'une
// boutique de PNJ ouverte, qui lui interdit de marcher et d'attaquer ; seule
// cette fermeture le lève.
struct CashShopCloseNotice {
  static constexpr std::uint16_t kOpcode = 0x084A;
  static constexpr std::size_t kWireSize = 2;
};

[[nodiscard]] core::Result<std::vector<std::uint8_t>> Encode(
    const CashShopCatalogueRequest& request);
[[nodiscard]] core::Result<std::vector<std::uint8_t>> Encode(const CashShopCloseNotice& notice);

}  // namespace ro::proto
