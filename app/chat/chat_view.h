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
#include "chat/client_command_words.h"
#include "chat/emotion_commands.h"
#include "chat/player_command.h"
#include "chat/setting_toggle_command.h"
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
class DatabaseStage;
class ChatTabsView;
struct FollowState;
class NaviSession;

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
  // LE MARCHÉ, dont `/marche` demande l'ouverture. Nul : la commande se tait.
  MarketStage* market = nullptr;
  // LA BASE DE DONNÉES, que `/db` ouvre. Nulle : la commande se tait.
  DatabaseStage* database = nullptr;
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
  // LES COMMANDES DU JOUEUR QUI VISENT LE GROUPE, LA GUILDE OU UN ÉCHANGE : le
  // routage les lit, la partie les exécute — elle seule tient ce qu'elles
  // touchent. Nul : elles se taisent. Voir `RunPlayerCommand`.
  std::vector<PlayerCommand>* player_asks = nullptr;
};

// Route une ligne tapée.
//
// Une émoticône part au serveur, qui la renvoie à tout le monde ; un effet se
// joue CHEZ NOUS, sans rien demander à personne ; une commande inconnue se dit
// tout haut à soi-même. Tout le reste part au serveur — les commandes
// d'administration y comprises, qu'il reconnaît dans le texte.
//
// `keys` sont les touches tenues à la validation : Ctrl+Entrée et Alt+Entrée
// retournent la destination d'une parole sans sigil — voir
// `ro::net::ChatArming::keys`. Une macro ou un menu n'en tiennent aucune.
void SubmitChat(const ChatDispatch& to, const std::string& line, std::uint32_t said_ms,
                ro::net::ChatKeys keys = {});

// UNE COMMANDE DU JOUEUR, lue par `ReadPlayerCommand`. Celles qui ne touchent
// que la liaison, les options ou la conversation s'exécutent ici ; les autres
// rejoignent `player_asks`. Une forme fausse dit la bonne.
void RunPlayerCommand(const ChatDispatch& to, PlayerCommand command, std::uint32_t said_ms);

// UNE BASCULE « / » D'UNE CASE DES OPTIONS, lue par `ReadSettingToggle` : le
// réglage s'inverse par le chemin de la case, puis l'écho s'écrit.
void RunSettingToggle(const ChatDispatch& to, SettingToggle toggle, std::uint32_t said_ms);

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
