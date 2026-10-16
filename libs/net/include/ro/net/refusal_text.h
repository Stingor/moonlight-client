#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace ro::net {

// POURQUOI LE SERVEUR A MIS FIN À LA SESSION — `SC_NOTIFY_BAN` (0x0081).
//
// ⚠ CE N'EST PAS UN BANNISSEMENT, malgré son nom. Les TROIS serveurs l'emploient
// pour toute fin de session qu'ils décident, et le motif le plus fréquent n'est
// ni une sanction ni une panne mais « quelqu'un vient de se connecter avec ce
// compte ». Le serveur appelle `set_eof` aussitôt après l'avoir envoyé : c'est
// le dernier mot qu'on entend de lui, et le seul qui dise pourquoi.
//
// 🔴 C'EST AUSSI L'AVIS DU RENVOI POUR INTÉGRITÉ, sous le code 3 : le serveur de
// carte coupe par là, au terme d'un délai, une session dont la poignée de main
// d'intégrité n'est pas arrivée. Son libellé
// d'origine ne dit que « délai dépassé », ce qui enverrait chercher une panne
// de réseau ; celui-ci nomme les deux causes.
//
// Les valeurs sont celles que `clif_authfail_fd` énumère au-dessus d'elle-même.
[[nodiscard]] std::string BanText(std::uint8_t code);

// POURQUOI LE SERVEUR DE CONNEXION A DIT NON — `AC_REFUSE_LOGIN` (0x083e).
//
// `unblock_time` est le second champ du même paquet. Il ne porte quelque chose que pour le code 6.
//
// 🔴 ET CE SERVEUR NE LE REMPLIT PAS : son `logclif_auth_failed` reçoit bien la
// date en paramètre, puis écrit la chaîne VIDE à sa place
// (src/login/loginclif.cpp:182 — le paramètre n'est lu nulle part). Le message
// ne promet donc une date que lorsqu'il en tient une.
//
// Les valeurs sont celles que `logclif_auth_failed` énumère ; leurs libellés
// d'origine sont dans `conf/msg_conf/login_msg.conf`.
[[nodiscard]] std::string LoginRefusalText(std::uint32_t code, std::string_view unblock_time);

// POURQUOI LE SERVEUR A REFUSÉ DE CRÉER — `HC_REFUSE_MAKECHAR` (0x006e).
//
// 🔴 QUATRE CODES POUR QUINZE CAUSES, et c'est ce qu'il faut savoir avant de
// lire la table. `chclif_createnewchar_refuse` ne traduit que quatre retours de
// `char_make_new_char`, et l'un d'eux — le 0xFF, « création refusée » — ramasse
// TOUT le reste : nom vide, nom trop court, caractères de contrôle, dièse en
// tête, lettres hors de la liste autorisée, emplacement déjà pris, plafond de
// personnages atteint, classe interdite, panne de base. Le serveur ne dit pas
// laquelle.
//
// ⚠ IL FAUT DONC ÉNUMÉRER, et c'est ce que fait le libellé : donner « création
// refusée » tout court laisse le joueur changer son nom au hasard sans savoir
// ce qu'il corrige.
//
// ⚠ ET LE 0x00 NE VEUT PAS DIRE QUE LE NOM EST PRIS PAR UN JOUEUR : le serveur
// le rend aussi pour les noms RÉSERVÉS — celui de ses messages internes, et
// celui du spectateur du décor —, qu'aucune recherche en base ne trouverait.
[[nodiscard]] std::string MakeCharRefusalText(std::uint8_t code);

// POURQUOI LA SUPPRESSION A ÉTÉ REFUSÉE — TROIS TABLES, ET NON UNE.
//
// 🔴 LES TROIS ÉTAPES ONT DES CODES QUI SE RESSEMBLENT ET NE VEULENT PAS DIRE
// LA MÊME CHOSE. À la réservation, 2 n'existe pas et 4 veut dire « en
// guilde » ; à l'annulation, 2 veut dire « introuvable » ; à la confirmation,
// 2 veut dire « en guilde OU en groupe ». Une seule fonction à qui l'on
// passerait le code laisserait l'appelant choisir la table — donc se tromper
// de table le jour où il recopie une ligne. Trois noms, trois portes.
//
// ⚠ ET ELLES NE PARTAGENT PAS LE TYPE : la réservation et l'annulation rendent
// un entier SIGNÉ de quatre octets, la confirmation un octet. C'est ce que les
// paquets portent.

// `HC_DELETE_CHAR3_RESERVED` (0x0828). Codes de `chclif_parse_char_delete2_req`.
[[nodiscard]] std::string DeleteReserveRefusalText(std::int32_t result);

// `HC_DELETE_CHAR3_CANCEL` (0x082c). Codes de
// `chclif_parse_char_delete2_cancel`.
[[nodiscard]] std::string DeleteCancelRefusalText(std::int32_t result);

// `HC_REFUSE_DELETECHAR` (0x0070). Codes de `chclif_parse_delchar`.
//
// 🔴 SON CODE ZÉRO COUVRE QUATRE CAUSES, et son libellé d'origine — « adresse
// email incorrecte » — est le plus trompeur des quatre. `char_delete` y tombe
// aussi pour une panne de base, pour un niveau trop haut (`char_del_level`, nul
// en production) et surtout pour `CHAR_DELETE_TIME` : la suppression n'a pas
// été réservée, ou l'heure n'est pas atteinte. Un joueur qui saisit son email
// JUSTE se voit alors répondre qu'il est faux.
//
// ⚠ L'ÉCRAN, LUI, SAIT LEQUEL : il connaît sa propre réservation et l'échéance
// que porte `DelRevDate`. Ce texte-ci énumère ; à l'interface de dire mieux
// quand elle le peut.
[[nodiscard]] std::string DeleteRefusalText(std::uint8_t code);

// `HC_ACK_CHANGE_CHARACTER_SLOT` (0x0b70), quand sa raison n'est pas zéro.
//
// ⚠ UN SEUL CODE D'ÉCHEC POUR SIX CAUSES : `chclif_parse_moveCharSlot` répond
// « 1 » pour une place hors du compte, une place occupée que le serveur
// n'échange pas, un compteur à zéro, la fonction coupée, ou une panne de base.
// Le texte énumère.
[[nodiscard]] std::string MoveRefusalText(std::uint16_t reason);

// `HC_ACK_CHANGE_CHARNAME` (0x08fd). Codes de `char_rename_char_sql`.
[[nodiscard]] std::string RenameRefusalText(std::uint32_t result);

}  // namespace ro::net
