#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "ro/net/connection.h"
#include "ro/net/world_update.h"
#include "ro/sim/world_fwd.h"

namespace ro::net {

// LES RÉGLAGES DE SESSION, ceux que le serveur pousse à un client qu'il a
// reconnu (`ZC_MOONLIGHT_SETTINGS 0x0F05`).
//
// Le paquet en porte une trentaine d'un coup, à l'entrée en jeu. Ce client n'en
// lit que ceux que la fiche et le menu du staff montrent : le niveau de groupe,
// qui est un DROIT que le serveur seul accorde, et les états qu'il persiste pour
// le staff (vitesse, `@killer`, `@killable`). Ils arrivent dans
// `sim::SessionSettings`.
//
// ⚠ LA QUEUE N'EST PAS ENGENDRÉE. Le générateur s'arrête à l'en-tête de
// longueur variable ; la liste qui suit se lit ici, à la main, comme les listes
// de queue des autres paquets à compte.
//
// Disposition, fixée par le serveur (`moonlight/src/map/clif.cpp:5689`) :
//
//   [type:2][len:2][char_id:4][count:2] puis count × {id:2, value:4}

// Un réglage tel qu'il voyage.
struct PlayerSetting {
  std::uint16_t id = 0;
  std::uint32_t value = 0;
};

// L'identifiant du NIVEAU DE GROUPE, en lecture seule : le serveur y met `pc_get_group_level(sd)`
// et n'accepte jamais qu'on le lui renvoie.
inline constexpr std::uint16_t kSettingGroupLevel = 26;

// LA VITESSE DE MARCHE, et le serveur en est la
// SOURCE : il la garde dans un registre de personnage et la repose à chaque
// connexion. Le client ne la déduit de rien — elle arrive ici, ou nulle part.
//
// ⚠ PLUS BAS = PLUS RAPIDE : c'est un délai entre deux cases, pas une vitesse.
inline constexpr std::uint16_t kSettingWalkSpeed = 28;

// L'ABSENCE, et c'est le premier de cette liste que le
// CLIENT émet.
//
// 🔴 UN ORDRE, PAS UN RÉGLAGE, et le serveur le dit ainsi : il ne le persiste
// pas — on ne se reconnecte pas absent — et ne le renvoie jamais, « dont le
// client lirait ce sommeil comme une paralysie ». La valeur est un masque de
// bits : le sommeil au-dessus du personnage, l'étiquette
// « [AFK] » devant le pseudo. Ce qu'il ne connaît pas, il le laisse tomber.
inline constexpr std::uint16_t kSettingAfk = 29;

// `@killer` ET `@killable`, deux états de staff que le serveur GARDE dans un
// registre de personnage et repose à chaque connexion. Il les annonce ici à
// l'entrée en jeu et après chaque bascule, commande tapée comprise : le client
// n'en tient aucune copie.
inline constexpr std::uint16_t kSettingKiller = 30;
inline constexpr std::uint16_t kSettingKillable = 31;
// `@playertest` (plafonds GM d'ASPD et de delay ignorés) et `@nodelay` (ni
// cooldown, ni after-cast delay, ni incantation) : même régime, gardés et redits par le serveur.
inline constexpr std::uint16_t kSettingPlayerTest = 32;
inline constexpr std::uint16_t kSettingNoDelay = 33;

// LA LISTE DE RAMASSAGE AUTOMATIQUE (`@autolootid`) : un ajout, un retrait,
// l'identifiant d'item en valeur.
//
// 🔴 CHAQUE TRAME 0x0F05 EN PORTE LA LISTE ENTIÈRE : l'annonce complète ajoute
// une entrée « ajout » par item, la resynchronisation commence par un ajout de
// ZÉRO — « efface » — puis fait de même. Un ajout accepté, lui, ne revient pas :
// c'est au client de le noter d'avance, et au serveur de le démentir.
inline constexpr std::uint16_t kSettingAutolootAdd = 23;
inline constexpr std::uint16_t kSettingAutolootRemove = 24;

// LE RELAI DU CANAL DISCORD : le serveur le
// garde par personnage et le dit à l'entrée en jeu ; le client le lui écrit au
// clic. Il ne change que ce que CE client montre — la parole publique de la
// carte part vers Discord, coché ou non.
inline constexpr std::uint16_t kSettingDiscordChat = 9;

// L'en-tête, avant la liste : deux octets d'opcode, deux de longueur, quatre
// d'identifiant de personnage, deux de compte.
inline constexpr std::size_t kPlayerSettingsHeader = 10;

// Ce que pèse une entrée de la liste.
inline constexpr std::size_t kPlayerSettingSize = 6;

// Lit la liste. ⚠ TOUT OU RIEN : une trame dont le compte annoncé déborde des
// octets reçus ne rend RIEN, plutôt qu'un début de liste suivi de valeurs
// inventées. Un droit à demi lu serait pire qu'un droit absent.
[[nodiscard]] std::vector<PlayerSetting> ReadPlayerSettings(std::span<const std::uint8_t> frame);

// Applique au monde ce que ce client sait lire. Ce qu'il ne connaît pas est
// ignoré en silence : le serveur en envoie pour des fonctions que ce client n'a
// pas.
//
// ⚠ `kIgnored` et non `kApplied` : rien de ce qui se DESSINE n'en dépend. Un
// droit n'est pas un corps sur la carte, et le compteur de trames doit continuer
// de distinguer les deux.
[[nodiscard]] FrameOutcome ApplyPlayerSettingsFrame(const Frame& frame, sim::World& world);

// DIT UN RÉGLAGE AU SERVEUR (`CZ_MOONLIGHT_SETTING 0x0F04`).
//
// ⚠ CE CANAL N'EST OUVERT QU'À UN CLIENT QUI S'EST ANNONCÉ : le serveur rend la
// main sans un mot tant que la poignée de main d'intégrité n'est pas passée,
// c'est-à-dire tant qu'il n'a pas posé sur la session le drapeau qui la
// conclut. Ce n'est pas une course — la
// poignée part à l'entrée en jeu, et rien ici n'est émis avant un geste du
// joueur —, mais un envoi trop tôt se perd en silence.
//
// ⚠ RIEN À RENDRE, contrairement aux demandes d'action : ce paquet est de
// taille FIXE, son encodage ne peut pas échouer, et le serveur n'accuse rien.
// Qui veut savoir s'il est parti regarde si la liaison est ouverte — et c'est
// ce que l'appelant doit faire AVANT d'inscrire l'envoi dans son miroir.
void SendPlayerSetting(Connection& link, std::uint16_t id, std::uint32_t value);

}  // namespace ro::net
