#pragma once

#include <cstdint>
#include <string_view>

#include "ro/core/error.h"
#include "ro/net/connection.h"

namespace ro::net {

// ── CE QUE LE STAFF DEMANDE SUR UNE ENTITÉ DÉSIGNÉE ─────────────────────────
//
// Trois ordres, tous revalidés par le serveur : le bouton qu'on voit n'est pas
// une garde. Aucun compte rendu n'est attendu ici — le serveur écrit lui-même,
// dans la conversation, ce qu'il a fait ou refusé.

// `CZ 0x0F25`, réservé aux clients ayant donné leur poignée de main (ADR 0040).
// Niveau de groupe 99 et plus.
//
// 🔴 LE PNJ SE DÉSIGNE PAR SON GID, PAS PAR SON NOM : le nom de la plaque n'est
// pas le nom unique du serveur dès qu'il y a un duplicate ou un `#suffixe`, et
// une commande @ par le nom raterait précisément ceux-là. Recharger n'est
// d'ailleurs exprimable que par lui : ce qui se recharge est le FICHIER du
// script, dont seul le serveur connaît le chemin.
//
// ⚠ MIROIR EXACT DE L'ÉNUMÉRATION DU SERVEUR : les valeurs ne se réordonnent
// pas.
enum class NpcAdminAction : std::uint8_t {
  kReloadFile = 0,
  kUnload = 1,
  kMoveToMe = 2,
};

// `CZ 0x0F2B`, même réserve. Niveau de groupe 80 et plus. Le serveur résout le
// nom depuis l'AID puis rejoue la commande @ correspondante sous les droits du
// groupe de l'émetteur.
//
// ⚠ MIROIR EXACT DE L'ÉNUMÉRATION DU SERVEUR, comme la précédente.
enum class PlayerAdminAction : std::uint8_t {
  kComeHere = 0,
  kSitStand = 1,
  // Le paramètre porte le delta, signé.
  kEventPoints = 2,
  kMute = 3,
  kUnmute = 4,
  // Le serveur lit l'état de la cible et choisit `@jail` ou `@unjail`.
  kJailToggle = 5,
  kNuke = 6,
  // Bannit le COMPTE.
  kBlock = 7,
};

[[nodiscard]] core::Result<void> RequestNpcAdmin(Connection& link, std::uint32_t npc_id,
                                                 NpcAdminAction action);

// ⚠ `param` PART À CHAQUE ACTION : le serveur déclare une taille FIXE, et seul
// le don de points d'event le lit.
[[nodiscard]] core::Result<void> RequestPlayerAdmin(Connection& link, std::uint32_t account_id,
                                                    PlayerAdminAction action,
                                                    std::int32_t param = 0);

// `CZ 0x00CC` : expulser un joueur. Le serveur rejoue `@kick`.
[[nodiscard]] core::Result<void> RequestKick(Connection& link, std::uint32_t account_id);

// ── LES ANNONCES DU STAFF ────────────────────────────────────────────────────
//
// `CZ 0x0099` (`/b`) à tout le serveur, `CZ 0x019C` (`/lb`) à la carte seule.
// Le serveur les rejoue en `@kami` et `@lkami`, sous les droits du groupe de
// l'émetteur : un groupe qui n'a pas la commande est refusé SANS UN MOT.
//
// ⚠ LE TEXTE PART TEL QUEL, en UTF-8 converti dans la page de codes du jeu : le
// serveur n'y ajoute pas de nom, c'est à l'appelant de le mettre.
//
// ⚠ AVEC SON OCTET NUL FINAL. Le serveur copie la diffusion locale sur la
// longueur de la trame moins l'en-tête, et `safestrncpy` réserve le dernier
// octet au nul : sans lui, la dernière lettre tomberait.
[[nodiscard]] core::Result<void> RequestBroadcast(Connection& link, std::string_view text);
[[nodiscard]] core::Result<void> RequestLocalBroadcast(Connection& link, std::string_view text);

}  // namespace ro::net
