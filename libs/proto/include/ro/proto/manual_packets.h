#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ro/core/error.h"

namespace ro::proto {

// Les paquets sans structure côté serveur, écrits et lus champ par champ.
//
// POURQUOI CEUX-LÀ SONT ÉCRITS À LA MAIN
// Le serveur les lit champ par champ, à coups de RFIFOL(fd, pos[i]), sans
// jamais décrire leur disposition par un type. Un générateur ne peut pas
// produire ce qui n'existe pas.
//
// Ce qui existe, en revanche, c'est la ligne de table qui les enregistre :
//
//   parseable_packet(0x086A, 19, clif_parse_WantToConnection, 2, 6, 10, 14, 18);
//
// Les décalages y sont écrits par le serveur lui-même, et parsed_packets.h les
// rend. Chaque disposition ci-dessous est donc RECOUPÉE avec eux avant d'être
// émise : écrite à la main, mais pas crue sur parole.
//
// ⚠ L'OPCODE NE SE DEVINE PAS. À cette version, le serveur accepte l'entrée en
// jeu sous quatre opcodes, et 0x0072 — celui que toute la documentation ancienne
// lui donne — porte en réalité l'usage d'une compétence. Les valeurs ci-dessous
// sont celles sur lesquelles le serveur ET le client s'accordent.

// L'entrée sur la carte : le premier paquet qu'un client envoie au serveur de
// zone, avec les jetons obtenus aux deux phases précédentes.
struct EnterMapRequest {
  static constexpr std::uint16_t kOpcode = 0x086A;
  static constexpr std::size_t kWireSize = 19;

  std::uint32_t account_id = 0;
  std::uint32_t character_id = 0;
  std::uint32_t login_id1 = 0;
  // L'horloge du client au moment de l'envoi, en millisecondes. Le serveur s'en
  // sert pour caler la sienne sur celle du client.
  std::uint32_t client_tick = 0;
  std::uint8_t sex = 0;
};

// « La carte est chargée, je suis prêt. »
//
// Sans lui, le serveur n'envoie jamais le monde : ni les unités, ni l'état du
// personnage. Il ne porte aucun champ.
struct MapLoadedNotice {
  static constexpr std::uint16_t kOpcode = 0x007D;
  static constexpr std::size_t kWireSize = 2;
};

// « Je suis mort : fais-moi réapparaître. »
//
// 🔴 CE PAQUET N'EST PAS UNE TÉLÉPORTATION. Le serveur ne le sert qu'à un
// personnage MORT (`clif_parse_Restart`, puis `pc_respawn`) ; un vivant qui veut
// rejoindre son point de sauvegarde passe par la commande `@load`, qui est autre
// chose. Confondre les deux donne un bouton muet et rien pour l'expliquer.
struct RestartRequest {
  static constexpr std::uint16_t kOpcode = 0x00B2;
  static constexpr std::size_t kWireSize = 3;

  // 0 = réapparaître au point de sauvegarde, 1 = revenir à la sélection de
  // personnage. Les deux valeurs sont celles du `switch` du serveur.
  static constexpr std::uint8_t kSavePoint = 0;
  static constexpr std::uint8_t kCharacterSelect = 1;

  std::uint8_t what = kSavePoint;
};

// « Je me relève sur place. »
//
// ⚠ IL NE PORTE AUCUN CHAMP, et le serveur décide seul s'il l'accorde : il exige
// un objet du groupe TOKEN_OF_SIEGFRIED, ou l'état de régénération
// (`pc_revive_item`). Refusé, il est SILENCIEUX — d'où le bouton qui ne paraît
// que lorsque l'une des deux conditions est visible de notre côté.
struct AutoReviveRequest {
  static constexpr std::uint16_t kOpcode = 0x0292;
  static constexpr std::size_t kWireSize = 2;
};

// « Mémorise ma position comme point de Warp Portal. »
//
// ⚠ IL NE PORTE AUCUN CHAMP : le serveur prend la carte et la case de sa propre
// session, et choisit lui-même la place (`pc_memo` avec un rang de -1 décale le
// plus ancien point). Il refuse un mort sans un mot ; tout autre refus revient
// par `ZcAckRememberWarppoint` ou `ZcNotifyMapinfo`.
struct RememberWarpPointRequest {
  static constexpr std::uint16_t kOpcode = 0x011D;
  static constexpr std::size_t kWireSize = 2;
};

// « Je veux marcher jusqu'à cette case. »
//
// ⚠ Le serveur accepte QUATRE opcodes pour cette demande à cette version, tous
// dirigés vers clif_parse_WalkToXY : c'est ainsi qu'il sert plusieurs
// générations de clients à la fois. Celui-ci vient du dernier bloc que
// 20250716 traverse, « #if PACKETVER >= 20130320 ». Trois d'entre eux portent
// la même disposition — cinq octets, la destination au décalage deux — et le
// choix entre eux ne se voit donc pas de notre côté du fil, et le serveur les
// accepte indifféremment.
//
// Le quatrième, 0x00a7, lit la destination au décalage six : le confondre avec
// les autres ferait marcher le personnage vers une case tirée du bourrage.
struct WalkRequest {
  static constexpr std::uint16_t kOpcode = 0x0881;
  static constexpr std::size_t kWireSize = 5;

  // Deux coordonnées et une direction, DÉJÀ empaquetées sur trois octets.
  //
  // Le paquet les transporte sans les lire : les découper est l'affaire de la
  // couche qui place le personnage, et proto ne connaît pas la grille.
  std::array<std::uint8_t, 3> packed_destination = {};
};

// « Je fais ceci à cet acteur » : frapper, frapper sans relâche, s'asseoir,
// se relever.
//
// Trois opcodes de sept octets acceptent ce paquet à cette version, plus un
// quatrième de vingt-trois — 0x0190, celui des vieux clients, tout de
// bourrage. Celui-ci vient du même bloc que la marche (0x0881), le dernier que
// 20250716 traverse, et la table de longueurs du client le connaît. Le
// serveur lit les trois pareil.
struct ActionRequest {
  static constexpr std::uint16_t kOpcode = 0x088E;
  static constexpr std::size_t kWireSize = 7;

  // Les actions que le serveur lit (clif_parse_ActionRequest_sub, dans
  // src/map/clif.cpp). Un coup isolé n'atteint sa cible que si elle est déjà
  // à portée ; la suite de coups la POURSUIT, et c'est le serveur qui marche.
  static constexpr std::uint8_t kAttackOnce = 0;
  static constexpr std::uint8_t kSitDown = 2;
  static constexpr std::uint8_t kStandUp = 3;
  static constexpr std::uint8_t kAttackRepeatedly = 7;

  std::uint32_t target_id = 0;
  std::uint8_t action = kAttackOnce;
};

// « Je ramasse cet objet. » Même bloc que la marche ; le client l'enregistre à
// six octets.
struct PickupRequest {
  static constexpr std::uint16_t kOpcode = 0x0933;
  static constexpr std::size_t kWireSize = 6;

  std::uint32_t item_id = 0;
};

// ── LES TROIS VERBES DU SAC ─────────────────────────────────────────────────
//
// 🔴 LE SERVEUR ÉCOUTE PLUSIEURS OPCODES POUR LE MÊME GESTE, AVEC DES
// DISPOSITIONS DIFFÉRENTES. À cette version, `clif_parse_UseItem` répond à
// 0x009F comme à 0x0439, et `clif_parse_DropItem` à quatre numéros. Choisir au
// hasard n'échoue pas : le serveur lit des octets aux mauvaises places et agit
// sur un autre objet.
//
// Ce qui tranche est la table de longueurs du CLIENT, recoupée avec les
// décalages que le serveur déclare :
//
//   0x0439  8 octets   champs en 2 et 4    <- la disposition classique
//   0x009F 20 octets   champs en 7 et 20   <- le second champ tombe HORS du
//                                             paquet : ce n'est pas celui-là
//   0x00AB  4 octets   champ en 2
//   0x0363  6 octets   champs en 2 et 4
//
// ⚠ L'INDICE EST CELUI DU CLIENT, tel que le fil l'a donné. Le serveur lui
// retranche son décalage à la lecture (`index - 2`) ; le défaire ici le
// retrancherait deux fois.

// « J'utilise cet objet. »
struct UseItemRequest {
  static constexpr std::uint16_t kOpcode = 0x0439;
  static constexpr std::size_t kWireSize = 8;

  std::uint16_t client_slot = 0;
  // ⚠ LE SERVEUR NE LE LIT PAS — il sait déjà à qui il parle. Le client
  // l'envoie quand même, et nous faisons pareil : un paquet qui s'écarte de ce
  // que le client d'origine émet se remarque.
  std::uint32_t account_id = 0;
};

// « Je porte cet objet. » ⚠ Celui-ci a une structure côté serveur
// (`PACKET_CZ_REQ_WEAR_EQUIP`) et vit donc dans les paquets engendrés, pas ici.

// « Je retire cet objet. »
struct UnequipItemRequest {
  static constexpr std::uint16_t kOpcode = 0x00AB;
  static constexpr std::size_t kWireSize = 4;

  std::uint16_t client_slot = 0;
};

// « Je jette ceci au sol. »
struct DropItemRequest {
  static constexpr std::uint16_t kOpcode = 0x0363;
  static constexpr std::size_t kWireSize = 6;

  std::uint16_t client_slot = 0;
  // Combien on en jette. Une pile se jette en plusieurs fois si l'on veut.
  std::uint16_t amount = 0;
};

// « Je monte cette caractéristique d'un cran. »
//
// 🔴 DEUX PAQUETS SE DISPUTENT CE GESTE, ET ILS NE MONTENT PAS LES MÊMES
// CARACTÉRISTIQUES. Le générateur a produit `CzAdvancedStatusChange` (0x0B24,
// six octets) parce que le serveur en DÉCLARE la structure — mais il la lit avec
// `clif_parse_traitstatus_up`, qui appelle `pc_traitstatusup` : ce sont les
// caractéristiques de MÉTIER des quatrièmes classes, `SP_POW` à `SP_CON`. Les six
// que la fiche montre — force, agilité, constitution, intelligence, dextérité,
// chance — passent par CELUI-CI, que le serveur lit avec `clif_parse_StatusUp`
// et confie à `pc_statusup`.
//
// Se tromper de paquet ne produirait aucune erreur visible : l'un comme l'autre
// est accepté, et le mauvais monterait autre chose — ou rien.
//
// ⚠ ET LA QUANTITÉ TIENT SUR UN SEUL OCTET, là où le numéro en prend deux :
// cinq octets en tout. L'écrire sur deux décalerait le paquet d'un cran et le
// serveur lirait le numéro de travers.
struct StatUpRequest {
  static constexpr std::uint16_t kOpcode = 0x00BB;
  static constexpr std::size_t kWireSize = 5;

  // Le numéro du serveur : `SP_STR` à `SP_LUK`, soit 13 à 18.
  std::uint16_t parameter = 0;
  // Combien de crans d'un coup. Le client d'origine en envoyait toujours un ;
  // depuis 2013 il envoie le vrai nombre, et le serveur le lit.
  std::uint8_t amount = 1;
};

// ── LES TROIS VERBES DU COFFRE ──────────────────────────────────────────────
//
// 🔴 LES TROIS ONT ÉTÉ RENUMÉROTÉS, et pas au même moment. Le mélange de 2013
// a déplacé les deux transferts — 0x00F3 et 0x00F5, les numéros que toute la
// documentation nomme encore — vers 0x08AC et 0x0874, mais a laissé la
// fermeture à 0x0193, où elle était depuis 2004. Prendre les trois anciens
// numéros, ou supposer que le mélange les a tous pris, se trompe deux fois.
//
// Deux sources indépendantes disent la même chose : le dernier bloc de
// `clif_packetdb.hpp` que PACKETVER traverse, et la table de longueurs du
// client, qui déclare 0x08AC et 0x0874 sur huit octets et 0x0193 sur deux.
//
// 🔴 ET LES ANCIENS NUMÉROS NE SONT PAS LIBRES : ils ont été repris par
// d'autres gestes, et le serveur les lit toujours. Se tromper ne fait donc pas
// rien — cela fait autre chose :
//
//   0x00F3  `clif_parse_GlobalMessage`  le personnage PARLE à voix haute
//   0x00F5  `clif_parse_TakeItem`       il ramasse un objet au sol
//   0x00F7  `clif_parse_MoveFromKafra`  il SORT du coffre au lieu d'y ranger
//
// Le dernier est le pire des trois : il garde un sens de coffre, mais c'est
// l'autre sens. Fermer le coffre par le numéro que la documentation nomme
// viderait une case au hasard.

// « Je range ceci dans le coffre. »
//
// ⚠ L'INDICE EST CELUI DU SAC. Le serveur lui retranche DEUX
// (`clif_parse_MoveToKafra`, `RFIFOW(...) - 2`) là où il n'en retranche qu'un à
// celui du coffre : le décalage est asymétrique, et prendre la case d'un
// contenant pour l'autre désigne l'objet d'à côté.
struct StorageStoreRequest {
  static constexpr std::uint16_t kOpcode = 0x08AC;
  static constexpr std::size_t kWireSize = 8;

  std::uint16_t client_slot = 0;
  // ⚠ QUATRE OCTETS, là où le jet n'en prend que deux : le serveur lit cette
  // quantité avec `RFIFOL`.
  std::int32_t amount = 0;
};

// « Je sors ceci du coffre. »
//
// ⚠ L'INDICE EST CELUI DU COFFRE, dont le serveur retranche UN
// (`clif_parse_MoveFromKafra`). Voir la remarque de `StorageStoreRequest`.
struct StorageTakeRequest {
  static constexpr std::uint16_t kOpcode = 0x0874;
  static constexpr std::size_t kWireSize = 8;

  std::uint16_t client_slot = 0;
  std::int32_t amount = 0;
};

// « Je referme le coffre. »
//
// 🔴 CE N'EST PAS UN GESTE D'AFFICHAGE : le coffre est une SESSION du serveur,
// et tant qu'elle dure il refuse les échanges entre le sac et la charrette
// (`sd->state.storage_flag`). Fermer la fenêtre sans le dire laisserait le
// personnage bloqué sur un coffre que plus rien ne montre.
struct CloseStorageRequest {
  static constexpr std::uint16_t kOpcode = 0x0193;
  static constexpr std::size_t kWireSize = 2;
};

// UN CHUCHOTEMENT REÇU.
//
// ⚠ CE N'EST PLUS 0x0097. À cette version c'est 0x09DE, et la disposition n'est
// pas la même : l'identifiant de l'auteur passe DEVANT son nom, et le drapeau de
// staff tient sur un octet au lieu de quatre. Porter l'ancien lirait le nom dans
// les octets de l'identifiant.
//
// ⚠ ET LE GÉNÉRATEUR LE REFUSE, à raison : le serveur le déclare trois fois
// derrière une chaîne de #if, et `isAdmin` y change de largeur ET DE SIGNE. Voir
// tools/packetgen/refus.csv, qui tient la liste de ces refus.
//
// L'auteur peut venir d'une autre carte, ou n'être personne — un message du
// système emprunte ce paquet. `sender_id` vaut alors zéro, et seul le nom reste.
struct WhisperNotice {
  static constexpr std::uint16_t kOpcode = 0x09DE;
  // À partir de là : le nom, le drapeau, puis le message jusqu'au bout.
  static constexpr std::size_t kHeaderSize = 4 + 4 + 24 + 1;
  // Ce qu'un nom de personnage occupe, toujours : NAME_LENGTH du serveur.
  static constexpr std::size_t kNameSize = 24;

  std::uint32_t sender_id = 0;
  std::string sender;
  // Vrai quand l'auteur est du staff. ⚠ LE CLIENT D'ORIGINE S'EN SERT MAL : dès
  // que ce drapeau est levé, il bascule la ligne en type « annonce » et elle
  // échappe au filtre des chuchotements. On range le genre nous-mêmes, donc ce
  // travers ne nous atteint pas.
  bool from_staff = false;
  std::string message;
};

// UNE PAROLE COLORÉE — ET C'EST LE SEUL PAQUET QUI PORTE UNE COULEUR.
//
// ⚠ DEUX CHOSES DIFFÉRENTES PASSENT PAR CET OPCODE, et c'est L'IDENTIFIANT
// qui les sépare, pas une devinette sur le texte :
//
//   zéro     un message de CANAL (`clif_channel_msg` écrit `WBUFL(buf,4) = 0`),
//            celui des alliés compris. Le nom du canal et celui de l'auteur
//            sont dans le texte, et nulle part ailleurs.
//   non nul  la parole de L'ACTEUR que cet identifiant désigne
//            (`clif_messagecolor` écrit `WBUFL(buf,4) = bl->id`) : un PNJ, un
//            monstre, ou le joueur lui-même quand le serveur lui répond.
//
// ⚠ LE PRENDRE POUR UN CANAL DANS LES DEUX CAS rangeait « le banque est
// fermée » dans l'onglet des canaux, et privait de bulle les PNJ qui parlent
// en couleur. Le client d'origine, lui, lit l'identifiant.
//
// Le serveur l'écrit à la main, sans structure : rien à engendrer.
struct ColoredNotice {
  static constexpr std::uint16_t kOpcode = 0x02C1;
  static constexpr std::size_t kHeaderSize = 4 + 4 + 4;

  // L'acteur qui parle, ou ZÉRO pour un canal. Voir l'en-tête.
  std::uint32_t speaker_id = 0;
  // La couleur demandée, en 0xRRGGBB.
  std::uint32_t color = 0;
  std::string message;
};

// « Suivant. » Le clic qui fait reprendre un script de PNJ.
//
// Le serveur n'en déclare aucune structure : il lit ses quatre octets à la main
// (clif_parse_NpcNextClicked), et le générateur n'a donc rien à engendrer.
struct NpcNextRequest {
  static constexpr std::uint16_t kOpcode = 0x00B9;
  static constexpr std::size_t kWireSize = 6;

  std::uint32_t npc_id = 0;
};

// Le choix fait dans un menu de PNJ.
//
// ⚠ LE RANG COMMENCE À UN, ET UN MAUVAIS RANG ÉJECTE LE JOUEUR. Zéro, ou un rang
// au-delà de la longueur du menu, n'est pas refusé poliment :
// `clif_parse_NpcSelectMenu` y voit une manipulation du protocole et appelle
// `clif_GM_kick`. La déconnexion qui s'ensuit ne ressemble à rien et ne dit
// rien. L'annulation, elle, est DEUX CENT CINQUANTE-CINQ.
struct NpcMenuChoice {
  static constexpr std::uint16_t kOpcode = 0x00B8;
  static constexpr std::size_t kWireSize = 7;

  std::uint32_t npc_id = 0;
  std::uint8_t choice = 0;
};

// UNE UNITÉ AU SOL apparaît : un piège, un mur, une zone.
//
// Le serveur l'écrit à la main (clif_getareachar_skillunit, src/map/clif.cpp),
// et la forme dépend du PACKETVER : quatre dispositions s'y succèdent, et
// c'est la dernière — 0x09ca — qui vaut à partir de 20130731.
//
// ⚠ Le paquet porte sa propre longueur en tête, à l'imitation d'un paquet de
// taille variable, alors que la table le déclare FIXE à vingt-trois octets. Les
// deux disent la même chose ; on lit la table, comme partout ailleurs.
//
// L'identifiant d'unité n'est ni une classe ni un identifiant d'effet : c'est
// une numérotation à part (e_skill_unit_id, src/map/skill.hpp), que le client
// traduit d'abord en identifiant de COMPÉTENCE avant d'en tirer quoi que ce
// soit à montrer.
struct SkillUnitNotice {
  static constexpr std::uint16_t kOpcode = 0x09CA;
  static constexpr std::size_t kWireSize = 23;

  std::uint32_t unit_actor_id = 0;
  std::uint32_t creator_id = 0;
  std::uint16_t cell_x = 0;
  std::uint16_t cell_y = 0;
  std::uint32_t unit_id = 0;
  std::uint8_t range = 0;
  // Faux pour un piège CACHÉ. Le serveur remplace alors l'identifiant d'unité
  // par celui de l'unité muette : il ne dit pas ce qu'il cache.
  std::uint8_t visible = 0;
  std::uint8_t skill_level = 0;
};

// « Voici mon horloge. » — le battement qui tient la session ouverte.
//
// ⚠ Ce paquet n'a rien de décoratif : le serveur COUPE toute session dont il
// n'a rien reçu depuis stall_time secondes, soixante par défaut
// (conf/packet_athena.conf ; src/common/socket.cpp, où le dépassement appelle
// set_eof). La coupure est propre, donc muette : la liaison se ferme sans la
// moindre erreur, et un client qui ne bat pas la mesure tombe au bout d'une
// minute d'inactivité sans savoir pourquoi.
//
// Le serveur y répond par sa propre horloge, dont on ne fait rien pour
// l'instant : la simulation date ses faits sur l'horloge de l'appelant.
//
// ⚠ PAS 0x0363, et l'erreur ne se voyait pas. Le serveur monte sa table en DEUX
// temps : sa table de base, puis le MÉLANGE qui la recouvre
// (packetdb_readdb inclut clif_packetdb.hpp puis clif_shuffle.hpp, et le second
// écrase le premier). À cette version, le mélange reprend 0x0363 et le dirige
// vers la CHUTE D'UN OBJET : le battement arrivait donc comme une demande de
// jeter l'objet dont le numéro était le bas de notre horloge, en autant
// d'exemplaires que son haut. Rien ne se voyait — le serveur ne répond pas à un
// battement, et la session tient sur n'importe quel octet reçu — sinon un objet
// perdu de loin en loin.
struct TickRequest {
  static constexpr std::uint16_t kOpcode = 0x0360;
  static constexpr std::size_t kWireSize = 6;

  std::uint32_t client_tick = 0;
};

// « Je lance cette compétence SUR CELUI-CI. »
//
// L'opcode vient du mélange, et c'est celui que le client d'origine émet. Le
// serveur l'accepte aussi sous
// 0x089b, que sa table de base lui donne ; les deux mènent au même
// gestionnaire, et l'on suit le client.
//
// ⚠ Le NIVEAU vient AVANT l'identifiant sur le fil. L'ordre paraît inversé et
// il ne l'est pas : clif_parse_UseSkillToId lit le niveau au premier décalage
// et l'identifiant au second.
//
// Une compétence qui vise une CASE ne passe pas par là : le serveur écarte
// celles-là d'entrée, sans un mot. Une compétence qui se lance sur soi, en
// revanche, y passe — et le serveur remplace la cible par le demandeur, quoi
// qu'on écrive.
struct SkillOnTargetRequest {
  static constexpr std::uint16_t kOpcode = 0x0438;
  static constexpr std::size_t kWireSize = 10;

  std::uint16_t skill_level = 0;
  std::uint16_t skill_id = 0;
  std::uint32_t target_id = 0;
};

// « Je lance cette compétence SUR CETTE CASE. » — une zone, un mur, un piège.
//
// Le client d'origine l'émet sous cet opcode, et sa propre table de longueurs
// le déclare à onze octets.
//
// Le ONZIÈME octet reste inconnu. Le client l'écrit — c'est le bas du troisième
// argument de son répartiteur de commandes — mais le serveur ne le lit PAS : sa
// ligne de lecture est commentée, et le commentaire qui la remplace dit qu'on
// ignore à quoi elle servait. On envoie donc zéro, et la taille est juste.
struct SkillOnGroundRequest {
  static constexpr std::uint16_t kOpcode = 0x0AF4;
  static constexpr std::size_t kWireSize = 11;

  std::uint16_t skill_level = 0;
  std::uint16_t skill_id = 0;
  std::uint16_t cell_x = 0;
  std::uint16_t cell_y = 0;
};

// « Je dis ceci à voix haute. »
//
// Le seul paquet sortant de longueur VARIABLE : sa tête porte l'opcode puis la
// longueur totale, et le texte suit.
//
// ⚠ Le serveur exige le NOM de qui parle en tête du texte, séparé du message
// par un espace, deux-points, espace. Il le compare au nom du personnage
// connecté et REJETTE tout le message s'il diffère (clif_process_message, dans
// src/map/clif.cpp) : ce n'est pas une décoration, c'est une condition.
struct ChatRequest {
  static constexpr std::uint16_t kOpcode = 0x00F3;
  // L'opcode et la longueur ; le texte suit, terminé par un zéro.
  static constexpr std::size_t kHeaderSize = 4;
  static constexpr std::string_view kNameSeparator = " : ";

  std::string speaker;
  std::string text;
};

// « Je parle à mon GROUPE. »
//
// ⚠ MÊME EXIGENCE QUE LE MESSAGE PUBLIC : le serveur passe par
// `clif_process_message`, qui réclame le nom du personnage en tête du texte et
// REJETTE tout le message s'il ne correspond pas. Ce n'est pas une décoration.
struct PartyChatRequest {
  static constexpr std::uint16_t kOpcode = 0x0108;
  static constexpr std::size_t kHeaderSize = 4;

  std::string speaker;
  std::string text;
};

// « Je parle à ma GUILDE. » Même forme, même exigence.
struct GuildChatRequest {
  static constexpr std::uint16_t kOpcode = 0x017E;
  static constexpr std::size_t kHeaderSize = 4;

  std::string speaker;
  std::string text;
  // Sur une carte de champ de bataille, la ligne part au relais du champ.
  bool battlefield = false;
};

// « Je parle à mon CLAN. » Même forme encore, et même exigence.
//
// ⚠ LE CLAN N'EST PAS LA GUILDE : on y entre par sa CLASSE, on n'en choisit
// pas les membres, et un personnage peut tenir les deux à la fois. D'où deux
// paquets là où un seul aurait suffi si c'était la même chose.
struct ClanChatRequest {
  static constexpr std::uint16_t kOpcode = 0x098D;
  static constexpr std::size_t kHeaderSize = 4;

  std::string speaker;
  std::string text;
};

// « Je chuchote à quelqu'un. »
//
// ⚠ CELUI-CI NE PORTE PAS LE NOM DE L'ÉMETTEUR, contrairement aux trois autres :
// il porte celui du DESTINATAIRE, dans un champ à lui, et le texte est nu. Le
// serveur sait qui parle par la connexion.
//
// ⚠ ET C'EST AUSSI PAR LÀ QUE PASSE UN CANAL. `clif_parse_WisMessage` regarde si
// le destinataire commence par un croisillon, et route alors vers le canal de ce
// nom — `#ally` pour les alliés. Il n'y a pas de paquet de canal à l'envoi : le
// `CZ_ALLY_CHAT` des clients récents existe bien, mais ce serveur-ci le fait
// lire par `clif_parse_dull`, c'est-à-dire qu'il le jette.
struct WhisperRequest {
  static constexpr std::uint16_t kOpcode = 0x0096;
  // L'opcode, la longueur, puis le destinataire sur vingt-quatre octets.
  static constexpr std::size_t kHeaderSize = 4 + 24;
  static constexpr std::size_t kNameSize = 24;
  // Le nom du canal des alliés, tel que la configuration du serveur le pose.
  static constexpr std::string_view kAllyChannel = "#ally";

  std::string recipient;
  std::string text;
};

// « Je montre cette émoticône. »
//
// ⚠ PAS 0x00BF. Le vieux paquet d'émoticône existe encore et le serveur
// l'écoute, mais son gestionnaire lit le numéro à un décalage que sa table ne
// déclare pas : elle vaut zéro, et il relit l'octet de poids faible de
// l'opcode. Le client moderne ne l'emploie plus. Il passe par le paquet des
// émoticônes de boutique, qui porte le LOT puis le numéro dans ce lot, et dont
// le lot zéro est le jeu de base.
//
// La structure du serveur nomme ses quatre octets « unknown » : c'est son
// gestionnaire qui les lit, deux entiers de seize bits aux décalages deux et
// quatre (clif_parse_cash_emotion_use, dans src/map/clif.cpp).
// MONTER UN RANG DE COMPÉTENCE (CZ_UPGRADE_SKILLLEVEL).
//
// ⚠ UN PAQUET PAR RANG, et ce n'est pas un choix d'écriture : le paquet ne porte
// PAS de niveau. Le serveur monte d'un cran et revalide chaque fois
// (`pc_skillup`) — demander cinq rangs se fait en cinq paquets, et un seul n'en
// monterait qu'un.
struct SkillUpRequest {
  static constexpr std::uint16_t kOpcode = 0x0112;
  static constexpr std::size_t kWireSize = 4;

  std::uint16_t skill_id = 0;
};

// « QUI EST-CE ? » — la demande d'un nom, et de tout ce que le serveur écrit
// sous lui.
//
// Le serveur ne pousse la carte d'un JOUEUR de lui-même qu'aux moments où elle
// change (`clif_name_area` : changement de métier, de titre, de guilde). Un
// joueur déjà en vue à notre arrivée n'en a donc jamais envoyé, et sa guilde
// resterait invisible. C'est le client qui la réclame, une fois par entité : il
// met l'identifiant en file dès que sa fiche n'est pas marquée valide.
//
// ⚠ L'OPCODE VIENT DU DERNIER BLOC QUI L'INSCRIT, ET CE N'EST PAS
// `clif_packetdb.hpp`. Le serveur inscrit `clif_parse_GetCharNameRequest` sous
// QUATRE numéros au fil des versions — 0x0368, 0x096A, 0x094A, puis 0x0898 —
// et `clif_shuffle.hpp`, inclus APRÈS, le remet sous 0x0368 pour tout
// PACKETVER au-dessus de 20180307. S'arrêter au premier fichier fait émettre
// 0x0898, que le serveur lit alors comme autre chose.
//
// 🔴 LE SERVEUR BORNE LA QUESTION : même carte, et dans le champ de vision. Une
// demande hors de portée ne répond RIEN — pas même un refus.
struct NameRequest {
  static constexpr std::uint16_t kOpcode = 0x0368;
  static constexpr std::size_t kWireSize = 6;

  std::uint32_t actor_id = 0;
};

// « COMMENT S'APPELLE CE PERSONNAGE ? » — sous son identifiant de PERSONNAGE,
// pas d'acteur. C'est ainsi qu'on nomme le forgeron d'un objet : le fil n'en
// porte que l'identifiant. Le serveur répond par `ZC 0x0AF7`, même pour un
// personnage hors ligne ou hors de vue.
//
// ⚠ LE MÊME PIÈGE QUE CI-DESSUS : `clif_parse_SolveCharName` a changé quatre
// fois de numéro, et c'est le dernier bloc de `clif_shuffle.hpp` que PACKETVER
// traverse qui le pose sous 0x0369.
struct CreatorNameRequest {
  static constexpr std::uint16_t kOpcode = 0x0369;
  static constexpr std::size_t kWireSize = 6;

  std::uint32_t character_id = 0;
};

// « UN BUG, ICI » — voir l'ADR 0052. Longueur VARIABLE :
//
//   [type:2][len:2][category:1][ctx_len:2][ctx:ctx_len][message: jusqu'à len]
//
// ⚠ ÉCRIT À LA MAIN parce que la queue porte deux textes bout à bout, que le
// générateur ne sait pas poser ; contrôlé comme les autres contre ce que le
// serveur déclare lire. Les deux textes sont DÉJÀ dans l'encodage du serveur :
// ce paquet ne convertit rien.
struct BugReportRequest {
  static constexpr std::uint16_t kOpcode = 0x0F13;
  static constexpr std::size_t kHeaderSize = 7;

  std::uint8_t category = 0;
  std::string context;
  std::string message;
};

// « Je tourne sur place. »
//
// ⚠ L'OPCODE N'EST PAS 0x009B, que toute la documentation lui donne : à cette
// version, 0x009B porte l'entrée en jeu. Le serveur inscrit
// `clif_parse_ChangeDir` sous six numéros au fil des versions, et un seul est
// long de DIX octets — c'est celui que la table de longueurs du client déclare
// aussi. Les cinq autres en font cinq, et les confondre ferait lire la
// direction dans le bourrage.
//
// ⚠ ET LES DEUX CHAMPS NE SE SUIVENT PAS. Le serveur les lit aux décalages
// QUATRE et NEUF, chacun sur un seul octet, alors que sa propre structure
// déclare la tête sur deux — `clif_parse_ChangeDir` passe par RFIFOB et non
// par le type. Quatre octets de bourrage séparent les deux, et les écrire à la
// suite ferait tourner le personnage n'importe où.
//
// 🔴 LE SERVEUR NE RÉPOND PAS À L'ENVOYEUR : `clif_changed_dir` diffuse en
// AREA_WOS, tout le monde autour SAUF soi. Son propre demi-tour doit donc être
// posé localement — sans quoi rien ne bougerait jamais chez celui qui tourne.
struct TurnRequest {
  static constexpr std::uint16_t kOpcode = 0x0085;
  static constexpr std::size_t kWireSize = 10;

  // Le REGARD, relatif au corps : voir ro/sim/movement.h.
  std::uint8_t head_direction = 0;
  // Le CORPS, dans la numérotation du serveur.
  std::uint8_t direction = 0;
};

struct EmotionRequest {
  static constexpr std::uint16_t kOpcode = 0x0BE9;
  static constexpr std::size_t kWireSize = 6;

  // Le lot du JEU DE BASE, le seul dont le sprite soit dans l'installation.
  static constexpr std::uint16_t kBasePack = 0;

  std::uint16_t pack_id = kBasePack;
  std::uint16_t emotion_id = 0;
};

// L'EFFET joué sur un acteur, que le serveur écrit à la main
// (clif_specialeffect, src/map/clif.cpp) : l'opcode, l'identifiant de l'acteur,
// puis le numéro d'effet, sur dix octets.
//
// C'est par là que passe la TAILLE d'un corps : le protocole n'a aucun champ
// pour elle, et le serveur envoie un effet — 420 et 421 pour rapetisser, 422 et
// 423 pour agrandir. ⚠ LES VARIANTES DOUBLES NE SONT PAS LES MÊMES QUE LES
// SIMPLES : la simple vient du changement de taille et le corps y va par une
// courbe ; la double est réémise à chaque apparition et pose la taille d'un
// coup. Le client les distingue, et la table des effets d'acteur aussi.
struct SpecialEffectNotice {
  static constexpr std::uint16_t kOpcode = 0x01F3;
  static constexpr std::size_t kWireSize = 10;

  std::uint32_t actor_id = 0;
  std::uint32_t effect_id = 0;
};

// L'EFFET QUI PORTE UN NOMBRE : le même effet qu'au-dessus, plus une valeur
// que le jeu fait paraître au-dessus de l'acteur — les points qu'un soin
// périodique rend, la magie qu'un objet restitue.
//
// ⚠ CE N'EST PAS ZcNotifyEffect3, malgré le nom que les deux partagent. La
// structure engendrée porte l'opcode 0x0B69 et une valeur de SOIXANTE-QUATRE
// bits : c'est la forme que le serveur déclare pour notre PACKETVER. Il ne
// l'émet jamais. clif_specialeffect_value écrit son paquet À LA MAIN, opcode
// 0x0284 en dur et quatorze octets, quelle que soit la version — la valeur y
// tient donc sur trente-deux bits, signés. Décoder la structure engendrée sur
// cette trame-là la refuserait, et la longueur déclarée par le serveur (14) le
// dit déjà.
//
// Le client sait lire les deux : il étend la valeur de trente-deux bits au
// signe avant de la faire monter.
struct ValuedEffectNotice {
  static constexpr std::uint16_t kOpcode = 0x0284;
  static constexpr std::size_t kWireSize = 14;

  std::uint32_t actor_id = 0;
  std::uint32_t effect_id = 0;
  // Ce que l'effet compte. Négatif quand le jeu retire.
  std::int32_t value = 0;
};

// LES PROTÉGÉS DE DEVOTION, écrits à la main par le serveur (clif_devotion) :
// le croisé — ou le mercenaire —, puis cinq protégés, zéro pour une case vide.
struct DevotionListNotice {
  static constexpr std::uint16_t kOpcode = 0x01CF;
  static constexpr std::size_t kWireSize = 28;

  std::uint32_t devoter_id = 0;
  std::array<std::uint32_t, 5> devotee_ids{};
  // La portée de la compétence, en cases. Le client ne la lit pas.
  std::int16_t range = 0;
};

// Lit une trame d'unité au sol. Faux si elle n'a ni le bon opcode ni la bonne
// taille.
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, SkillUnitNotice& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, WhisperNotice& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ColoredNotice& packet);

// Lit une trame d'effet. Faux si elle n'a ni le bon opcode ni la bonne taille.
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, SpecialEffectNotice& packet);

// Lit une trame d'effet à valeur. Même contrat.
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ValuedEffectNotice& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, DevotionListNotice& packet);

// Rend la trame prête à émettre, ou l'erreur qui dit pourquoi la disposition
// écrite ici ne s'accorde plus avec ce que le serveur déclare lire.
//
// Le contrôle est fait à CHAQUE encodage plutôt qu'une fois pour toutes : il
// coûte trois comparaisons, et une disposition fausse coûte une session.
[[nodiscard]] core::Result<std::vector<std::uint8_t>> Encode(const EnterMapRequest& request);
[[nodiscard]] core::Result<std::vector<std::uint8_t>> Encode(const MapLoadedNotice& notice);
[[nodiscard]] core::Result<std::vector<std::uint8_t>> Encode(const RestartRequest& request);
[[nodiscard]] core::Result<std::vector<std::uint8_t>> Encode(const AutoReviveRequest& request);
[[nodiscard]] core::Result<std::vector<std::uint8_t>> Encode(
    const RememberWarpPointRequest& request);
[[nodiscard]] core::Result<std::vector<std::uint8_t>> Encode(const WalkRequest& request);
[[nodiscard]] core::Result<std::vector<std::uint8_t>> Encode(const ActionRequest& request);
[[nodiscard]] core::Result<std::vector<std::uint8_t>> Encode(const PickupRequest& request);
[[nodiscard]] core::Result<std::vector<std::uint8_t>> Encode(const UseItemRequest& request);
[[nodiscard]] core::Result<std::vector<std::uint8_t>> Encode(const UnequipItemRequest& request);
[[nodiscard]] core::Result<std::vector<std::uint8_t>> Encode(const DropItemRequest& request);
[[nodiscard]] core::Result<std::vector<std::uint8_t>> Encode(const StatUpRequest& request);
[[nodiscard]] core::Result<std::vector<std::uint8_t>> Encode(const StorageStoreRequest& request);
[[nodiscard]] core::Result<std::vector<std::uint8_t>> Encode(const StorageTakeRequest& request);
[[nodiscard]] core::Result<std::vector<std::uint8_t>> Encode(const CloseStorageRequest& request);
[[nodiscard]] core::Result<std::vector<std::uint8_t>> Encode(const TickRequest& request);
[[nodiscard]] core::Result<std::vector<std::uint8_t>> Encode(const NpcNextRequest& request);
[[nodiscard]] core::Result<std::vector<std::uint8_t>> Encode(const NpcMenuChoice& choice);
[[nodiscard]] core::Result<std::vector<std::uint8_t>> Encode(const ChatRequest& request);
[[nodiscard]] core::Result<std::vector<std::uint8_t>> Encode(const PartyChatRequest& request);
[[nodiscard]] core::Result<std::vector<std::uint8_t>> Encode(const GuildChatRequest& request);
[[nodiscard]] core::Result<std::vector<std::uint8_t>> Encode(const ClanChatRequest& request);
[[nodiscard]] core::Result<std::vector<std::uint8_t>> Encode(const WhisperRequest& request);
[[nodiscard]] core::Result<std::vector<std::uint8_t>> Encode(const SkillUpRequest& request);
[[nodiscard]] core::Result<std::vector<std::uint8_t>> Encode(const EmotionRequest& request);
[[nodiscard]] core::Result<std::vector<std::uint8_t>> Encode(const NameRequest& request);
[[nodiscard]] core::Result<std::vector<std::uint8_t>> Encode(const CreatorNameRequest& request);
[[nodiscard]] core::Result<std::vector<std::uint8_t>> Encode(const BugReportRequest& request);
[[nodiscard]] core::Result<std::vector<std::uint8_t>> Encode(const TurnRequest& request);
[[nodiscard]] core::Result<std::vector<std::uint8_t>> Encode(const SkillOnTargetRequest& request);
[[nodiscard]] core::Result<std::vector<std::uint8_t>> Encode(const SkillOnGroundRequest& request);

// Vrai si le serveur déclare bien lire ce paquet à ces décalages.
//
// Exposé pour que les essais puissent le vérifier sans encoder, et pour que
// l'échec d'un encodage puisse être expliqué.
[[nodiscard]] bool MatchesServerLayout(std::uint16_t opcode, std::string_view handler,
                                       std::span<const std::uint16_t> field_offsets);

}  // namespace ro::proto
