#pragma once

#include <cstdint>

#include "ro/core/error.h"
#include "ro/net/connection.h"

namespace ro::net {

// ── CE QUE CE CLIENT SAIT AFFICHER ET LIRE — `CZ_MOONLIGHT_UI_CAPS 0x0F24` ──
//
// Le serveur sait qu'il parle à un client qui s'est annoncé (la poignée de
// main) ; il ne sait pas lesquelles de ses SURFACES existent chez lui, ni
// lesquels de ses paquets maison il sait lire. Un bit par surface ou par
// paquet, miroir exact de l'énumération que le serveur tient de son côté.
//
// ⚠ C'EST UN ÉTAT DE SESSION, jamais persisté : le serveur l'oublie à chaque
// entrée en zone, et il se réannonce donc après chaque poignée de main acceptée.
inline constexpr std::uint32_t kUiCapNpcDialog = 0x00000001;
inline constexpr std::uint32_t kUiCapChat = 0x00000002;
inline constexpr std::uint32_t kUiCapMvpTracker = 0x00000004;
inline constexpr std::uint32_t kUiCapCardAlbum = 0x00000008;
// 🔴 UNE CAPACITÉ DE LECTURE, pas une préférence : ce client sait lire l'octet
// de nature en queue d'entrée de l'album. Elle part TOUJOURS avec
// `kUiCapCardAlbum` — le joueur peut éteindre le liseré, le parseur, lui, sait
// toujours le lire.
inline constexpr std::uint32_t kUiCapCardAlbumDropper = 0x00000010;
// 🔴 UNE CAPACITÉ DE LECTURE : ce client sait peindre un Flag Graffiti
// (`ZC 0x0F36`). Sans ce bit, le serveur ne le lui montre pas, et il ne
// l'envoie jamais à qui ne l'a pas annoncé — un opcode inconnu viderait le
// tampon de réception du client d'origine.
inline constexpr std::uint32_t kUiCapFlagGraffiti = 0x00000020;
// 🔴 UNE CAPACITÉ DE LECTURE : ce client sait lire le maître d'un monstre
// invoqué (`ZC 0x0F37`). Même règle : le serveur ne l'envoie qu'à qui l'annonce.
inline constexpr std::uint32_t kUiCapUnitMaster = 0x00000040;
// 🔴 UNE CAPACITÉ DE LECTURE : ce client sait lire les règles du serveur
// (`ZC 0x0F38`) — rayon de vue, niveaux maximum de l'homoncule, délai après sa
// téléportation. Même règle : le serveur ne l'envoie qu'à qui l'annonce.
inline constexpr std::uint32_t kUiCapServerRules = 0x00000080;
// 🔴 UNE CAPACITÉ DE LECTURE : ce client sait lire les QUEUES du carnet MVP
// (`ZC 0x0F32`) — qui il est au bout d'un groupe, d'où vient une invitation, à
// quelle commande répond un résultat —, et reçoit la présence des membres
// poussée. Sans ce bit, le serveur écrit les trames d'avant, octet pour octet.
inline constexpr std::uint32_t kUiCapMvpTrackerExt = 0x00000100;
// 🔴 UNE CAPACITÉ DE LECTURE : ce client sait lire l'auteur d'une ligne de
// parole (`ZC 0x0F3A`). Même règle : le serveur ne l'envoie qu'à qui l'annonce.
inline constexpr std::uint32_t kUiCapChatAuthor = 0x00000200;
// 🔴 UNE CAPACITÉ DE LECTURE : ce client sait lire pourquoi une quête quitte le
// journal (`ZC 0x0F3B`). Même règle : le serveur ne l'envoie qu'à qui l'annonce.
inline constexpr std::uint32_t kUiCapQuestEnd = 0x00000400;
// 🔴 UNE CAPACITÉ DE LECTURE : ce client sait lire le relais Discord riche
// (`ZC 0x0F3C`). Même règle : le serveur ne l'envoie qu'à qui l'annonce.
inline constexpr std::uint32_t kUiCapDiscordRich = 0x00000800;
// 🔴 UNE CAPACITÉ DE LECTURE : ce client sait lire le maître d'un compagnon
// (`ZC 0x0F37`). Même règle : le serveur ne l'envoie qu'à qui l'annonce.
inline constexpr std::uint32_t kUiCapCompanionMaster = 0x00001000;
// 🔴 UNE CAPACITÉ DE LECTURE : ce client sait lire ce qu'apporte une pièce ou un
// ensemble qui entre ou sort de l'équipement (`ZC 0x0F3D`). Même règle : le
// serveur ne l'envoie qu'à qui l'annonce.
inline constexpr std::uint32_t kUiCapEquipAttributes = 0x00002000;

[[nodiscard]] core::Result<void> AnnounceUiCaps(Connection& link, std::uint32_t caps);

}  // namespace ro::net
