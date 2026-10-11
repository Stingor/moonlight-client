#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "ro/sim/combat_notice.h"
#include "ro/sim/devotion.h"
#include "ro/sim/packed_position.h"
#include "ro/sim/status_effects.h"

namespace ro::sim {

// Ce qu'un ACTEUR est, du point de vue du monde — et rien de ce que le monde en
// fait.
//
// 🔴 CE FICHIER EST NÉ D'UNE COUTURE, PAS D'UN PLAFOND. `world.h` portait deux
// choses : la DESCRIPTION d'une entité — son apparence, sa posture, ses états,
// les constantes que le serveur emploie pour les dire — et la CLASSE qui tient
// la collection. La première est lue par qui dessine ; la seconde ne l'est que
// par qui reçoit des trames. Les séparer rend la première incluable seule.
//
// ⚠ QUI INCLUAIT `world.h` N'A RIEN À CHANGER : il inclut ce fichier.

// Ce qu'un acteur est, du point de vue du monde.
//
// Le serveur le dit dans un octet de son paquet d'apparition. Ce qui en dépend
// ici est mince — surtout le fait qu'un objet au sol ne porte pas de nom — mais
// le renderer, lui, en tire l'ensemble des sprites à charger.
enum class ActorKind : std::uint8_t {
  kPlayer,
  kNonPlayer,
  kMonster,
  kItem,
  kSkillUnit,
  kUnknown,
};

// UN FAMILIER, parmi les non-joueurs.
//
// ⚠ `ActorKind` LES RANGE AVEC LES PNJ, et pour le dessin c'est juste : ils se
// peignent de la même façon. Mais un pet n'a pas de dialogue, et son maître a
// des ordres à lui donner — ce qui se décide sur une entité doit savoir la
// différence.
enum class Familiar : std::uint8_t {
  kNone,
  kPet,
  kHomunculus,
  kMercenary,
  kElemental,
};

// CE QU'UN MONSTRE EST DANS LA HIÉRARCHIE DES BOSS.
//
// 🔴 TROIS VALEURS, ET NON DEUX. Le champ `isBoss` du paquet d'apparition n'est
// pas un drapeau : c'est `e_mob_bosstype` du serveur (src/map/mob.hpp), et
// `clif_set_unit_idle` y écrit `get_bosstype()` — aucun, mini-boss
// (`CLASS_BOSS`), MVP (mode `MD_MVP`). Le lire comme un booléen perd la seule
// chose que le client en fasse : il pose une tête de mort ARGENT sur l'un et
// DORÉE sur l'autre.
enum class BossRank : std::uint8_t {
  kNone = 0,
  kMiniBoss = 1,
  kMvp = 2,
};

// Ce que le rang vaut, tel que le paquet le déclare. Une valeur que le serveur
// n'emploie pas se lit « aucun » : elle ne doit pas décider d'un insigne.
[[nodiscard]] constexpr BossRank BossRankOf(std::uint8_t sent) noexcept {
  switch (sent) {
    case 1:
      return BossRank::kMiniBoss;
    case 2:
      return BossRank::kMvp;
    default:
      return BossRank::kNone;
  }
}

// L'apparence, telle que le serveur la donne.
//
// Ce sont des IDENTIFIANTS, pas des chemins : la traduction vers un fichier de
// sprite passe par les tables Lua du client, que cette couche ne connaît pas.
struct Appearance {
  std::uint16_t job = 0;
  std::uint16_t head = 0;
  std::uint32_t weapon = 0;
  std::uint32_t shield = 0;
  std::uint16_t lower_headgear = 0;
  std::uint16_t middle_headgear = 0;
  std::uint16_t upper_headgear = 0;
  std::uint16_t robe = 0;
  // Le costume ; zéro est la tenue ordinaire.
  std::uint16_t body_style = 0;
  std::int16_t head_palette = 0;
  std::int16_t body_palette = 0;
  std::uint8_t sex = 0;
};

// ⚠ CE QUE LE MONDE RETIENT D'UN ÉTAT DÉPEND DE QUI LE PORTE. Sur un acteur
// quelconque, sa seule PRÉSENCE — c'est elle qui décide d'une aura ou d'une
// monture. Sur le JOUEUR, sa durée aussi : voir `ActiveStatus`, et
// `player_statuses()`.

// Monté sur une monture cash-shop : EFST_ALL_RIDING.
inline constexpr StatusEffectId kStatusAllRiding = 613;
// Le second costume d'été : EFST_DRESS_UP.
inline constexpr StatusEffectId kStatusDressUp = 856;
// Dans un Madogear : EFST_MADOGEAR_TYPE. Sa valeur dit lequel — la
// combinaison, ou la machine pour toute autre valeur (`e_mado_type`).
inline constexpr StatusEffectId kStatusMadogear = 1149;
inline constexpr std::int32_t kMadogearSuitValue = 2;

// UN EFFET DE CHAPEAU : ce qu'un costume, une monture ou une aura d'événement
// fait paraître autour de celui qui le porte.
//
// ⚠ CE N'EST PAS UN ÉTAT, et les deux numérotations n'ont rien à voir. Un état
// est un EFST de `src/map/status.hpp` ; celui-ci est un rang dans `HatEFID`,
// une table LUA de l'installation, et c'est elle — et non le code du client — qui
// dit ce qu'il montre. Les confondre ferait jouer l'effet d'un état homonyme.
// Voir `ro/render/hat_effect_table.h`.
using HatEffectId = std::uint16_t;

// Les états que le serveur annonce avec un acteur, par ZC_STATE_CHANGE et
// par ses paquets d'apparition : le corps (opt1), la santé (opt2) et les
// options (e_option), avec les valeurs de src/map/status.hpp. Le corps est
// une valeur, les deux autres des bits.
inline constexpr std::uint16_t kBodyStone = 1;
inline constexpr std::uint16_t kBodyFrozen = 2;
inline constexpr std::uint16_t kBodyStunned = 3;
inline constexpr std::uint16_t kBodyAsleep = 4;
inline constexpr std::uint16_t kBodyPetrifying = 6;
inline constexpr std::uint16_t kBodyBurning = 7;
inline constexpr std::uint16_t kBodyImprisoned = 8;
inline constexpr std::uint16_t kHealthCursed = 0x2;
inline constexpr std::uint16_t kHealthSilenced = 0x4;
inline constexpr std::uint32_t kOptionHide = 0x2;
inline constexpr std::uint32_t kOptionCloak = 0x4;
inline constexpr std::uint32_t kOptionInvisible = 0x40;
// LA TÊTE D'ORC : `@option 0 0 2048`, et l'état que pose l'Orc Head.
inline constexpr std::uint32_t kOptionOrcishHead = 0x800;

// CE QU'UN ACTEUR AFFICHE AU-DESSUS DE LUI, et qui n'est pas son nom : une
// échoppe qu'il tient, ou un salon qu'il a ouvert.
//
// ⚠ DEUX PAQUETS, DEUX ESPACES D'IDENTIFIANTS, ET ILS SE RESSEMBLENT. L'enseigne
// d'une échoppe nomme son VENDEUR, et se retire sous ce même identifiant
// (ZC_STORE_ENTRY puis ZC_DISAPPEAR_ENTRY). Un salon, lui, porte un identifiant
// À LUI, distinct de celui de son propriétaire, et c'est par celui-là qu'il se
// ferme (ZC_DESTROY_ROOM). Les confondre retirerait l'enseigne d'un autre.
//
// L'ÉCHOPPE D'ACHAT suit la règle de l'échoppe de vente : elle nomme son
// ACHETEUR, et se retire sous ce même identifiant (ZC_BUYING_STORE_ENTRY puis
// ZC_DISAPPEAR_BUYING_STORE_ENTRY).
enum class SignboardKind : std::uint8_t { kVendingShop, kChatRoom, kBuyingStore };

// L'état d'un salon, tel que le serveur le numérote : e_status_chatroom, dans
// src/map/clif.hpp.
//
// ⚠ ZÉRO EST « PRIVÉ », ET UN « PUBLIC ». L'ordre paraît inversé et ne l'est
// pas : c'est celui du serveur, et le deviner à l'endroit ferait peindre les
// salons fermés comme ouverts.
inline constexpr std::uint8_t kRoomPrivate = 0;
inline constexpr std::uint8_t kRoomPublic = 1;
inline constexpr std::uint8_t kRoomArena = 2;
inline constexpr std::uint8_t kRoomPkZone = 3;

struct Signboard {
  SignboardKind kind = SignboardKind::kVendingShop;
  std::string title;
  // L'identifiant DU SALON, jamais celui de son propriétaire. Zéro pour une
  // échoppe, qui n'en a pas.
  std::uint32_t room_id = 0;
  // Combien de monde s'y trouve, et combien il en tient. Les deux valent zéro
  // pour une échoppe : le serveur ne dit pas combien d'objets elle vend.
  std::uint16_t users = 0;
  std::uint16_t limit = 0;
  std::uint8_t status = kRoomPrivate;
};

// CE QUE LE SERVEUR ÉCRIT SOUS UN ACTEUR, ET QUI N'EST PAS SON NOM.
//
// 🔴 CES TROIS CHAÎNES SONT DES PLACES, PAS DES SENS, et c'est le SERVEUR qui
// l'impose. `ZC_ACK_REQNAMEALL` les nomme groupe, guilde et rang — mais rAthena
// y écrit tout autre chose dès qu'il s'agit d'un MONSTRE : sous
// `show_mob_info`, « Lv. 12 | HP: 63% » part dans le champ du groupe, la RACE
// dans celui de la guilde et l'ÉLÉMENT dans celui du rang. Son propre
// commentaire le dit : « Even thought mobhp ain't a name, we send it as one so
// the client can parse it. » Les nommer d'après le joueur ferait chercher un
// groupe là où il y a une barre de vie.
//
// Le client natif ne les interprète pas davantage : il compose deux lignes, et
// ces noms-là sont les siens.
//
//     [titre] nom (aside)
//     bannière [insigne]
struct NameCard {
  // Ce qui s'écrit ENTRE PARENTHÈSES à la suite du nom.
  std::string aside;
  // La seconde ligne, et ce qui s'y écrit ENTRE CROCHETS.
  std::string banner;
  std::string badge;
  // Le TITRE, quand le serveur l'envoie en toutes lettres — ce qu'il ne fait
  // que pour un PNJ, une unité nommée par `setunittitle`. Pour un joueur, il
  // n'envoie qu'un numéro, et c'est une table de données du client qui le
  // traduit : voir `title_id`.
  std::string title;
  // Le numéro du titre équipé, de la table `title_tbl` de l'installation. Zéro
  // quand le joueur n'en porte aucun — la table commence à mille.
  std::int32_t title_id = 0;
  // LE NOM TEL QUE L'ÉTIQUETTE L'ÉCRIT, quand il n'est pas celui du personnage
  // — « [AFK] Nom » pour un joueur absent. Vide sinon.
  //
  // 🔴 CE N'EST PAS UN NOM, et il ne remplace pas `Actor::name` : le serveur
  // cherche un joueur par son nom EXACT (invitation, ami, guilde, murmure), et
  // ce libellé-ci n'appartient à personne.
  std::string shown_name;
};

// Ce qu'un acteur fait de son corps : debout, assis, ou mort — à terre, là
// où il est tombé, jusqu'à ce que le serveur le relève ou le retire.
enum class Posture : std::uint8_t { kStanding, kSitting, kDead };

// QUI A DONNÉ LE REGARD qu'un acteur montre hors de sa marche : le serveur, qui
// annonce une direction ; la marche, dont le dernier pas le fixe ; ou l'acteur
// lui-même, tourné vers ce qu'il frappe ou vise. Les trois donnent la même
// direction de boussole, mais le client n'en tient pas le même ANGLE — et le
// nombre qui jaillit d'un coup dérive selon cet angle.
enum class FacingOrigin : std::uint8_t { kServer, kWalk, kAim };

// La vitesse de marche d'un personnage neuf, en millisecondes par case :
// DEFAULT_WALK_SPEED, dans src/common/mmo.hpp. Elle tient lieu de vitesse tant
// que le serveur n'a pas dit la sienne — à zéro, un acteur se téléporterait.
inline constexpr std::uint16_t kDefaultWalkSpeedMsPerCell = 150;

// Seize sous-cases par case ; celle-ci en est le centre. C'est là que se
// tient quiconque n'a pas dit autrement (voir aussi kSubCellCenter, dans
// movement.h, pour l'interpolation).
inline constexpr std::uint8_t kCellCenterSubCell = 8;

// Un acteur du monde : joueur, monstre, marchand, objet au sol.
// La classe qui désigne un PORTAIL. Un portail est un acteur comme un autre —
// il apparaît, il porte un nom, on peut le viser — et seule sa classe le
// distingue d'un PNJ.
inline constexpr std::uint16_t kPortalJob = 45;

// Ce que vaut une vie qu'on ne connaît pas. Zéro dirait « mort », et une barre
// vide s'afficherait au-dessus de tous les monstres intacts.
inline constexpr std::int32_t kUnknownHealth = -1;

// Une émoticône : le LOT dont elle vient, et son numéro dans ce lot.
//
// Le lot zéro est le jeu de base, celui dont le sprite est dans toute
// installation ; les autres sont des lots de boutique, avec leur propre sprite.
struct Emotion {
  std::uint16_t pack = 0;
  std::uint16_t id = 0;

  [[nodiscard]] friend bool operator==(const Emotion&, const Emotion&) = default;
};

// Le lot du jeu de base.
inline constexpr std::uint16_t kBaseEmotionPack = 0;

// L'ÉMOTICÔNE QU'UNE COMPÉTENCE ANNONCÉE FAIT JOUER À SON LANCEUR, quand son
// lanceur et sa cible sont là : Wink Charm donne le baiser (/chup), comme si le
// serveur l'avait annoncé — il ne l'annonce pas.
inline constexpr std::uint16_t kWinkCharmSkill = 1011;
inline constexpr std::uint16_t kEmotionChup = 30;

// UN EFFET JOUÉ SUR UN ACTEUR : son numéro, et quand le serveur l'a dit.
//
// Le monde ne sait pas ce qu'un numéro montre — c'est une table du client qui
// le dit — et ne garde que le fait.
struct PlayedEffect {
  std::uint32_t id = 0;
  // Dans l'horloge de l'APPELANT.
  std::uint32_t since_ms = 0;
  // Ce que l'effet COMPTE, quand le serveur le dit — les points qu'un soin
  // périodique rend, la magie qu'un objet restitue. Vide pour les effets
  // ordinaires, qui sont l'immense majorité : seul le paquet 0x0284 porte une
  // valeur, et le client ne la fait paraître que pour dix numéros. Voir
  // EffectNumberColor.
  std::optional<std::int32_t> value;
  // L'ÉTAT QUI L'A POSÉ, quand c'est un état et non le serveur qui le nomme.
  // Un même numéro ne montre pas la même chose selon l'état qui le pose : la
  // traînée de Energy Coat est bleue, celle d'Overthrust rose.
  std::optional<StatusEffectId> posed_by_state;
  // UN EFFET D'OFFICE, que l'installation donne à la créature et que rien
  // n'annonce : le plafond de la liste ne le chasse jamais. Voir
  // `World::RecordLastingEffect`.
  bool lasting = false;
  // LA COULEUR QUE LE CLIENT DONNE LUI-MÊME à une traînée, quand ce n'est ni un
  // état ni le serveur : le bond de Body Relocation la pose blanche.
  std::optional<std::array<float, 3>> told_color;
  // L'INSTANT OÙ L'EFFET S'ÉTEINT DE LUI-MÊME, quand le client lui donne une
  // durée : sans elle, seul le serveur le retire. Voir `World::ExpireEffects`.
  std::optional<std::uint32_t> ends_ms;
  // NÉ PAR LA CLASSE QUE LE GESTIONNAIRE D'EFFETS TIENT POUR SON NUMÉRO, et non
  // par son fichier ou ses nœuds : un même numéro peut venir des deux chemins.
  bool born_of_class = false;

  [[nodiscard]] friend bool operator==(const PlayedEffect&, const PlayedEffect&) = default;
};

// Combien d'effets un acteur garde à la fois : un GARDE-FOU.
//
// Un effet quitte la liste quand le serveur ou un autre effet le retire, quand
// son échéance tombe, ou quand il a FINI — c'est `app`, qui connaît les tables
// du client, qui le dit (`EffectFinishedAt`). Ce qui reste au-delà, ce sont
// les effets dont la trace se relit dans la liste après leur instance — l'angle
// qu'un effet a donné au corps lui reste — et les effets en boucle qu'aucun
// retrait ne vient chercher. Au-delà du plafond, le plus ancien s'en va — hors
// les effets d'office, que le client garde tant que l'acteur vit.
//
// ⚠ CE PLAFOND EST À NOUS, pas au client, dont la liste n'en a pas : il borne
// une liste qui, sans lui, grandirait tant que l'acteur vit.
inline constexpr std::size_t kEffectsKeptPerActor = 16;

// CE QUE LE MONDE RETIENT D'UN OBJET AU SOL, et qui ne vaut que pour lui.
//
// 🔴 DEUX PAQUETS, ET LEUR DIFFÉRENCE EST TOUT CE QUI SÉPARE UN BUTIN D'UN
// DÉCOR. `ZC_ITEM_FALL_ENTRY5 0x0ADD` annonce un objet qui VIENT DE TOMBER ;
// `ZC_ITEM_ENTRY 0x009D` un objet DÉJÀ POSÉ, que l'on découvre en entrant dans
// la zone. Le client arme sa chute pour le premier seulement : les confondre
// ferait pleuvoir la carte entière à chaque arrivée.
struct GroundItem {
  // L'instant où le serveur l'a annoncé, dans l'horloge de l'APPELANT.
  //
  // ⚠ UN SEUL INSTANT POUR DEUX MOUVEMENTS, comme chez le client, qui n'en
  // garde qu'un : c'est l'origine de la chute ET celle du
  // scintillement. Un objet déjà posé depuis une heure scintille donc à partir
  // du moment où on le VOIT, et non de celui où il est tombé — le serveur ne
  // dit pas le second.
  std::uint32_t since_ms = 0;
  // Vrai quand l'objet vient de tomber, et lui seul chute.
  bool falling = false;
  // CE QUE LE SERVEUR DEMANDE DE MONTRER AVEC LA CHUTE : le drapeau
  // `showdropeffect` et le `dropeffectmode` de `ZC_ITEM_FALL_ENTRY5 0x0ADD`.
  //
  // ⚠ DEUX CHAMPS ET NON UN, parce que le client en fait DEUX choses. Le
  // drapeau seul déclenche l'effet propre à l'OBJET — le client le cherche dans
  // la fiche de l'objet, donc dans une table Lua de l'installation —, et le
  // mode déclenche EN PLUS un pilier de lumière. Les
  // fondre en une seule valeur perdrait le cas « drapeau levé, mode nul », où
  // le client montre encore quelque chose.
  //
  // ⚠ LE MODE N'EST PAS LA COULEUR, et le nom du serveur ment : voir
  // `ro::render::ItemDropPillarEffectId`.
  bool show_drop_effect = false;
  std::uint16_t drop_effect_mode = 0;
  // DE QUOI COMPOSER SON NOM, et c'est tout ce que le fil en donne.
  //
  // 🔴 LE SERVEUR NE NOMME JAMAIS UN OBJET AU SOL. `clif_name` ne traite pas
  // `BL_ITEM` : demander son nom ne rend rien, et le client le COMPOSE depuis
  // la table d'objets de l'installation. C'est pourquoi un objet au sol n'a
  // pas de `name` comme les autres acteurs — le sien se fabrique à
  // l'affichage, depuis `item_id`.
  //
  // ⚠ LE TYPE NE VIENT QUE DU PAQUET DE CHUTE. `ZC_ITEM_ENTRY 0x009D` ne le
  // porte pas ; il vaut alors zéro, ce qui range l'objet parmi ceux qui ne se
  // décorent pas — sans conséquence, le fil ne donnant ni cartes ni
  // raffinement.
  bool identified = true;
  std::int32_t type = 0;
  // COMBIEN LA PILE EN PORTE. Les deux paquets le donnent.
  //
  // 🔴 LE CLIENT NE LE MONTRE JAMAIS, et ce n'est pas un oubli de notre part :
  // il le retient et ne l'affiche nulle part — son nameplate ne porte qu'un
  // rectangle de désignation. Le montrer est donc un AJOUT assumé.
  std::uint16_t amount = 1;
  // L'IDENTIFIANT DE L'OBJET, sur les 32 bits que le fil lui donne.
  //
  // 🔴 PAS DANS `look.job`, qui n'a que 16 bits : un objet d'identifiant
  // supérieur à 65 535 y prenait le sprite et le nom d'un autre.
  std::uint32_t item_id = 0;
};

struct Actor {
  std::uint32_t id = 0;
  ActorKind kind = ActorKind::kUnknown;
  // Aucun pour tout ce qui n'est pas un non-joueur.
  Familiar familiar = Familiar::kNone;
  CellPosition position;
  // Vers où la TÊTE regarde, le corps restant où il est : zéro tout droit, un
  // et deux de part et d'autre. Le serveur l'annonce avec chaque unité et le
  // redit par ZC_CHANGE_DIRECTION ; c'est ce que /doridori change.
  std::uint8_t head_direction = 0;
  // Vrai si un demi-tour a été annoncé DEPUIS le départ du déplacement en
  // cours. Remis à faux par chaque nouvelle marche.
  //
  // 🔴 SANS LUI, TOURNER SUR PLACE NE SE VOIT PAS. Le déplacement annoncé
  // SUBSISTE après l'arrivée — c'est lui qui garde la direction du dernier pas
  // —, si bien que la queue du chemin dictait le regard de quiconque avait
  // marché une fois. La direction posée par ZC_CHANGE_DIRECTION ne se voyait
  // donc que sur la TÊTE, qui ne vient d'aucun chemin : un personnage qui
  // tournait sur place ne bougeait que les yeux. Ce drapeau départage les
  // deux — le pas en cours dit le regard pendant la marche, le demi-tour
  // reprend la main une fois la marche finie.
  //
  // ⚠ UN DRAPEAU PLUTÔT QU'UNE DATE : `movement_started_ms` vaut zéro tant
  // qu'aucune marche n'a eu lieu, et un instant nul est un instant valable
  // dans l'horloge de l'appelant. Deux zéros ne se départagent pas.
  bool turned_since_walk_began = false;
  // D'où vient le regard montré une fois la marche finie ou tant qu'il n'y en a
  // pas eu. Pendant une marche, c'est le pas en cours qui le donne, quoi que
  // dise ce champ.
  FacingOrigin facing_origin = FacingOrigin::kServer;
  // L'ÉCART EN CASES vers ce que l'acteur a visé — x vers l'est, y vers le
  // nord —, quand `facing_origin` vaut `kAim`. La direction de boussole le
  // résume en huit ; l'écart garde l'angle exact que le client en tire.
  //
  // ⚠ FRACTIONNAIRE : on vise d'où l'on se tient vers où l'autre se trouve à
  // l'instant du coup, au milieu d'un pas s'il le faut, et non de case à case.
  float aimed_dx_cells = 0.0F;
  float aimed_dy_cells = 0.0F;
  // Le déplacement en cours, s'il y en a un. Le serveur n'en donne que les deux
  // bouts ; le chemin est l'affaire de qui dessine.
  std::optional<CellMovement> movement;
  // Millisecondes par case. Zéro tant que le serveur ne l'a pas dit — et un
  // acteur à zéro se TÉLÉPORTE, faute de durée à interpoler.
  std::uint16_t speed_ms_per_cell = 0;
  // L'instant où le déplacement a commencé, dans l'horloge de L'APPELANT.
  //
  // Le monde ne lit aucune horloge : il porte celle qu'on lui donne. C'est ce
  // qui permet à un rejeu de fournir les instants d'une capture et d'obtenir
  // exactement les mêmes positions qu'en direct.
  std::uint32_t movement_started_ms = 0;
  // Où dans sa case l'acteur se tient, en seizièmes : le centre, sauf pour
  // un objet au sol, que le serveur pose où il est tombé.
  std::uint8_t sub_x = kCellCenterSubCell;
  std::uint8_t sub_y = kCellCenterSubCell;
  std::string name;
  // CE QUE LE SERVEUR A DIT DE PLUS SOUS CE NOM, s'il l'a dit.
  //
  // ⚠ VIDE N'EST PAS ABSENT, et c'est ce que l'option porte : un acteur dont on
  // n'a pas encore la carte se demande, un acteur dont la carte est vide — un
  // joueur sans guilde — ne se redemande pas. Le client natif tient la même
  // distinction, par un octet « valide » dans son dictionnaire de noms.
  std::optional<NameCard> card;
  Appearance look;
  // Les états en cours, tels que le serveur les a posés. Une réapparition
  // repart de rien : le serveur redit alors ce qui tient encore.
  std::set<StatusEffectId> status_effects;
  // LA VALEUR QUE PORTE UN ÉTAT, pour les rares qui en ont une à MONTRER.
  //
  // 🔴 SIX EFFETS SE CALCULENT SUR ELLE, et non sur l'état. Rolling Cutter
  // montre un anneau par tour — `valeur + 1299`, de un à dix — et la
  // malédiction du Devil assombrit par paliers. Sans cette valeur, ces
  // états-là restent muets, et rien ne le dit.
  //
  // ⚠ ELLE N'ARRIVE PAS AVEC TOUS LES PAQUETS : `ZC_MSG_STATE_CHANGE` (0x0983)
  // et sa forme à durées (0x043F) la portent, celui de FIN (0x0196) non — et il
  // n'en a pas besoin, l'état partant avec sa valeur. Un état posé par un
  // paquet sans `val1` n'a donc pas d'entrée ici, et c'est une absence et non
  // un zéro : `StateValueEffectFor` ne montre rien sans valeur.
  std::map<StatusEffectId, std::int32_t> status_values;
  // Les effets de CHAPEAU qu'il porte. Même règle : une réapparition repart de
  // rien, et le serveur redit la liste entière à l'entrée en vue.
  std::set<HatEffectId> hat_effects;
  // Le dernier coup porté, et le dernier reçu.
  std::optional<Strike> attack;
  std::optional<Strike> hit;
  // L'incantation en cours, s'il y en a une. Voir Cast : elle reste posée
  // après son échéance, et c'est à qui la montre de ne plus la montrer.
  std::optional<Cast> cast;
  // La dernière compétence sans dégâts reçue.
  std::optional<SupportHit> support;
  // La dernière compétence que cet acteur a lancée SUR UNE CASE.
  std::optional<GroundSkillUse> ground_skill;
  // La dernière compétence sans dégâts que cet acteur a employée, sur une
  // case ou sur quelqu'un. Voir `SkillUse`.
  std::optional<SkillUse> skill_use;
  // L'instant du dernier ramassage, dans l'horloge de l'APPELANT.
  std::optional<std::uint32_t> pickup_since_ms;
  // La dernière émoticône montrée, et l'instant où elle est apparue dans
  // l'horloge de l'APPELANT. Elle s'éteint d'elle-même au bout de son
  // animation ; rien ne l'efface, et le serveur ne dit jamais qu'elle a fini.
  std::optional<Emotion> emotion;
  std::uint32_t emotion_since_ms = 0;
  // L'enseigne qu'il porte, s'il en tient une.
  std::optional<Signboard> signboard;
  // CE QU'IL EST EN TANT QU'OBJET AU SOL. Vide pour tout le reste : un
  // personnage ne tombe pas du ciel et ne scintille pas.
  std::optional<GroundItem> ground_item;
  // LES EFFETS que le serveur a joués sur cet acteur, du plus ancien au plus
  // récent.
  //
  // ⚠ UNE LISTE, ET NON LE DERNIER. Un acteur en porte plusieurs à la fois :
  // un corps rouge tient pendant qu'un coup l'atteint, et le client lui-même en
  // empile autant qu'il en reçoit. N'en garder qu'un ferait perdre le premier
  // dès que le second arrive, et rendrait 0x0B0D incapable de retirer autre
  // chose que le dernier.
  std::vector<PlayedEffect> effects;
  // LA PORTÉE que le serveur annonce d'un sort en cours d'incantation, s'il en
  // annonce une. Voir SkillScale : elle vit sur son LANCEUR, pas sur la case.
  std::optional<SkillScale> skill_scale;
  // Ce que l'acteur vient de DIRE, et l'instant où il l'a dit dans l'horloge de
  // l'APPELANT. Vide tant qu'il n'a rien dit ; c'est à qui dessine de décider
  // combien de temps la bulle tient.
  std::string chat_text;
  std::uint32_t chat_since_ms = 0;
  // LA COMPÉTENCE QU'IL VIENT D'ANNONCER, et l'instant de l'annonce dans
  // l'horloge de l'APPELANT. Zéro tant qu'il n'a rien annoncé.
  //
  // 🔴 L'IDENTIFIANT ET NON LE TEXTE : le libellé affiché vit dans les tables
  // Lua du client, que cette couche ne lit pas — c'est `app` qui croise les
  // deux. Poser le texte ici mettrait le nom d'une compétence dans une couche
  // qui ne sait pas nommer.
  //
  // ⚠ LA BANNIÈRE EST UNE BULLE DE PAROLE chez le client : il l'envoie à
  // l'acteur par le message 7, celui d'une phrase de chat. Voir
  // `ro/sim/skill_banner.h`.
  std::uint16_t announced_skill_id = 0;
  std::uint32_t announced_skill_since_ms = 0;
  // Qui la compétence annoncée vise — zéro quand le paquet ne le dit pas. Les
  // compétences de couple nomment leur partenaire dans la bannière.
  std::uint32_t announced_target_id = 0;
  // LE DÉLAI DE COMBO que le serveur a annoncé en dernier, et l'instant de
  // l'annonce : un enchaînement tient sa pose jusqu'à ce que le coup suivant
  // soit permis, et une annonce neuve repousse cette échéance.
  std::uint32_t combo_delay_ms = 0;
  std::uint32_t combo_delay_since_ms = 0;
  Posture posture = Posture::kStanding;
  // L'instant du dernier changement de posture, dans l'horloge de
  // l'APPELANT : c'est là que l'animation de la posture commence.
  std::uint32_t posture_since_ms = 0;
  // L'INSTANT OÙ LE SERVEUR L'A FAIT APPARAÎTRE (`clif_spawn`), pour une
  // créature seulement : le client la fait alors monter de la transparence. Vide
  // pour qui entre simplement en vue, et pour un joueur, qui reçoit un effet.
  std::optional<std::uint32_t> appeared_ms;
  // Le corps — pétrifié, gelé, étourdi, endormi… — et l'instant de son
  // dernier changement ; la santé et les options, des bits.
  std::uint16_t body_state = 0;
  std::uint32_t body_state_since_ms = 0;
  std::uint16_t health_state = 0;
  std::uint32_t option = 0;
  // LE MODE PK de cet acteur : le karma de son porteur, que le serveur écrit
  // dans les trois paquets d'unité et dans le changement d'état — un joueur
  // n'en prend qu'en tuant un autre joueur. Qui le porte revient de TOUT geste
  // en GARDE, là où les autres n'y reviennent que du combat.
  bool pk_mode = false;
  // 🔴 LE FRONT MONTANT DES OPTIONS, QUE LE MOT SEUL NE DIT PAS. Le natif ne
  // compare pas deux images : à la réception, il tranche sur les DEUX MOTS DU
  // PAQUET reçu. Un bit qui tombe puis remonte entre deux
  // images ne laisse aucune trace dans le mot, et comparer le mot d'une image à
  // celui de la précédente manque ce changement-là.
  //
  // ⚠ AUCUN SORT DE CE SERVEUR N'Y TOMBE AUJOURD'HUI : relancer un sort qui
  // tient garde son bit levé, et le natif ne rejoue rien non plus. Le front
  // montant se lit ici parce que c'est là que le natif décide, pas pour
  // rattraper un symptôme.
  //
  // `option_risen` porte les bits qui se sont allumés au dernier changement, et
  // le compteur dit qu'il y en a eu un. Qui les lit retient le compteur.
  //
  // ⚠ DEUX CHANGEMENTS DANS LA MÊME IMAGE NE LAISSENT QUE LE DERNIER : le
  // compteur avance de deux, et les bits sont ceux du second.
  std::uint32_t option_risen = 0;
  std::uint32_t option_rise_count = 0;
  // ET LE FRONT DESCENDANT, sur le même modèle : les bits qui se sont éteints
  // au dernier changement, et le compte de ces changements. La sortie de
  // Hiding fait sa bouffée de fumée sur lui.
  std::uint32_t option_fallen = 0;
  std::uint32_t option_fall_count = 0;
  // LE COMPTE DES CHANGEMENTS D'ÉTAT REÇUS en direct, qu'un bit ait bougé ou
  // non. Un acteur réduit au silence rejoue son bruit à CHAQUE changement
  // d'état qui le trouve encore muet, pas seulement quand le silence tombe.
  std::uint32_t state_change_count = 0;
  // DEPUIS QUAND LE SILENCE LE TIENT, les points de suspension au-dessus de
  // la tête — voir `ro/render/silence_dots.h`. Rien pour qui peut parler.
  std::optional<std::uint32_t> silenced_since_ms;
  // La distance parcourue AVANT le déplacement en cours, en cases.
  //
  // Le jeu anime la marche sur la distance, et son compteur ne repart JAMAIS de
  // zéro : il court sur toute la vie de l'acteur. Le serveur, lui, réannonce un
  // déplacement à chaque changement de cap — et sans ce report, le cycle
  // repartirait de sa première image à chaque fois. Le personnage marcherait
  // d'un seul pied.
  float travelled_before_cells = 0.0F;
  // La vie, quand le serveur la donne. Il ne la donne que d'un MONSTRE, sous un
  // réglage qui n'est pas toujours mis, et seulement une fois le monstre
  // entamé : moins un veut dire « on ne sait pas », et non « mort ».
  //
  // 🔴 TROIS SOURCES, ET NON UNE. `ZC_HP_INFO 0x0977` pour un monstre qu'on a
  // frappé, `ZC_NOTIFY_HP_TO_GROUPM 0x080E` pour un compagnon de groupe, et LE
  // PAQUET D'APPARITION LUI-MÊME — qui la porte depuis PACKETVER 20120221
  // (`clif_set_unit_idle` écrit `HP`/`maxHP`, ou moins un quand il se tait).
  // Sans cette troisième, un monstre déjà entamé qui entre dans le champ de
  // vision restait sans jauge jusqu'au coup suivant.
  std::int32_t health = kUnknownHealth;
  std::int32_t max_health = kUnknownHealth;

  // UN BOSS OU UN MVP, tel que le paquet d'apparition le déclare.
  //
  // ⚠ CE N'EST PAS LA CLASSE DE MONSTRE DU DOSSIER DE CIBLE. Celle-ci arrive
  // avec l'unité, pour TOUTE unité en vue, et vaut donc avant même qu'on ait
  // désigné quoi que ce soit ; celle-là est plus fine (normal, boss, garde) mais
  // ne concerne que ce qu'on vise. Les deux disent oui sur un MVP.
  BossRank boss = BossRank::kNone;

  // SON RANG AU CLASSEMENT PVP DE LA CARTE, quand le serveur l'a dit.
  //
  // 🔴 CELUI DES AUTRES, ET C'EST TOUT L'INTÉRÊT. `ZC_NOTIFY_RANKING 0x019a`
  // arrive en ZONE : on reçoit le rang de chaque voisin autant que le sien. Le
  // compteur du coin, lui, n'écoute que le nôtre — mais l'anneau qui marque les
  // dix premiers se pose sur QUICONQUE y entre, et il n'a pas d'autre source.
  //
  // ⚠ ZÉRO VEUT DIRE « PAS DE RANG », et non « premier » : le serveur compte à
  // partir de un. C'est aussi ce qu'il envoie en éteignant la zone.
  //
  // ⚠ ET IL NE SURVIT PAS À UNE RÉAPPARITION. Le serveur réannonce une unité à
  // chaque mise en marche, et son paquet ne porte pas le rang : voir
  // `World::Spawn`, qui le fait traverser comme la vie et l'enseigne.
  std::int32_t pvp_rank = 0;

  // SON NIVEAU, quand le serveur le donne. Zéro veut dire « on ne sait pas ».
  //
  // 🔴 LE PAQUET D'APPARITION LE PORTE POUR TOUT LE MONDE, et nous le jetions :
  // `clif_setlevel` écrit `status_get_lv` de qui apparaît, et ne se tait que
  // pour un PNJ ou un familier, qui n'ont pas de niveau. Sans lui, la couleur
  // d'écart de niveau — celle qui peint en rouge ce qui nous dépasse — ne peut
  // pas se calculer, et c'est la surcharge la plus visible du client.
  std::int16_t level = 0;

  // SA GUILDE, telle que le paquet d'apparition la déclare. Zéro : aucune.
  //
  // 🔴 C'EST ELLE QUI DIT SI UNE INVITATION OU UNE ALLIANCE A UN SENS, et elle
  // arrive avec l'unité, avant toute carte de nom. La bannière de la carte ne
  // porte qu'un NOM, que deux guildes peuvent partager.
  std::uint32_t guild_id = 0;

  // LE PAQUET D'APPARITION L'ANNONCE AVEC UN SPRITE DE JOUEUR (type d'objet 0).
  //
  // ⚠ CE N'EST PAS « UN JOUEUR ». Le serveur annonce ainsi tout ce qui porte un
  // corps de joueur — un monstre, un PNJ ou un familier qui en empruntent un —,
  // et il annonce autrement un joueur déguisé en autre chose. `kind` range ce
  // dernier avec les joueurs ; la flèche de sélection, elle, lit ce champ-ci.
  bool player_sprite = false;

  // LA SECONDE VALEUR d'un état posé, par état, quand le paquet la porte — la
  // même règle que `status_values`, dont elle suit la vie.
  //
  // ⚠ UN ÉTAT S'EN SERT POUR DÉSIGNER UN AUTRE ACTEUR : son identifiant y est
  // écrit, et c'est sur lui que l'effet naît.
  std::map<StatusEffectId, std::int32_t> status_second_values;

  // L'INSTANT OÙ UN ÉTAT A ÉTÉ POSÉ, dans l'horloge de l'appelant, quand
  // l'appelant l'a dit. Un état qui change la pose s'anime depuis cet instant :
  // faire le mort joue la chute, puis tient l'acteur couché.
  std::map<StatusEffectId, std::uint32_t> status_since_ms;

  // L'ÉLÉMENT DE SES SPHÈRES, tel que le dernier compte non nul l'a annoncé :
  // un pour l'eau, deux pour la terre, trois pour le feu, quatre pour le vent,
  // zéro tant qu'aucune n'a paru. Des méthodes nommées choisissent leur effet
  // par lui.
  //
  // ⚠ UN COMPTE NUL NE L'EFFACE PAS, et une annonce faite pendant que l'acteur
  // se cache ne le change pas : le client ne l'écrit qu'en montrant les
  // sphères.
  std::int16_t spirit_charm_element = 0;

  // LES EFFETS QU'ON LUI A RETIRÉS, les plus récents en dernier, et leur compte
  // depuis sa naissance. Retirer coupe l'instance qui joue, et pas seulement
  // son inscription : celui qui dessine lit ce journal pour savoir quoi couper.
  //
  // ⚠ UN RETRAIT S'INSCRIT MÊME QUAND L'EFFET N'ÉTAIT PLUS DANS `effects` : la
  // liste en garde seize, et l'instance du dix-septième joue peut-être encore.
  std::vector<std::uint32_t> removed_effects;
  std::uint32_t removed_effects_total = 0;
  // LA BARRE posée hors incantation — compétence annoncée, barre de
  // progression d'un script : voir `SkillGauge`. Vide quand il n'y en a pas.
  std::optional<SkillGauge> gauge;
  // LA POSE D'ARRIVÉE qu'un coup a armée, puis l'instant où l'arrivée l'a
  // levée — voir `kArrivalPoseSkill`.
  bool arrival_pose_armed = false;
  std::optional<std::uint32_t> arrival_pose_since_ms;
  // LE NOMBRE DE REGARDS REÇUS depuis son entrée, et ce nombre à sa dernière
  // compétence de case. Un regard reçu depuis rend à l'attente l'image de la
  // tête qu'une compétence de case lui avait imposée — voir
  // `GroundSkillKeepsCasterMotion`.
  std::uint32_t turns_received = 0;
  std::uint32_t turns_received_at_ground_skill = 0;
  // LE DERNIER ARRÊT QUE LE SERVEUR A IMPOSÉ, et s'il l'a fait glisser — voir
  // `NoteStop`. Un changement de motion ; glissé, il quitte la marche.
  std::optional<std::uint32_t> stopped_ms;
  bool stop_slid = false;
  // LE DERNIER RÉVEIL — voir `NoteSleep`. Un changement de motion, qui quitte le
  // sommeil.
  std::optional<std::uint32_t> awoke_ms;
  // LE TYPE D'OBJET que le paquet d'apparition annonce, tel quel. `kind` et
  // `familiar` en sont le résumé ; trois valeurs n'y ont pas de place et
  // décident pourtant de la motion d'une créature : le PNJ au sprite de
  // monstre, et les deux familles de monstres dont l'emploi joue un rang.
  std::uint8_t object_type = 0;
  // DEPUIS QUAND IL DORT, bulles au-dessus de la tête — voir `NoteSleep`. Rien
  // pour qui est éveillé.
  std::optional<std::uint32_t> asleep_since_ms;
  // DEPUIS QUAND LA MALÉDICTION LE TIENT, sa marque au-dessus des pieds — voir
  // `ro/render/ailment_sprites.h`. Rien pour qui n'est pas maudit.
  std::optional<std::uint32_t> cursed_since_ms;
  // DEPUIS QUAND BLADE STOP LE TIENT, et de quel côté : le moine qui a saisi la
  // lame, ou celui dont la lame est saisie. Rien hors de la prise. Voir
  // `World::SetBladeStop`.
  std::optional<std::uint32_t> blade_stop_since_ms;
  bool blade_stop_caster = false;
  // QUAND LE MOINE A RELÂCHÉ LA LAME : il garde sa pose jusqu'à sa motion
  // suivante, que cet instant permet de reconnaître. Rien pour qui ne l'a pas
  // relâchée, ou la tient encore.
  std::optional<std::uint32_t> blade_stop_released_ms;
  // LES PROTÉGÉS QU'IL ANNONCE par DEVOTION, et combien de fois il l'a fait.
  // Voir `World::TellDevotion`.
  DevotionList devotion;
  // LA VITESSE IMPOSÉE DU DÉPLACEMENT EN COURS, en millisecondes par case : le
  // bond de Body Relocation glisse à quinze. Zéro pour une marche, qui suit
  // `speed_ms_per_cell`. Une marche neuve l'efface.
  std::uint16_t glide_ms_per_cell = 0;
  // L'IDENTIFIANT DE SON MAÎTRE : pour un monstre invoqué, le joueur, l'homoncule
  // ou le monstre qui l'a appelé ; pour un compagnon — pet, homoncule,
  // mercenaire, élémentaire —, le joueur à qui il appartient. Le serveur
  // Moonlight le dit par son propre paquet, après l'apparition. Rien tant qu'il
  // ne l'a pas dit, zéro s'il a dit « sans maître » : une annonce d'unité ne le
  // porte jamais, et `World::Spawn` garde donc celui qu'on savait. Voir
  // `TellSummonMaster` et `SummonMasterOf`.
  std::optional<std::uint32_t> summon_master_id;
};

// LA VITESSE QUI RÈGLE LE DÉPLACEMENT EN COURS : celle du glissement s'il y en
// a un, celle de la marche sinon.
[[nodiscard]] inline std::uint16_t MovingSpeedOf(const Actor& actor) noexcept {
  return actor.glide_ms_per_cell != 0 ? actor.glide_ms_per_cell : actor.speed_ms_per_cell;
}

// Une UNITÉ AU SOL : un piège, un mur, une zone.
//
// Ce n'est pas un acteur. Elle ne marche pas, ne frappe pas, ne meurt pas ;
// elle occupe une case et disparaît. Lui donner un acteur obligerait tout ce
// qui parcourt les acteurs à la reconnaître pour l'écarter.
//
// L'identifiant d'unité est une numérotation À PART — ni classe, ni identifiant
// d'effet (e_skill_unit_id, dans src/map/skill.hpp du serveur). Le monde le
// garde tel quel : le traduire est l'affaire de qui dessine.
struct SkillUnit {
  std::uint32_t id = 0;
  // Qui l'a posée. Zéro quand le serveur ne le dit pas.
  std::uint32_t creator_id = 0;
  CellPosition cell;
  std::uint32_t unit_id = 0;
  // La portée déclarée, en cases. Elle sert au jeu, pas au dessin.
  std::uint8_t range = 0;
  std::uint8_t skill_level = 0;
  // L'instant de la pose, dans l'horloge de l'APPELANT.
  std::uint32_t placed_ms = 0;
  // Le modèle de l'unité se montre-t-il ? Le serveur le dit à la pose, et
  // l'unité que l'on a posée soi-même se montre toujours. Cachée, elle porte
  // l'unité muette.
  bool visible = true;
  // L'instant où l'unité a pris son apparence : sa pose, ou le changement
  // d'apparence que le serveur annonce quand un piège se déclenche. Le modèle
  // se rebâtit là, et l'horloge de la seconde famille en part.
  std::uint32_t look_since_ms = 0;
  // L'instant où le modèle a reçu l'ordre de jouer son animation UNE fois
  // jusqu'à sa dernière image : un coup porté par l'unité, ou son changement
  // d'apparence. Vide tant qu'il reste figé sur son image de pose.
  std::optional<std::uint32_t> sprung_ms;
  // CE QUE L'UNITÉ DIT, et depuis quand : le message d'une Talkie Box qui se
  // déclenche. Il flotte au-dessus d'elle comme une parole au-dessus d'une tête.
  std::string said;
  std::uint32_t said_since_ms = 0;
  // L'APPARENCE DE SA POSE, que les changements suivants ne touchent pas : un
  // piège qui se déclenche prend celle du piège usé, et c'est celle qu'il avait
  // en paraissant qui décide de l'effet de son déclenchement. Zéro quand
  // l'apparition ne l'a pas dite.
  std::uint32_t placed_unit_id = 0;
  // LE MESSAGE TRACÉ AU SOL d'un Graffiti, tel que le serveur l'envoie. Vide
  // pour toute autre unité.
  std::string scribbled;
  // L'EMBLÈME PEINT AU SOL d'un Flag Graffiti : la guilde du lanceur et la
  // version de son emblème à la pose. Zéro pour toute autre unité.
  std::int32_t emblem_guild_id = 0;
  std::int32_t emblem_version = 0;
};

// Le Flag Graffiti du serveur Moonlight : une unité qu'aucun client d'origine ne
// connaît, annoncée par son propre paquet.
inline constexpr std::uint32_t kFlagGraffitiUnit = 20860;

// L'unité MUETTE : le serveur y ramène tout ce qu'il ne veut pas montrer — un
// piège caché, une unité qu'un réglage rend invisible. Elle a un identifiant et
// une case, et rien à dessiner.
inline constexpr std::uint32_t kSilentSkillUnit = 0x86;

}  // namespace ro::sim
