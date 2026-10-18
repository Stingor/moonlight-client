#pragma once

// LES MOTS DES COMMANDES « / » QUE `SubmitChat` COMPARE LUI-MÊME.
//
// Ils vivent à part du routage pour se lire sans lui : la liste des commandes
// du chat et son essai les comparent à la table `slash_commands.cpp`.

#include <array>
#include <string_view>

namespace moonclient {

// LA COMMANDE QUI COLLE LA CAMÉRA AU PERSONNAGE, ou la laisse le rattraper.
//
// C'est celle du jeu, au même nom : `TT_FIXED_CAMERA_ON_OFF`, le réglage 0x36
// que la caméra lit pour rattraper sa cible. Elle part ÉTEINTE, comme là-bas —
// la caméra rattrape, et c'est ce qui donne à RO son flottement.
inline constexpr std::string_view kFixedCameraCommand = "/camera";

// LA BASCULE DU CHAT DE CHAMP DE BATAILLE : armée, tout ce qu'on tape sans sigil
// part à l'équipe. À la casse près, comme toute la table des commandes du jeu.
inline constexpr std::string_view kBattleChatCommand = "/battlechat";

// Laisse reculer la caméra plus loin dehors. Éteinte par défaut.
inline constexpr std::string_view kExpandedZoomCommand = "/zoom";

// Montre ou cache les ombres de la carte, sur le sol comme sur les corps. Celle
// du jeu, au même nom ; allumée par défaut.
inline constexpr std::string_view kLightmapCommand = "/lightmap";

// Montre ou cache le brouillard des cartes qui en ont un. Celle du jeu, au même
// nom ; allumée par défaut.
inline constexpr std::string_view kFogCommand = "/fog";

// Montre ou cache le bouton à part du Vote Shop. Celle du jeu,
// au même nom ; allumée par défaut. ⚠ UNE BASCULE SANS ARGUMENT, comme là-bas.
inline constexpr std::string_view kCashShopButtonCommand = "/cashshop";

// LES DEUX MODIFICATEURS DE CIBLAGE, et leurs alias courts.
//
// 🔴 CE SONT DES COMMANDES DU JEU, et elles restent à la sortie. Le client
// d'origine les tient dans sa table des commandes sous les identifiants 105 et
// 109 — lesquels sont AUSSI les numéros de ses réglages `TT_NOSHIFT_ON_OFF` et
// `TT_NOCTRL_ON_OFF` : chez lui comme ici, la commande et la case à cocher sont
// le même interrupteur.
//
// ⚠ L'ALIAS PORTE LE MÊME NUMÉRO, il ne fait rien de différent : deux entrées,
// un seul réglage. Ne pas le reconnaître ferait passer `/ns` pour une commande
// inconnue alors que c'est la forme que les joueurs emploient.
//
// ⚠ ET CES NOMS NE SE TRADUISENT PAS : un joueur qui a tapé `/ns` depuis dix
// ans le retapera ici.
inline constexpr std::string_view kNoShiftCommand = "/noshift";
inline constexpr std::string_view kNoShiftShortCommand = "/ns";
inline constexpr std::string_view kNoCtrlCommand = "/noctrl";
inline constexpr std::string_view kNoCtrlShortCommand = "/nc";

// ACCEPTER OU REFUSER L'APPEL D'URGENCE. Celle du jeu, au même nom : elle
// bascule la case « Accepter l'appel d'urgence » de Gameplay, et ne dit rien
// elle-même — c'est l'écho du serveur qui écrit la ligne.
inline constexpr std::string_view kCallCommand = "/call";

// LES DEUX BASCULES DES ÉCHOPPES, celles du jeu sous leurs noms — voir
// `ShopOptions`. `/sh` est l'alias de `/shopping`, un seul réglage.
// ⚠ `/showshop` NE DIT RIEN dans le chat, comme au client d'origine : l'effet se
// voit, les enseignes paraissent ou disparaissent.
inline constexpr std::string_view kShoppingCommand = "/shopping";
inline constexpr std::string_view kShoppingShortCommand = "/sh";
inline constexpr std::string_view kShowShopCommand = "/showshop";

// OUVRE LE FORMULAIRE DE SALON. Celle du jeu, et son double : « /채팅방 » tapé
// sur un clavier coréen resté en mode latin.
inline constexpr std::string_view kChatRoomCommand = "/chat";
inline constexpr std::string_view kChatRoomHangulCommand = "/coxldqkd";
// QUITTE LE SALON : celle du jeu, que porte aussi la croix de sa salle. Hors
// salon, elle ne fait rien.
inline constexpr std::string_view kChatRoomLeaveCommand = "/q";

// LA COMMANDE QUI OUVRE LA LISTE DES RACCOURCIS — les dix macros de chat. Le
// jeu l'ouvre par Alt+M ; la commande vient en plus, et ne demande aucun
// clavier partagé.
inline constexpr std::string_view kMacroWindowCommand = "/macros";

// LES COMMANDES QUI OUVRENT L'ATLAS DES RECETTES.
//
// 🔴 ELLES OUVRENT, ELLES NE BASCULENT PAS : une commande TAPÉE est une demande
// à sens unique, et la refermer parce qu'elle était déjà ouverte se lirait comme
// une commande qui n'a pas marché. ⚠ ET SANS LA CASSE : « /Atlas » est la même
// intention, et la refuser renverrait le joueur là d'où on veut le sortir.
inline constexpr std::array<std::string_view, 3> kCraftAtlasCommands = {"/atlas", "/recipes",
                                                                        "/recettes"};

// Le premier mot de la ligne est-il l'une de ces commandes ?
[[nodiscard]] bool IsCraftAtlasCommand(std::string_view line);

// LE RETOUR AU POINT DE SAUVEGARDE D'UN PERSONNAGE VIVANT.
//
// 🔴 CE N'EST PAS UNE COMMANDE DU CLIENT, ET C'EST TOUT L'INTÉRÊT : elle
// commence par une arobase, donc elle part telle quelle au SERVEUR, qui seul
// sait téléporter. Rien dans le protocole ne le permet autrement — le paquet de
// réapparition, lui, n'est servi qu'à un mort.
//
// ⚠ ELLE NE SE TRADUIT PAS : c'est un nom de commande serveur.
inline constexpr std::string_view kSavePointChatCommand = "@load";

// LES DEUX COMMANDES DE SON DU JEU, et elles restent à la sortie.
//
// ⚠ LE JEU EN A DEUX NOMS POUR LES BRUITS : le client officiel les coupe par
// `/sound`, mais `/snd` est la forme courte que les joueurs emploient. On
// reconnaît les deux — une commande qui existe sous deux noms dans la bouche
// des joueurs et sous un seul dans le client est une commande qu'on croit
// cassée.
// L'ECHELLE de l'interface. Sans argument elle se DIT ; avec un facteur, elle
// se pose et se retient d'une partie a l'autre.
inline constexpr std::string_view kScaleCommand = "/echelle";
// ET SON ALIAS ANGLAIS, au nom que portent les commandes du jeu (`/bgm`,
// `/effect`) : un joueur de RO la cherche plutôt sous ce nom-là.
inline constexpr std::string_view kScaleAltCommand = "/scale";

// LA COMMANDE QUI ÉCRIT LA CONVERSATION SUR LE DISQUE, un fichier par onglet.
//
// ⚠ C'EST CELLE DU CLIENT, au même nom et au même endroit — `<racine>\Chat\`.
// Un joueur qui la connaît doit la retrouver là où il l'a laissée ; voir
// `chat_log_file.h` et ADR 0033, entrée A4.
inline constexpr std::string_view kSaveChatCommand = "/savechat";

inline constexpr std::string_view kMusicCommand = "/bgm";
inline constexpr std::string_view kSoundCommand = "/snd";
inline constexpr std::string_view kSoundCommandLong = "/sound";

// LES DEUX COMMANDES D'EFFET DU JEU, et elles ne font PAS la même chose.
//
// `/effect` crible l'AFFICHAGE : le client coupe le dessin, sauf pour une
// liste blanche de cent quatorze identifiants. `/mineffect` choisit une
// RESSOURCE : il fait prendre une variante allégée là où le client en connaît
// une. Les ranger comme une paire de jumelles serait l'erreur plausible.
//
// ⚠ DEUX BASCULES AVEUGLES, SANS ARGUMENT : chez le client, ce qui suit la
// commande est analysé puis JETÉ sans être relu — « /effect off » bascule
// exactement comme « /effect ».
inline constexpr std::string_view kEffectsShownCommand = "/effect";
inline constexpr std::string_view kMinEffectCommand = "/mineffect";
// ⚠ UN SECOND NOM QUE SEULE LA TABLE DE MESSAGES RÉVÈLE : après avoir comparé
// à « /mineffect », le client compare au TEXTE du message 698 et saute au même
// endroit. Ce texte vaut « /minimize » dans cette installation — une
// traduction décide donc ici quels MOTS sont des commandes.
inline constexpr std::string_view kMinEffectAltCommand = "/minimize";
// `/skyeffect` : une bascule à nous, sans équivalent chez le client. Elle ne
// coupe que le ciel et la météo de la carte — voir `Game::RaiseMapSky`.
inline constexpr std::string_view kSkyEffectsCommand = "/skyeffect";

// LE BATTLE MODE : la barre de saisie se replie, et le clavier revient au jeu.
//
// ⚠ LES DEUX NOMS DU CLIENT, et pas un de plus : c'est une BASCULE, et un
// « on »/« off » en argument y est ignoré — dans le client d'origine comme ici.
// Les règles de la barre vivent dans `ro/ui/chat_bar.h`.
inline constexpr std::string_view kBattleModeCommand = "/bm";
inline constexpr std::string_view kBattleModeCommandLong = "/battlemode";

}  // namespace moonclient
