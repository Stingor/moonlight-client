#pragma once

#include <cstdint>
#include <string_view>

#include "ro/core/error.h"
#include "ro/net/connection.h"

namespace ro::net {

// Les demandes d'ACTION du joueur : frapper, ramasser, s'asseoir.
//
// Comme pour la marche, LE SERVEUR EST AUTORITAIRE : ces fonctions ne font rien
// au monde, elles posent une demande. Le serveur répond par ce qu'il annonce à
// tout le monde — un coup, un ramassage, une posture, que world_update
// applique — ou par rien du tout quand il refuse. L'erreur rendue ne concerne
// que l'ÉMISSION : une disposition de paquet qui ne s'accorde plus avec ce que
// le serveur déclare lire.

// Frapper un acteur QUI EST DÉJÀ À PORTÉE.
//
// 🔴 CE PAQUET NE RAPPROCHE PERSONNE, ni avec `repeat` ni sans. Nous avions
// écrit le contraire, et cela a coûté des mois où cliquer sur un monstre
// lointain ne faisait rien : dans `unit_attack_timer_sub` (src/map/unit.cpp),
// la poursuite — `unit_walktobl` — est la branche du MONSTRE. Celle du JOUEUR,
// deux lignes plus haut, envoie `ZC_ATTACK_FAILURE_FOR_DISTANCE` et rend la
// main sans armer aucune horloge. Marcher jusqu'à la cible est l'affaire de qui
// demande — voir `world/attack_intent.h`, qui n'appelle ceci qu'à portée.
//
// Ce que `repeat` décide, c'est la SUITE : avec, le serveur enchaîne les coups
// jusqu'à ce que la cible tombe ou s'éloigne ; sans, il n'en porte qu'un.
[[nodiscard]] core::Result<void> RequestAttack(Connection& link, std::uint32_t target_id,
                                               bool repeat);

// Ramasser un objet au sol.
//
// Le serveur exige d'en être à deux cases (pc_takeitem, dans src/map/pc.cpp)
// et ne rapproche personne : marcher jusqu'à l'objet est l'affaire de qui
// demande — voir sim/pickup_intent.h.
[[nodiscard]] core::Result<void> RequestPickup(Connection& link, std::uint32_t item_id);

// S'asseoir, ou se relever. Le serveur l'annonce comme un coup, par
// ZC_NOTIFY_ACT, à tout le monde — y compris à qui l'a demandé.
[[nodiscard]] core::Result<void> RequestSit(Connection& link, bool sit);

// RÉAPPARAÎTRE AU POINT DE SAUVEGARDE, après un décès.
//
// 🔴 POUR UN PERSONNAGE MORT, ET LUI SEUL. Le serveur ne sert ce paquet qu'à un
// mort ; un vivant qui veut rejoindre son point de sauvegarde passe par la
// commande `@load`, qui n'est pas la même chose et ne suit pas le même chemin.
[[nodiscard]] core::Result<void> RequestRespawn(Connection& link);

// SE RELEVER SUR PLACE.
//
// ⚠ LE SERVEUR DÉCIDE SEUL, et son refus est SILENCIEUX : il exige un jeton de
// Siegfried ou l'état de régénération. C'est à l'appelant de ne proposer le
// geste que lorsque l'un des deux se voit.
[[nodiscard]] core::Result<void> RequestAutoRevive(Connection& link);

// Parler à voix haute.
//
// ⚠ `speaker` doit être le nom du personnage connecté : le serveur le compare
// au sien et rejette tout le message s'il diffère. C'est aussi par là que
// passent les commandes du serveur, celles qui commencent par arobase ou par
// dièse : il les reconnaît dans le texte.
[[nodiscard]] core::Result<void> RequestChat(Connection& link, std::string_view speaker,
                                             std::string_view text);

// Combien de temps sépare deux battements d'horloge.
//
// Le serveur coupe au bout de soixante secondes sans rien recevoir ; dix laisse
// six chances de se rattraper avant qu'il ne raccroche, et ne coûte que six
// octets.
inline constexpr std::uint32_t kTickIntervalMs = 10'000;

// Dire au serveur qu'on est toujours là. À envoyer toutes les kTickIntervalMs,
// même quand le joueur ne fait rien : voir TickRequest.
[[nodiscard]] core::Result<void> RequestTick(Connection& link, std::uint32_t client_tick);

// Montrer une émoticône du jeu de base. Le serveur la renvoie à tout le monde
// par le paquet des émoticônes de boutique, lot zéro : voir EmotionRequest.
[[nodiscard]] core::Result<void> RequestEmotion(Connection& link, std::uint16_t emotion);

}  // namespace ro::net
