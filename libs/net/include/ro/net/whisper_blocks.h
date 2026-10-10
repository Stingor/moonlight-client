#pragma once

#include <cstdint>
#include <string_view>

#include "ro/core/error.h"
#include "ro/net/connection.h"
#include "ro/net/world_update.h"
#include "ro/sim/whisper_blocks.h"
#include "ro/sim/world_fwd.h"

namespace ro::net {

// ── BLOQUER LES CHUCHOTEMENTS : CE QUE LE SERVEUR EN DIT ────────────────────
//
// Les trois réponses — à une demande par nom, à « tout bloquer » ou « tout
// autoriser », et la liste redite — mettent à jour `blocks`, et écrivent dans
// la conversation ce que le client d'origine y écrit. Toute autre trame rend
// `kUnhandled`.
//
// ⚠ LE BLOCAGE RÉUSSI D'UN NOM NE DIT RIEN : le client d'origine se tait, et
// c'est le libellé du menu qui change. Son déblocage réussi, lui, se dit.
[[nodiscard]] FrameOutcome ApplyWhisperBlockFrame(const Frame& frame, sim::WhisperBlockList& blocks,
                                                  sim::World& world, std::uint32_t now_ms);

// ── CE QU'ON LUI DEMANDE ────────────────────────────────────────────────────

// BLOQUER OU AUTORISER CE NOM, en UTF-8, et noter la demande pour que la
// réponse la retrouve. Refusé avant d'envoyer : un nom vide, un nom qu'on ne
// sait pas écrire en 1252, et un nom trop long — le serveur le tronquerait, et
// bloquerait quelqu'un d'autre.
[[nodiscard]] core::Result<void> RequestWhisperBlock(Connection& link,
                                                     sim::WhisperBlockList& blocks,
                                                     std::string_view name,
                                                     sim::WhisperBlockWay way);

// BLOQUER OU AUTORISER TOUT LE MONDE.
[[nodiscard]] core::Result<void> RequestWhisperBlockEveryone(Connection& link,
                                                             sim::WhisperBlockWay way);

// REDEMANDER LA LISTE : sa réponse l'écrit dans la conversation.
[[nodiscard]] core::Result<void> RequestWhisperBlockList(Connection& link);

}  // namespace ro::net
