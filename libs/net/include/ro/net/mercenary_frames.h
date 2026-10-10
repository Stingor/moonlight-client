#pragma once

#include <cstdint>

#include "ro/core/error.h"
#include "ro/net/connection.h"
#include "ro/net/world_update.h"
#include "ro/sim/mercenary.h"

namespace ro::net {

// LES TRAMES DU MERCENAIRE.
//
// Ce qui le fait paraître : sa fiche (`0x029B`), ses compétences (`0x029D`), une
// valeur qui change (`0x02A2`). Ce qui le fait partir : le message de fin de
// contrat ou de mort (`0x0291`), et la disparition de son acteur pour cause de
// mort (`0x0080`, genre 1).
//
// ⚠ LE SERVEUR N'ENVOIE RIEN QUAND LE JOUEUR LE RENVOIE : c'est au client de
// l'oublier en envoyant l'ordre. Le changement de carte, lui, se lit dans le
// monde.

// L'heure à la réception : l'horloge du client, et l'heure murale qui date la
// fin du contrat.
struct MercenaryClock {
  std::uint32_t now_ms = 0;
  std::int64_t now_unix_s = 0;
};

[[nodiscard]] FrameOutcome ApplyMercenaryFrame(const Frame& frame, sim::MercenaryBook& book,
                                               MercenaryClock clock);

// « Statut du mercenaire » : `0x029F`, commande 1. ⚠ LE SERVEUR L'IGNORE : rien
// ne revient. Le client l'envoie quand même en ouvrant la fiche.
[[nodiscard]] core::Result<void> RequestMercenaryInfo(Connection& link);

// « Renvoie-le » : `0x029F`, commande 2. Le serveur met fin au contrat sans
// répondre.
[[nodiscard]] core::Result<void> RequestMercenaryDismiss(Connection& link);

}  // namespace ro::net
