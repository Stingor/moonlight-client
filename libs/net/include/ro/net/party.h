#pragma once

#include <cstdint>
#include <string_view>

#include "ro/core/error.h"
#include "ro/net/connection.h"
#include "ro/net/world_update.h"
#include "ro/sim/banned_words.h"
#include "ro/sim/party_roster.h"

namespace ro::net {

// ── LE GROUPE : CE QUE LE SERVEUR EN DIT ────────────────────────────────────
//
// 🔴 LA LISTE N'EST PAS DANS LE MONDE — `world.h` touche son plafond —, et ce
// traducteur ne passe donc pas par `ApplyFrame`. L'appelant lui soumet CHAQUE
// trame, après le monde : il en lit que le monde lit aussi — la vie d'un membre,
// la réponse sur l'entité désignée —, et ce qu'il ne reconnaît pas rend
// `kUnhandled`.
//
// ⚠ `own_account_id` EST LE COMPTE JOUÉ. Le départ d'un membre est rediffusé à
// tout le groupe sous son compte : quand c'est le nôtre, c'est NOUS qui partons,
// et la liste se vide.
//
// ⚠ `now_ms` EST L'HORLOGE DE L'APPELANT : elle date le PS d'un membre, que le
// serveur ne donne que sur demande. Sans horloge, la réponse n'est pas lue.
//
// LES MOTS INTERDITS de `banned` se masquent dans le nom du groupe — le sien,
// et celui du groupe qui invite. Sans liste, rien ne se masque.
[[nodiscard]] FrameOutcome ApplyPartyFrame(const Frame& frame, sim::PartyRoster& party,
                                           std::uint32_t own_account_id, std::uint32_t now_ms,
                                           const sim::BannedWords* banned);
[[nodiscard]] FrameOutcome ApplyPartyFrame(const Frame& frame, sim::PartyRoster& party,
                                           std::uint32_t own_account_id, std::uint32_t now_ms);
[[nodiscard]] FrameOutcome ApplyPartyFrame(const Frame& frame, sim::PartyRoster& party,
                                           std::uint32_t own_account_id);

// ── CE QU'ON LUI DEMANDE ────────────────────────────────────────────────────
//
// ⚠ TOUT ÉMET ET S'ARRÊTE LÀ : le serveur revalide chaque geste — chef ou non,
// carte verrouillée, donjon — et c'est sa réponse qui change la liste.
//
// ⚠ LES NOMS ARRIVENT EN UTF-8 et partent en 1252 : le serveur les compare
// octet par octet à ceux de sa base.

// Les deux règles d'objets se choisissent à la création ; l'EXP part toujours
// chacun pour soi.
[[nodiscard]] core::Result<void> RequestPartyCreation(Connection& link, std::string_view name,
                                                      bool pickup_shared, bool loot_shared);
// INVITER PAR LE NOM : le serveur cherche le personnage, qui doit être en ligne.
[[nodiscard]] core::Result<void> RequestPartyInvite(Connection& link, std::string_view name);
[[nodiscard]] core::Result<void> AnswerPartyInvitation(Connection& link, std::uint32_t party_id,
                                                       bool accept);
// ⚠ LE PAQUET NU. Un chef qui quitte doit d'abord passer la direction : voir
// `sim::PlanLeave`, que l'appelant consulte avant.
[[nodiscard]] core::Result<void> RequestPartyLeave(Connection& link);
// Le serveur exige le compte ET le nom du membre visé.
[[nodiscard]] core::Result<void> RequestPartyExpel(Connection& link, std::uint32_t account_id,
                                                   std::string_view name);
[[nodiscard]] core::Result<void> RequestPartyLeader(Connection& link, std::uint32_t account_id);
[[nodiscard]] core::Result<void> RequestPartyOptions(Connection& link,
                                                     const sim::PartyOptions& options);
// Refuser d'office toute invitation, ou les accepter de nouveau.
[[nodiscard]] core::Result<void> RequestPartyInviteRefusal(Connection& link, bool refuse);

// LE PS ET LA VIE D'UN MEMBRE se demandent par `RequestTargetInfo`
// (`ro/net/target_info.h`), avec son compte : le serveur les donne à qui est de
// son groupe, et la réponse est lue ici comme par le monde.

}  // namespace ro::net
