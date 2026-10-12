#pragma once

#include <cstdint>
#include <string_view>

#include "ro/core/error.h"
#include "ro/net/connection.h"
#include "ro/net/world_update.h"
#include "ro/sim/banned_words.h"
#include "ro/sim/entry_queue.h"
#include "ro/sim/talk_mute.h"
#include "ro/sim/world_fwd.h"

namespace ro::net {

// ── LE CHAMP DE BATAILLE : LA FILE ET LE COMBAT ─────────────────────────────
//
// Deux traducteurs, consultés à part du monde : la FILE d'inscription, qui vit
// hors de lui (`sim::EntryQueue`), et le COMBAT, qu'il porte
// (`sim::World::battlefield`). Chacun rend `kUnhandled` pour ce qui n'est pas à
// lui.

// `ZC 0x08D8`, `0x08D9`, `0x08DB`, `0x08DF`, `0x08E1` et `0x090E`.
[[nodiscard]] FrameOutcome ApplyEntryQueueFrame(const Frame& frame, sim::EntryQueue& queue,
                                                std::uint32_t now_ms);

// `ZC 0x02DC` (la parole, écrite dans la conversation avec sa bulle), `0x02DD`
// (les camps), `0x02DE` (le score), `0x02DF` (les coéquipiers, nous exceptés)
// et `0x0A0E` (la vie d'un coéquipier en vue). Les mots de `banned` se masquent
// dans la parole ; nul, rien ne se masque. `mute` en tait la ligne, pas la
// bulle : voir `sim::TalkMute`.
[[nodiscard]] FrameOutcome ApplyBattlefieldFrame(const Frame& frame, sim::World& world,
                                                 const sim::BannedWords* banned,
                                                 std::uint32_t now_ms, sim::TalkMute mute = {});

// ── LES VERBES ──────────────────────────────────────────────────────────────
//
// `field_name` est le nom que le SERVEUR donne au terrain, en UTF-8 ; il part
// en page de codes du jeu. Refusé vide, ou trop long pour ses 24 octets.

// « Je m'inscris », seul, avec mon groupe ou avec ma guilde.
[[nodiscard]] core::Result<void> RequestEntryQueueApply(Connection& link,
                                                        sim::EntryQueueApplyKind kind,
                                                        std::string_view field_name);
// « Je quitte la file. »
[[nodiscard]] core::Result<void> RequestEntryQueueCancel(Connection& link,
                                                         std::string_view field_name);
// « J'entre » ou « je refuse » : la réponse à l'invitation.
[[nodiscard]] core::Result<void> AnswerLobbyAdmission(Connection& link, bool accept,
                                                      std::string_view field_name,
                                                      std::string_view lobby_name);
// « Combien sommes-nous en file ? » Le serveur répond par `ZC 0x08D9`.
[[nodiscard]] core::Result<void> RequestEntryQueueRanking(Connection& link,
                                                          std::string_view field_name);

}  // namespace ro::net
