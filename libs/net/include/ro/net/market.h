#pragma once

#include <cstdint>
#include <optional>
#include <span>

#include "ro/core/error.h"
#include "ro/net/connection.h"
#include "ro/net/world_update.h"
#include "ro/sim/market.h"

namespace ro::net {

// ── LE MARCHÉ : `CZ 0x0F41` → `ZC 0x0F42` (ADR 0114) ────────────────────────
//
//   CZ [op][len:2][query_id:2][sorte:1][famille:1][raff_min:1][raff_max:1]
//      [drapeaux:1][prix_min:4][prix_max:4][échoppe:4][marchand:4]
//      puis un identifiant d'objet de 4 octets par objet cherché
//   ZC [op][len:2][query_id:2][statut:1][sorte:1][trame:2][trames:2][total:2]
//      [tronqué:1][échoppe:4][marchand:4][catalogue:1][saisie:50]
//      puis les entrées de 201 octets
//
// La sorte 2 est celle des boutiques de PNJ ; l'échoppe est alors le PNJ.
//
// 🔴 RÉSERVÉ À QUI ANNONCE `kUiCapMarket` : le serveur n'écrit `0x0F42` qu'à
// lui, et ignore la demande d'un autre.

// L'opcode de la réponse, pour qui doit reconnaître une trame qui ne se décode
// pas.
inline constexpr std::uint16_t kMarketReplyOpcode = 0x0F42;

// « Que vend-on ? » — une recherche, ou le contenu d'une échoppe. Le modèle
// attend la réponse dès l'envoi. Refusé sans rien envoyer si la demande porte
// plus de `sim::kMarketMostIds` identifiants.
[[nodiscard]] core::Result<void> RequestMarket(Connection& link, sim::Market& market,
                                               sim::MarketQuery query);

// Une trame de réponse décodée, ou rien quand elle ne se découpe pas en entrées
// entières, ou porte un statut ou une sorte inconnus.
[[nodiscard]] std::optional<sim::MarketReplyPart> DecodeMarketReply(
    std::span<const std::uint8_t> frame);

// La trame, versée au modèle. `kUnhandled` pour un autre opcode.
[[nodiscard]] FrameOutcome ApplyMarketFrame(const Frame& frame, sim::Market& market);

}  // namespace ro::net
