#pragma once

#include <array>
#include <cstdint>
#include <deque>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <glm/glm.hpp>

#include "ro/audio/audio_engine.h"
#include "ro/net/chat_target.h"
#include "ro/net/connection.h"
#include "ro/net/frame_tally.h"
#include "ro/render/camera.h"
#include "ro/render/font_atlas.h"
#include "ro/render/screen_overlay.h"
#include "ro/render/sprite_renderer.h"
#include "ro/sim/banned_words.h"
#include "ro/sim/player_config.h"
#include "ro/sim/walk_path.h"
#include "ro/sim/whisper.h"
#include "ro/sim/whisper_blocks.h"
#include "ro/sim/world_fwd.h"
#include "ro/vfs/item_table.h"
#include "ro/vfs/item_words.h"

#include "chat/chat_item_link.h"
#include "chat/chat_rich_decor.h"
#include "effects/effect_view.h"
#include "game/ui_stage.h"
#include "options/game_options.h"
#include "options/setting_links.h"
#include "world/map_music.h"
#include "world/map_stage.h"

// La PAROLE : ce qu'on tape, ce qu'on lit, et ce qui flotte au-dessus des
// têtes. Tout s'y décrit en pixels de fenêtre, et rien n'y est dessiné.
#include "ro/ui/chat_bubble.h"

namespace ro::sim {
// L'ARBRE DES COMPÉTENCES, en DÉCLARATION SEULE : cet en-tête n'en lit qu'un
// libellé, et l'inclure ferait entrer les tables Lua du client dans tout ce qui
// dessine une bulle.
class SkillTree;
}  // namespace ro::sim

namespace moonclient {

class ChatLinePreviewMaker;
class ChatRoomStage;
class ChatTabsView;
struct FollowState;
class NaviSession;

// L'émoticône qu'une ligne demande, ou rien si elle n'en demande aucune.
//
// Les commandes viennent du jeu lui-même : celles que le serveur documente dans
// son énumération emotion_type (src/map/clif.hpp) et celles de la liste
// classique, dans le même ordre qu'elle. Une commande inconnue n'est PAS
// envoyée au serveur — le jeu les traite chez lui.
[[nodiscard]] std::optional<std::uint16_t> EmotionCommand(std::string_view line);

// LE CHEMIN INVERSE : la commande qui joue cette émoticône-là, ou une vue vide.
//
// 🔴 LA MÊME TABLE QUE CI-DESSUS, LUE À L'ENVERS, et c'est ce qui les garde
// d'accord. Une seconde liste finirait par proposer une commande que le routage
// ne reconnaît pas — donc une macro qui ne joue rien et se dit tout haut.
//
// ⚠ LA PREMIÈRE FORME L'EMPORTE quand plusieurs nomment la même : « /ho » avant
// « /delight ». C'est la courte, celle que le client écrit lui-même.
//
// ⚠ ET UNE ÉMOTICÔNE PEUT N'EN AVOIR AUCUNE : le sprite en porte près de cent,
// la liste du jeu n'en nomme qu'une partie. Ce qui n'a pas de commande n'a rien
// à faire dans une macro — elle ne jouerait rien chez les autres clients.
[[nodiscard]] std::string_view EmotionCommandOf(std::uint16_t emotion);

// Ce à quoi une ligne tapée peut s'adresser.
//
// Passé par pointeurs plutôt que gardé : le routage ne POSSÈDE rien, il ne fait
// que diriger. C'est aussi ce qui laisse la carte changer sous lui sans qu'il
// ait à s'en apercevoir.
struct ChatDispatch {
  ro::net::Connection* link = nullptr;
  ro::sim::World* world = nullptr;
  EffectsInPlay* effects = nullptr;
  EffectNodesInPlay* effect_nodes = nullptr;
  const MapStage* stage = nullptr;
  ro::sim::WalkPlanner* planner = nullptr;
  // Ce que les trames reçues sont devenues. En LECTURE : la boucle le remplit,
  // /trames ne fait que le dire.
  const ro::net::FrameTally* frames = nullptr;
  // L'interrupteur de l'INSPECTEUR DE TRAMES, que /trames bascule en Debug. Il vit dans
  // la boucle, qui seule dessine, et le routage ne fait que le retourner.
  bool* packet_window = nullptr;
  // Celui de la FICHE DE PERSONNAGE, que /grimoire bascule en Debug.
  bool* skill_window = nullptr;
  bool* inventory_window = nullptr;
  // Celui de la FENÊTRE DES MACROS, que /macros bascule. Le jeu l'ouvre par
  // Alt+M ; la commande vient en plus, comme pour le grimoire et le sac.
  bool* macro_window = nullptr;
  // L'ATLAS DES RECETTES, que /atlas OUVRE — sans jamais le refermer.
  ro::ui::CraftAtlasWindowState* craft_atlas = nullptr;
  // LE TABLEAU DES CLASSEMENTS, dont `/ranking` demande l'ouverture. Nul : la
  // commande se tait.
  bool* ranking_board_asked = nullptr;
  // LA NAVIGATION, que `/navi` et `/navigation` ouvrent ou lancent : le guidage
  // et les tables de recherche, la fenêtre, et les noms affichés des cartes.
  NaviSession* navi = nullptr;
  ro::ui::NavigationWindowState* navigation = nullptr;
  const ro::vfs::MapDisplayNames* map_names = nullptr;
  // CE QUE LA BARRE A ARMÉ : le mode du sélecteur, et le destinataire de la
  // box « Pseudo ».
  //
  // ⚠ EN LECTURE, ET PASÉ TEL QUEL À `ReadChatIntent` : le routage ne décide
  // rien de ce que cela vaut face à un sigil — c'est `net` qui ordonne les
  // trois chemins, et lui seul.
  const ro::net::ChatArming* armed = nullptr;
  // LA MÊME, EN ÉCRITURE, pour la seule commande qui arme un mode :
  // `/battlechat`, qui bascule la barre vers le champ de bataille et retour.
  ro::net::ChatArming* arming = nullptr;
  // LES ONGLETS, pour `/savechat` — et pour rien d'autre. Le routage ne trie
  // pas, ne dessine pas et ne règle rien : il demande le dump et redit ce
  // qu'on lui répond. En salon, le fil du salon s'y ajoute.
  ChatTabsView* chat_tabs = nullptr;
  // LES OPTIONS DU JOUEUR, et ce qu'elles touchent.
  //
  // ⚠ QUATRE COMMANDES SONT DES RÉGLAGES — /bgm, /snd, /camera, /echelle —
  // et elles passent par le MÊME chemin que la fenêtre d'options.
  // Le routage ne touche donc pas le mélangeur ni la caméra lui-même : deux
  // chemins qui écriraient chacun leur valeur finiraient par se contredire, et
  // l'un des deux oublierait d'enregistrer.
  GameOptions* options = nullptr;
  OptionTargets option_targets;
  // LES SALONS, dont `/chat` ouvre le formulaire.
  ChatRoomStage* chat_rooms = nullptr;
  // LES ENVOIS QUI ATTENDENT LEUR ACCUSÉ : l'écho d'un chuchotement se compose
  // de ce qu'on y garde. Nul : aucun écho.
  ro::sim::WhisperOutbox* whispers = nullptr;
  // QUI NE PEUT PLUS NOUS CHUCHOTER : `/ex` et `/in` y notent leur demande,
  // que la réponse du serveur retrouve. Nul : ces commandes se taisent.
  ro::sim::WhisperBlockList* whisper_blocks = nullptr;
  // LA BARRE, qui suit le canal où une commande partie a lié la parole
  // publique : sa place en dépend. Nul : rien ne se suit.
  ChatLinePreviewMaker* line_preview = nullptr;
  // LE SUIVI D'UN JOUEUR, que `/follow` commence ou arrête. Nul : la commande
  // part au serveur comme une parole.
  FollowState* follow = nullptr;
  // LES RÉGLAGES QUE LE SERVEUR A DITS, en lecture : `/eqopen` demande
  // l'inverse de l'état de l'équipement. Nul : la commande se tait.
  const ro::sim::PlayerConfig* own_config = nullptr;
  // LES MOTS INTERDITS, masqués dans toute parole qui part. Nul : rien ne se
  // masque.
  const ro::sim::BannedWords* banned = nullptr;
};

// LA COMMANDE QUI COLLE LA CAMÉRA AU PERSONNAGE, ou la laisse le rattraper.
//
// C'est celle du jeu, au même nom : `TT_FIXED_CAMERA_ON_OFF`, le réglage 0x36
// que la caméra lit pour rattraper sa cible. Elle part ÉTEINTE, comme là-bas —
// la caméra rattrape, et c'est ce qui donne à RO son flottement.
inline constexpr std::string_view kFixedCameraCommand = "/camera";

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

// LE BATTLE MODE : la barre de saisie se replie, et le clavier revient au jeu.
//
// ⚠ LES DEUX NOMS DU CLIENT, et pas un de plus : c'est une BASCULE, et un
// « on »/« off » en argument y est ignoré — dans le client d'origine comme ici.
// Les règles de la barre vivent dans `ro/ui/chat_bar.h`.
inline constexpr std::string_view kBattleModeCommand = "/bm";
inline constexpr std::string_view kBattleModeCommandLong = "/battlemode";

// Route une ligne tapée.
//
// Une émoticône part au serveur, qui la renvoie à tout le monde ; un effet se
// joue CHEZ NOUS, sans rien demander à personne ; une commande inconnue se dit
// tout haut à soi-même. Tout le reste part au serveur — les commandes
// d'administration y comprises, qu'il reconnaît dans le texte.
void SubmitChat(const ChatDispatch& to, const std::string& line, std::uint32_t said_ms);

// CE QUE `/savechat` ÉCRIT : les onglets, et en salon le fil du salon. Le menu
// d'un membre n'écrit que le salon (`tabs_too` faux). Le dossier, ou l'absence
// de ligne, se dit dans la conversation.
void SaveChatAndSay(const ChatTabsView& tabs, const ChatRoomStage* rooms, bool tabs_too,
                    ro::sim::World& world, std::uint32_t said_ms);

// LES PAROLES RÉCENTES, projetées à l'écran : où flotte chacune, et depuis
// combien de temps.
//
// 🔴 DES POINTS, ET NON DES QUADS. Une bulle se peint par le MÊME texte que la
// conversation — même police, mêmes emoji, mêmes couleurs. Notre atlas de
// rendu, gravé en niveaux de gris et borné au latin-1, montrerait en losanges
// au-dessus des têtes la phrase que le fil affiche correctement. Deux piles de
// texte pour une seule phrase finissent toujours par diverger.
//
// `drawn` et `ids` sont les deux moitiés de ce que la scène vient de peindre :
// le quoi et le qui.
// 🔴 ET PAR LA MÊME TRADUCTION DES BALISES. Sans elle, ce que le joueur lit
// au-dessus d'une tête est le `<ITEML>…</ITEML>` brut pendant que son fil affiche
// « +4 Knife [3] » — deux rendus d'une seule phrase, exactement ce que l'alinéa
// ci-dessus interdit. Le balisage suit le même chemin : `**gras**`, `^RRGGBB`,
// `:nom:` et `||spoiler||` deviennent des styles, jamais du texte.
//
// ⚠ `decorated` GARDE LES PHRASES RÉÉCRITES le temps que les bulles soient
// peintes : les vues POINTENT dedans. Une file, pas un vecteur — celui-ci les
// déplacerait en grandissant. Elle est vidée ici à chaque appel.
//
// ⚠ `items` ET `words` PEUVENT MANQUER, avant l'entrée en jeu : les balises
// restent alors telles quelles, comme dans le fil.
//
// 🔴 ET LA BULLE PORTE AUSSI LA BANNIÈRE DU NOM D'UNE COMPÉTENCE. Chez le
// client c'est le MÊME message : le mode de jeu compose « <nom> !!  » et
// l'envoie à l'acteur par le message 7, celui d'une phrase. `skills` donne le
// libellé affiché, et il peut manquer tant que les tables du client ne sont
// pas lues — la bannière tombe alors sur le numéro.
// ⚠ LA VUE RENDUE POINTE, ELLE NE COPIE PAS, et une BANNIÈRE n'appartient à
// aucun acteur : elle est composée ici et rangée dans `decorated`, qui doit
// donc exister pour qu'elle paraisse.
//
// `item_icon` est celui du fil : un objet lié y porte la même icône. Nul : la
// bulle n'en montre aucune. `settings` aussi : un lien de réglage n'y est un
// lien que si le lecteur a sa destination, et sans lui, aucun ne l'est.
//
// ⚠ LA VIE D'UNE BULLE SE MESURE SUR CE QU'ELLE MONTRE, décoration faite.
[[nodiscard]] std::vector<ro::ui::ChatBubbleView> ChatBubblesOnScreen(
    const ro::render::Camera& camera, float aspect_ratio,
    std::span<const ro::render::DrawnActor> drawn, std::span<const std::uint32_t> ids,
    const ro::sim::World& world, std::uint32_t now_ms, float viewport_width, float viewport_height,
    const ro::ui::ChatBubbleLook& look, const ro::vfs::ItemTable* items,
    const ro::vfs::ItemWords* words, std::deque<DecoratedChatLine>* decorated,
    const ro::sim::SkillTree* skills, const CreatorNames* creators = nullptr,
    const ro::vfs::MapDisplayNames* maps = nullptr, const ItemIconLookup* item_icon = nullptr,
    const SettingReach& settings = {});

}  // namespace moonclient
