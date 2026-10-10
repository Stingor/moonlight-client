#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

#include "ro/core/error.h"
#include "ro/net/connection.h"
#include "ro/net/world_update.h"
#include "ro/sim/world_fwd.h"

namespace ro::net {

// LE DIALOGUE D'UN PNJ, dans les deux sens.
//
// C'est la première conversation du client : le serveur envoie une suite de
// lignes et de boutons, et le joueur RÉPOND. Les deux sens vivent ensemble
// parce qu'ils partagent une contrainte que ni l'un ni l'autre ne porte seul —
// le rang d'un choix ne vaut que pour le menu qui vient d'arriver.

// « Je parle à ce PNJ. » C'est ce qui OUVRE une conversation, et rien d'autre ne
// l'ouvre : sans cette trame, le serveur n'a aucune raison de dire quoi que ce
// soit, et la fenêtre de dialogue n'apparaît jamais.
//
// ⚠ NE L'ENVOYER QUE VERS UN PNJ. Le serveur fait de ce même paquet DEUX choses
// selon ce qu'il désigne : sur un PNJ il ouvre le script, mais sur un joueur ou
// un monstre il appelle `clif_parse_ActionRequest_sub` et ATTAQUE. Se tromper de
// cible ne montre pas une fenêtre vide, cela frappe quelqu'un.
[[nodiscard]] core::Result<void> ContactNpc(Connection& link, std::uint32_t npc_id);

// LA LONGUEUR UTILE D'UNE SAISIE DE TEXTE, en octets de la page de codes du jeu.
//
// ⚠ C'EST LE SERVEUR QUI LA FIXE, pas le champ : `clif_parse_NpcStringInput`
// recopie dans `sd->npc_str`, long de `CHATBOX_SIZE`, avec une fonction qui
// termine elle-même. Tout ce qui dépasse est jeté là-bas, en silence ; le
// couper ici est ce qui fait que le joueur voit ce qui partira vraiment.
inline constexpr std::size_t kNpcTextAnswerMaxBytes = 70;

// Les trames que le serveur envoie : une ligne (ZC_SAY_DIALOG et son jumeau
// secondaire), un « suivant », un « fermer », un `clear`, un menu, et les deux
// demandes de saisie.
//
// Même contrat que `ApplyFrame` ; `now_ms` est l'horloge de l'APPELANT — elle
// date la page en réception, dont le filet de publication se mesure en SILENCE
// (voir `sim::kNpcPageIdleMs`).
[[nodiscard]] FrameOutcome ApplyNpcDialogFrame(const Frame& frame, sim::World& world,
                                               std::uint32_t now_ms);

// « Suivant » : le script reprend. Le monde est AVANCÉ du même geste — la
// prochaine ligne effacera ce qui est écrit —, parce qu'oublier l'un des deux
// laisserait la fenêtre mentir jusqu'à la réponse du serveur.
[[nodiscard]] core::Result<void> AnswerNpcNext(Connection& link, sim::World& world);

// L'option choisie, désignée par sa POSITION D'AFFICHAGE, comptée à partir de
// zéro dans `NpcDialog::options`.
//
// ⚠ C'EST ICI QUE LE RANG SE TROUVE ET SE VÉRIFIE, et c'est le seul endroit qui
// le puisse : la couche des paquets ne connaît pas le menu, et l'interface ne
// connaît pas ce que le serveur fait d'un mauvais rang — il ÉJECTE le joueur.
// Une position sans rang est refusée ici, et rien ne part. Voir
// `sim::NpcMenuOption`, qui dit pourquoi les deux nombres diffèrent.
[[nodiscard]] core::Result<void> AnswerNpcMenu(Connection& link, sim::World& world,
                                               std::size_t option_index);

// Le nombre saisi, quand le script a demandé un `input` numérique.
[[nodiscard]] core::Result<void> AnswerNpcNumber(Connection& link, sim::World& world,
                                                 std::int32_t value);

// Le texte saisi. Il repart dans la page de codes du jeu, et coupé à ce que le
// serveur en gardera — voir `kNpcTextAnswerMaxBytes`.
[[nodiscard]] core::Result<void> AnswerNpcText(Connection& link, sim::World& world,
                                               std::string_view text);

// « Fermer » : la fenêtre s'en va, et le monde l'oublie.
//
// 🔴 CE N'EST PAS TOUJOURS LE MÊME PAQUET, et c'est le seul endroit qui sache
// choisir. Un script arrêté sur un `select` n'attend pas une fermeture, il
// attend un RANG : lui envoyer CZ_CLOSE_DIALOG laisse `sd->npc_id` posé, et le
// personnage ne lance plus aucune compétence jusqu'au changement de carte.
// L'annulation convenue — deux cent cinquante-cinq — est ce qui le termine.
[[nodiscard]] core::Result<void> LeaveNpcDialog(Connection& link, sim::World& world);

// CE QU'IL FAUT POUR PRENDRE CONGÉ D'UN PNJ QUAND LE DIALOGUE N'EST PLUS LÀ
// POUR LE DIRE.
//
// 🔴 QUITTER LA CARTE NE DÉLIE PAS LE SCRIPT. `pc_setpos` ne touche à
// `sd->npc_id` que pour un changement de SERVEUR de carte : sur le même, le
// script reste attaché, et tant qu'il l'est le personnage ne peut plus parler à
// personne — `npc_click` refuse d'ouvrir un second dialogue. Le serveur finit
// par le délier seul, au bout d'une minute (`SECURE_NPCTIMEOUT`) ; le mot qu'on
// lui dit vaut mieux qu'une minute d'attente.
struct NpcFarewell {
  std::uint32_t npc_id = 0;
  // Vrai si une liste de choix attendait encore : le serveur n'attend alors pas
  // une fermeture mais un RANG — voir `sim::kNpcMenuCancelled`.
  bool menu_pending = false;
};

// CE QU'ON DEVRAIT DIRE SI LA CONVERSATION S'INTERROMPAIT MAINTENANT. Rien, si
// personne ne parle.
//
// 🔴 IL SE LIT AVANT, PAS APRÈS. Le monde ferme la fenêtre en changeant de
// carte : après la trame, il n'y a plus rien à interroger. Et c'est l'ANCIEN
// dialogue qu'on veut, pas celui qui arrive — un script qui téléporte puis
// reprend la parole envoie sa page APRÈS le changement de carte, et prendre
// congé de celle-là fermerait une page neuve.
[[nodiscard]] std::optional<NpcFarewell> NpcFarewellDue(const sim::World& world);

// Prend congé, sans fenêtre et sans monde.
//
// 🔴 MÊME CHOIX DE PAQUET QUE « FERMER », et pour la même raison : un script
// arrêté sur un `select` attend un rang, pas une fermeture.
//
// ⚠ LE SERVEUR PEUT LE REFUSER, et c'est normal : `npc_checknear` rejette toute
// suite de script venue d'une autre carte que celle du PNJ. Le mot porte alors
// pour le seul cas où il peut porter — une téléportation SUR PLACE, à quelques
// cases, d'où le PNJ reste à portée. Ailleurs, le minuteur du serveur reprend
// la main.
[[nodiscard]] core::Result<void> SayNpcFarewell(Connection& link, const NpcFarewell& due);

}  // namespace ro::net
