#pragma once

#include <cstdint>
#include <string>

#include "ro/core/error.h"
#include "ro/net/connection.h"
#include "ro/net/handshake.h"
#include "ro/net/session.h"
#include "ro/sim/actor.h"
#include "ro/sim/packed_position.h"
#include "ro/sim/world.h"

namespace moonclient {

// Pourquoi la liaison s'est fermée, en clair.
//
// ⚠ Une fermeture PROPRE ne porte aucune erreur : le serveur a raccroché, et
// c'est tout ce qu'il y a à dire. Se contenter d'afficher le détail de l'erreur
// laissait alors une ligne vide — « liaison perdue : » — c'est-à-dire la
// question posée sans la réponse. L'état, lui, dit toujours quelque chose.
[[nodiscard]] std::string DescribeLinkLoss(ro::net::LinkStatus status, const ro::core::Error& why);

// Ce que la poignée de main a obtenu du serveur.
struct MapEntry {
  // Le nom de la carte, sans extension : de quoi nommer ses trois fichiers.
  std::string map_name;
  // Le nom du PERSONNAGE. Il ne vient d'aucun paquet du monde — le serveur
  // n'annonce jamais le joueur à lui-même — mais de la liste des personnages,
  // et le serveur l'exige en tête de chaque parole.
  std::string character_name;
  ro::sim::CellPosition spawn;
  std::uint32_t character_id = 0;
  // Le compte de la connexion : un changement de serveur de zone le redemande.
  ro::net::LoginPhase::Account account;
  // L'identifiant du joueur SUR LA CARTE. Ce n'est pas celui du personnage :
  // le serveur de zone donne à un joueur l'identifiant de son COMPTE
  // (sd->id = account_id, dans src/map/pc.cpp), et c'est celui-là que portent
  // tous les paquets qui parlent de lui — sa mort, ses coups, ses changements
  // d'apparence. Les confondre laisse un personnage que rien ne touche.
  std::uint32_t map_id = 0;
  // L'apparence du personnage et sa vitesse de marche, telles que la liste des
  // personnages les a données. Le serveur de zone n'annonce JAMAIS le joueur à
  // lui-même : sans cela il ne porterait rien, et se dessinerait en novice.
  ro::sim::Appearance look;
  std::uint16_t speed_ms_per_cell = 0;
  // 🔴 LES DEUX NIVEAUX VIENNENT D'ICI ET DE NULLE PART AILLEURS. Le serveur de
  // zone n'envoie `SP_BASELEVEL` qu'au MOMENT OÙ IL CHANGE (`pc_baselevelup`) :
  // il ne l'annonce jamais à l'entrée, contrairement aux caractéristiques et à
  // la vie. Un client qui les attendrait du monde afficherait « Nv 0 / 0 »
  // jusqu'au premier niveau gagné.
  std::int64_t base_level = 0;
  std::int64_t job_level = 0;

  // 🔴 ET LA BOURSE AUSSI, POUR EXACTEMENT LA MÊME RAISON. Le serveur de zone
  // n'annonce `SP_ZENY` QUE lorsqu'il change — `pc_payzeny`, `pc_getzeny`, le
  // courrier, l'échange — et jamais à l'entrée : son lot d'accueil
  // (`clif_parse_LoadEndAck`) envoie le poids et le poids maximum, et saute la
  // bourse. C'est le serveur de PERSONNAGES qui la donne, dans la liste, sous le
  // nom `money` (`char_mmo_char_tobuf` : `info.money = p.zeny`).
  //
  // ⚠ ET C'EST CE NOM QUI L'A CACHÉE : chercher « zeny » dans le serveur de
  // personnages ne rend rien. Le client natif la lit là, et c'est pourquoi il
  // l'affiche dès l'entrée.
  std::int64_t zeny = 0;
};

// L'APPARENCE D'UN PERSONNAGE, TELLE QUE LA LISTE LA DONNE.
//
// ⚠ PARTAGÉE AVEC LA SCÈNE DE SÉLECTION, qui assoit vingt-cinq personnages
// avant qu'aucun monde n'existe. Deux traductions de la même ligne de liste
// finiraient par diverger sur un champ que l'une aurait oublié — l'ordre des
// trois accessoires, ou le sexe « celui du compte » —, et le personnage
// changerait d'aspect en entrant en jeu.
//
// ⚠ `account_sex` EST CELUI DE LA CONNEXION : la liste marque 99 le personnage
// qui ne porte pas de sexe propre.
[[nodiscard]] ro::sim::Appearance AppearanceOf(const ro::proto::CharacterInfo& character,
                                               std::uint8_t account_sex);

// CE QUE LA LISTE ET L'ACCORD D'ENTRÉE DISENT DU PERSONNAGE CHOISI.
//
// ⚠ EXTRAIT POUR ÊTRE PARTAGÉ, et non par goût du découpage : l'écran de
// connexion joue les mêmes trois actes par un autre chemin — sans bloquer —, et
// deux compositions du même personnage finiraient par diverger sur un champ que
// l'une aurait oublié. Le sexe « du compte », l'ordre des trois accessoires et
// le repli de vitesse ne se retrouvent pas deux fois par hasard.
[[nodiscard]] MapEntry ComposeMapEntry(const ro::proto::CharacterInfo& chosen,
                                       const ro::net::LoginPhase::Account& account,
                                       std::string map_name, const ro::net::MapPhase::Spawn& spawn);

// FAIT NAÎTRE LE JOUEUR DANS UN MONDE NEUF.
//
// 🔴 IL NE NAÎT D'AUCUN PAQUET : le serveur annonce les AUTRES, jamais soi. Son
// nom, ses deux niveaux et sa bourse ne viennent que de la liste des
// personnages, et sa position que de l'accord d'entrée. Sans cette naissance le
// monde reste vide, et la caméra vise l'origine de la carte.
void SpawnPlayer(const MapEntry& entry, ro::sim::World& world);

}  // namespace moonclient
