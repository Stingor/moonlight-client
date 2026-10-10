#pragma once

#include "ro/core/error.h"
#include "ro/net/connection.h"
#include "ro/net/world_update.h"
#include "ro/sim/player_config.h"

namespace ro::net {

// ── « MONTRER MON ÉQUIPEMENT » ET « VOIR LES COSTUMES » ──────────────────────
//
// `CZ 0x02D8` demande, `ZC 0x02D9` répond en écho. L'état de l'équipement
// s'annonce aussi par `ZC 0x02DA` à l'entrée sur le serveur de carte ; celui des
// costumes, par l'écho, à chaque carte. Le serveur ne refuse jamais ces deux
// réglages.

// Lit l'annonce et l'écho de ces deux réglages, et l'écho de l'appel d'urgence
// (`/call`), que le serveur redit à chaque connexion.
//
// ⚠ L'ÉCHO DE L'ALIMENTATION DES COMPAGNONS N'EST PAS LU ICI : il reste NON
// TRAITÉ pour ce traducteur, et ce sont les fiches des compagnons qui le lisent.
[[nodiscard]] FrameOutcome ApplyConfigFrame(const Frame& frame, sim::PlayerConfig& config);

// Montrer ou cacher notre équipement aux autres joueurs.
//
// ⚠ ÉMET ET S'ARRÊTE LÀ : l'état change à l'écho du serveur, pas ici.
[[nodiscard]] core::Result<void> RequestShowEquipment(Connection& link, bool shown);

// Montrer ou masquer nos costumes, à nous comme à tout le voisinage.
//
// ⚠ ÉMET ET S'ARRÊTE LÀ, comme sa sœur. Le serveur répond d'abord par les
// nouvelles apparences, puis par l'écho.
[[nodiscard]] core::Result<void> RequestShowCostumes(Connection& link, bool shown);

// Accepter ou refuser l'appel d'urgence (`/call`).
//
// ⚠ ÉMET ET S'ARRÊTE LÀ, comme ses sœurs : l'état change à l'écho.
[[nodiscard]] core::Result<void> RequestCallable(Connection& link, bool callable);

}  // namespace ro::net
