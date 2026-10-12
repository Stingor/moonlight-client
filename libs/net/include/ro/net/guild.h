#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

#include "ro/core/error.h"
#include "ro/net/connection.h"
#include "ro/net/world_update.h"
#include "ro/sim/banned_words.h"
#include "ro/sim/guild.h"
#include "ro/sim/world_fwd.h"

namespace ro::net {

// ── LA GUILDE : CE QUE LE SERVEUR EN DIT ────────────────────────────────────
//
// 🔴 LE CARNET N'EST PAS DANS LE MONDE — `world.h` touche son plafond —, et ce
// traducteur ne passe donc pas par `ApplyFrame`. L'appelant lui soumet CHAQUE
// trame, après le monde : il en lit que le monde traite aussi — les apparitions
// d'unité, pour la version d'emblème de leur guilde, et 0x0B47 —, et ce qu'il
// ne reconnaît pas rend `kUnhandled`.
//
// ⚠ `own_char_id` EST LE PERSONNAGE JOUÉ. Le départ et l'expulsion d'un membre
// sont rediffusés à toute la guilde sous le numéro du personnage concerné :
// quand c'est le nôtre, c'est NOUS qui partons, et le carnet se vide.
//
// LES MOTS INTERDITS de `banned` se masquent dans ce que le client d'origine
// masque : le nom de la guilde, son avis, les noms de ses postes, ceux des
// guildes alliées ou ennemies, et celui de la guilde qui invite ou propose une
// alliance. Sans liste, rien ne se masque.
[[nodiscard]] FrameOutcome ApplyGuildFrame(const Frame& frame, sim::GuildBook& guild,
                                           std::uint32_t own_char_id,
                                           const sim::BannedWords* banned);
[[nodiscard]] FrameOutcome ApplyGuildFrame(const Frame& frame, sim::GuildBook& guild,
                                           std::uint32_t own_char_id);

// ── CE QU'ON LUI DEMANDE ────────────────────────────────────────────────────
//
// ⚠ TOUT ÉMET ET S'ARRÊTE LÀ : le serveur revalide chaque geste — droits du
// poste, drapeau de maître, carte verrouillée, guerre en cours — et c'est sa
// réponse, jamais notre supposition, qui change le carnet.
//
// ⚠ LES TEXTES ARRIVENT EN UTF-8 et sont convertis ici : les NOMS en 1252, que
// le serveur compare octet par octet à sa base, et les PHRASES — annonce,
// motif — comme le fil les accepte.

// LA FENÊTRE ENTIÈRE, redemandée d'un coup.
//
// ⚠ DANS CET ORDRE : les membres d'abord, les postes ENSUITE. La liste des
// membres revient sans nom de poste, que la liste des postes vient justement
// remplir ; les compétences, les expulsions, puis l'en-tête terminent. Puis
// les onglets que le serveur ouvre au personnage : il ne les pousse qu'au
// maître, et un changement de maître ne les redit pas.
[[nodiscard]] core::Result<void> RequestGuildWindow(Connection& link);

// Quitter, ou expulser un membre : le même paquet, seul l'opcode change.
[[nodiscard]] core::Result<void> RequestGuildLeave(Connection& link, std::uint32_t guild_id,
                                                   std::uint32_t account_id, std::uint32_t char_id,
                                                   std::string_view reason);
[[nodiscard]] core::Result<void> RequestGuildExpel(Connection& link, std::uint32_t guild_id,
                                                   std::uint32_t account_id, std::uint32_t char_id,
                                                   std::string_view reason);

// LE POSTE D'UN MEMBRE.
//
// 🔴 LE POSTE ZÉRO EST UN TRANSFERT DE DIRECTION, pas un changement de rang :
// le serveur le traduit en passation de la guilde entière. Les deux gestes
// partagent le paquet, et c'est l'appelant qui sait lequel il fait.
[[nodiscard]] core::Result<void> RequestGuildMemberPosition(Connection& link,
                                                            std::uint32_t account_id,
                                                            std::uint32_t char_id,
                                                            std::int32_t position);

[[nodiscard]] core::Result<void> RequestGuildNotice(Connection& link, std::uint32_t guild_id,
                                                    std::string_view subject,
                                                    std::string_view body);

// UN POSTE ÉDITÉ, tel que la fenêtre le renvoie.
struct GuildPositionEdit {
  std::int32_t position = 0;
  std::int32_t rights = 0;
  std::int32_t pay_rate = 0;
  std::string name;
};
[[nodiscard]] core::Result<void> RequestGuildPositions(Connection& link,
                                                       std::span<const GuildPositionEdit> edits);

[[nodiscard]] core::Result<void> RequestGuildRelationEnd(Connection& link, std::int32_t guild_id,
                                                         sim::GuildRelationKind kind);

[[nodiscard]] core::Result<void> RequestGuildCreation(Connection& link, std::uint32_t char_id,
                                                      std::string_view name);

// INVITER PAR LE NOM : le serveur cherche le personnage, qui doit être en ligne.
[[nodiscard]] core::Result<void> RequestGuildInvite(Connection& link, std::string_view name);

// PROPOSER UNE ALLIANCE, ou DÉCLARER ENNEMIE, la guilde de ce joueur. Réservés
// au maître, et le serveur le revérifie : un autre compte n'obtient rien.
//
// ⚠ ELLES VISENT UN JOUEUR, PAS UNE GUILDE : le serveur remonte de lui à sa
// guilde. L'alliance attend la réponse de l'autre maître ; l'hostilité est
// immédiate.
[[nodiscard]] core::Result<void> RequestGuildAlliance(Connection& link,
                                                      std::uint32_t target_account_id,
                                                      std::uint32_t own_account_id,
                                                      std::uint32_t own_char_id);
[[nodiscard]] core::Result<void> RequestGuildOpposition(Connection& link,
                                                        std::uint32_t target_account_id);

// RÉPONDRE À UNE INVITATION reçue.
[[nodiscard]] core::Result<void> AnswerGuildInvitation(Connection& link, std::int32_t guild_id,
                                                       bool accept);

// RÉPONDRE À UNE ALLIANCE PROPOSÉE, en nommant le compte qui l'a proposée.
[[nodiscard]] core::Result<void> AnswerGuildAlliance(Connection& link,
                                                     std::uint32_t inviter_account_id, bool accept);

// ── L'ISSUE D'UNE ALLIANCE OU D'UNE HOSTILITÉ ───────────────────────────────
//
// ZC 0x0173 et ZC 0x0181 : un numéro de verdict, que ce traducteur écrit dans
// la conversation. Il ne touche pas au carnet — après un succès, le serveur
// renvoie la liste des relations entière à chaque membre concerné.
//
// ⚠ UN VERDICT NE DIT PAS À QUI IL RÉPOND. L'auteur d'une proposition reçoit
// le même numéro pour un refus, une guilde pleine ou un départ : la phrase les
// nomme tous plutôt que d'en affirmer un. Un numéro inconnu n'écrit rien.
[[nodiscard]] FrameOutcome ApplyGuildRelationAnswerFrame(const Frame& frame, sim::World& world,
                                                         std::uint32_t now_ms);

// L'IMAGE DE L'EMBLÈME PAR LE FIL — le chemin historique. Le serveur ne répond
// que s'il en garde une.
[[nodiscard]] core::Result<void> RequestGuildEmblemImage(Connection& link, std::int32_t guild_id);

// « J'AI DÉPOSÉ UN NOUVEL EMBLÈME » : la version que le service web vient de
// rendre. Le serveur la publie alors à la guilde et aux alentours.
[[nodiscard]] core::Result<void> AnnounceGuildEmblem(Connection& link, std::int32_t guild_id,
                                                     std::int32_t version);

}  // namespace ro::net
