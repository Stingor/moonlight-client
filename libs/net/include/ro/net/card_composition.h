#pragma once

#include <cstddef>
#include <cstdint>

#include "ro/core/error.h"
#include "ro/net/connection.h"
#include "ro/net/world_update.h"
#include "ro/sim/world_fwd.h"

namespace ro::net {

// LE SERTISSAGE DES CARTES, traduit dans le monde. Voir `ro/sim/card_composition.h`
// pour ce que le client décide — rien — et ce qu'il retient.
//
// Trois réponses : la liste des pièces qu'une carte peut recevoir, l'accusé
// d'un sertissage, et la liste inverse du sertissage rapide.
//
// 🔴 L'ACCUSÉ EST LE SEUL MOT DU SERVEUR SUR LE SAC. Il retire la carte sans
// paquet de retrait et ne relistera pas la pièce : c'est ici que le sac perd la
// carte et que la pièce la reçoit.
[[nodiscard]] FrameOutcome ApplyCardCompositionFrame(const Frame& frame, sim::World& world);

// ── Ce que la structure engendrée ne porte pas ─────────────────────────────

// LA LISTE DES PIÈCES. ⚠ LE SERVEUR L'ÉCRIT À LA MAIN, sans structure : son
// numéro se nomme donc ici, et son corps est une suite de places du sac.
inline constexpr std::uint16_t kCardCandidatesOpcode = 0x017B;
inline constexpr std::size_t kCardCandidatesFixedSize = 4;

// La liste inverse : `[equip:2][count:2]` après l'en-tête, puis `count` places
// de carte. Le générateur s'arrête au bloc fixe.
inline constexpr std::size_t kCompatibleCardsFixedSize = 8;

// Ce que l'accusé écrit dans son octet de résultat. ⚠ TOUTE AUTRE VALEUR N'A
// AUCUN EFFET, ni message ni changement du sac : c'est ce que fait le client.
inline constexpr std::uint8_t kCardInserted = 0;
inline constexpr std::uint8_t kCardRefused = 1;

// ── LES VERBES ──────────────────────────────────────────────────────────────
//
// ⚠ ILS ÉMETTENT, ET RIEN DE PLUS, comme ceux du sac.

// « Quelles pièces cette carte peut-elle recevoir ? »
//
// ⚠ LE SERVEUR SE TAIT QUAND AUCUNE NE CONVIENT, et pendant un échange. Aucune
// réponse n'est donc garantie : c'est pourquoi l'appelant note la carte dans le
// monde, et la réponse — si elle vient — l'y retrouve.
[[nodiscard]] core::Result<void> RequestCardCandidates(Connection& link, std::uint16_t card_slot);

// « Sertis cette carte sur cette pièce. »
[[nodiscard]] core::Result<void> RequestInsertCard(Connection& link, std::uint16_t card_slot,
                                                   std::uint16_t equip_slot);

// « Quelles cartes du sac cette pièce peut-elle recevoir ? » — le sertissage
// RAPIDE. ⚠ UN PAQUET DU SERVEUR MOONLIGHT, qui ne répond qu'à un client
// reconnu par la poignée de main.
[[nodiscard]] core::Result<void> RequestCompatibleCards(Connection& link, std::uint16_t equip_slot);

}  // namespace ro::net
