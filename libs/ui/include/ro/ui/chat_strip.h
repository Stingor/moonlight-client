#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include <imgui.h>

#include "ro/ui/skin_art.h"

namespace ro::ui {

// Ce que la bande peut porter d'onglets. C'est la borne du CHARGEUR du client :
// au-delà il refuse, et le onzième onglet disparaîtrait à la reconnexion
// suivante sans un mot.
inline constexpr int kMostTabs = 10;

// LA BANDE D'ONGLETS DE LA CHATBOX, PEINTE À LA MAIN.
//
// ⚠ CE N'EST PAS UN `BeginTabBar`, et la différence n'est pas une question de
// goût. Une languette du client est un rectangle gris de la hauteur du texte
// plus six pixels, à peine bordé, et le GESTE qu'elle accepte n'est pas celui
// d'ImGui : clic gauche pour choisir, clic droit pour régler, et un glissement
// qui la déplace parmi ses sœurs. Une barre d'ImGui donne l'un sans les autres.
//
// ⚠ ET LE GLISSEMENT OBLIGE À UN BOUTON INVISIBLE plutôt qu'à un simple test de
// survol : sans élément déclaré, glisser une languette DÉPLACERAIT la fenêtre,
// qui n'a pas de barre de titre et se traîne donc par son corps.

// La hauteur d'une languette, au-dessus de la taille du texte.
inline constexpr float kChatTabHeightPadding = 6.0F;
// Ce qu'elle ajoute à la largeur de son nom, et où le nom se pose dedans.
inline constexpr float kChatTabWidthPadding = 14.0F;
inline constexpr float kChatTabTextLeft = 7.0F;
inline constexpr float kChatTabTextTop = 3.0F;
// L'écart entre deux languettes.
inline constexpr float kChatTabGap = 2.0F;
// Le gris de l'`UITabStrip` du client. L'onglet montré est le même, éclairci ;
// le survolé l'est à peine.
inline constexpr ImU32 kChatTabColor = IM_COL32(0x8E, 0x93, 0x8E, 255);
inline constexpr int kChatTabActiveLift = 58;
inline constexpr int kChatTabHoverLift = 24;
// Le filet d'une languette, et le presque-noir de son nom : elle est CLAIRE,
// donc son texte ne peut pas être celui du cadre.
inline constexpr ImU32 kChatTabEdgeColor = IM_COL32(0x30, 0x30, 0x30, 200);
inline constexpr ImU32 kChatTabTextColor = IM_COL32(0x14, 0x14, 0x14, 255);
// Le point d'un onglet qui a du neuf : vif, pour se voir sur le gris.
inline constexpr ImU32 kChatUnreadDotColor = IM_COL32(0xE8, 0x5A, 0x3C, 255);
// Ce qui sépare un CLIC d'un GLISSEMENT. Sans ce seuil, choisir un onglet d'une
// main un peu tremblante le déplacerait.
inline constexpr float kChatTabDragThreshold = 6.0F;
// Où le fantôme se tient par rapport au curseur, pendant le glissement.
inline constexpr float kChatTabGhostLeft = 20.0F;
inline constexpr float kChatTabGhostTop = 6.0F;

// UNE COUCHE D'UNE IMAGE D'ÉMOTICÔNE : sa texture et ses quatre coins.
//
// 🔴 UNE COUCHE SEULE NE VEUT RIEN DIRE. Une émoticone du jeu en
// superpose plusieurs : la première est la BULLE VIDE, ce qu'elle contient
// arrive par-dessus. N'en poser qu'une donne une grille de bulles vides.
//
// ⚠ QUATRE POINTS ET NON UN RECTANGLE : un quad peut être TOURNÉ, et un
// rectangle aligné aux axes ne saurait pas le représenter.
struct ChatEmotionLayer {
  ImTextureID id = 0;
  std::array<ImVec2, 4> corners{};
  ImU32 tint = IM_COL32_WHITE;
};

// Une image de l'animation : ses couches, dans l'ordre du dessin.
struct ChatEmotionFrame {
  std::span<const ChatEmotionLayer> layers;
};

// UNE ÉMOTICÔNE PROPOSÉE AU JOUEUR, telle que la grille la peint.
//
// 🔴 L'INTERFACE NE SAIT PAS LESQUELLES SE PROPOSENT. Une partie des
// numéros appartient au JEU et aux PNJ : le serveur les envoie, personne ne les
// demande. C'est `app` qui tient le catalogue et ne passe ici que ce qui s'envoie.
//
// ⚠ LE NUMÉRO VOYAGE AVEC L'IMAGE. La grille saute des rangs ; s'en remettre à la
// position d'une case ferait envoyer l'émoticone d'à côté, en silence.
struct ChatEmotion {
  std::uint16_t id = 0;
  // Son nom court, pour l'infobulle : « :smile: ».
  std::string_view name;
  // LA COMMANDE DU JEU QUI LA JOUE — « /swt ». Vide quand aucune ne la nomme.
  //
  // 🔴 ELLE N'EST PAS LE NOM COURT, et la différence décide d'un comportement.
  // `:sweat:` est un jeton de CETTE chatbox : écrit dans une macro, il ne serait
  // que du texte pour tous les autres clients. Ce qui joue la bulle au-dessus de
  // la tête, partout, c'est la commande.
  std::string_view command;
  std::span<const ChatEmotionFrame> frames;
  float frame_ms = 0.0F;
  // 🔴 LA BOÎTE DE TOUTE L'ACTION, jamais celle d'une image. C'est elle
  // qui fixe l'échelle ET le centrage : les prendre sur l'image courante les
  // ferait changer à chaque image, et le sprite se dilaterait puis se
  // contracterait au rythme de son animation, dans une case qui ne bouge pas.
  ImVec2 box_min{0.0F, 0.0F};
  ImVec2 box_max{0.0F, 0.0F};
  // SON NOM `:nom:` DANS LE CHAT, celui que Discord et les autres clients
  // lisent ; vide quand elle n'en a pas.
  std::string_view chat_token;
};

// UN GROUPE D'EMOJI, avec son intertitre DÉJÀ TRADUIT.
//
// ⚠ UN EMOJI EST DU TEXTE, là où une émoticone est une image du jeu. Il part
// tel quel, n'importe quoi qui sache l'écrire le montre — et au clic il
// S'INSÈRE dans la phrase qu'on écrit, quand une émoticone, elle, S'ENVOIE
// seule. Deux natures, deux gestes, deux onglets.
struct ChatEmojiGroup {
  std::string_view label;
  std::span<const std::string_view> items;
};

// Une languette, telle que la bande la peint. Ce qu'elle laisse passer reste
// l'affaire de l'appelant : la bande ne sait que des noms.
struct ChatStripTab {
  std::string_view name;
  // Cet onglet masque-t-il ce qui précède ? Le menu ne propose « Réafficher »
  // que s'il y a quelque chose à défaire : un menu qui propose toujours
  // d'annuler ne dit plus rien de l'état de l'onglet.
  bool cleared = false;
  // L'ONGLET DU COMPTEUR DE DÉGÂTS : son corps peint le compteur, et son menu
  // n'a ni genres, ni vidage, ni apparence de fil.
  bool meter = false;
  // Une ligne est arrivée depuis qu'on l'a regardé : un point le signale.
  bool unread = false;
};

// Ce que la bande retient d'une image à l'autre : le geste en cours, et rien de
// plus. ⚠ IL VIT CHEZ L'APPELANT, comme tout le reste : `ui` ne possède rien.
struct ChatStripState {
  // 🔴 ON A PIOCHÉ UN EMOJI, ET LE CLAVIER LUI EST DÛ — mais PAS
  // MAINTENANT. Rendre le focus au champ pendant que la palette est ouverte la
  // referme sous les doigts du joueur : ImGui ferme un popup dès que le focus
  // s'en va. Or on en pioche volontiers deux ou trois d'affilée. On retient
  // donc, et l'on rend le clavier à la FERMETURE.
  bool picked_emoji = false;

  // Le rang de la languette qu'on traîne, ou moins de zéro.
  int dragging = -1;
};

// Le côté du bouton de droite, par rapport à la hauteur d'une languette.
inline constexpr float kChatStripButtonShare = 0.8F;

// Ce que le joueur a demandé pendant cette image. Tout est facultatif.
struct ChatStripAction {
  // Il a cliqué le bouton de droite : montrer ou cacher la recherche.
  bool toggled_search = false;
  // Il a choisi cet onglet.
  int chose = -1;
  // Il veut régler celui-là — clic droit. C'est l'onglet DÉSIGNÉ, pas l'onglet
  // montré : on règle celui qu'on vise.
  int configure = -1;
  // L'émoticone SANS NOM DE CHAT qu'il vient de choisir dans la grille, ou moins
  // de zéro : faute de `:nom:` à écrire, elle part en GESTE du jeu, au-dessus de
  // la tête.
  int chose_emotion = -1;
  // Le `:nom:` d'une émoticone cliquée, à ENVOYER TOUT DE SUITE, seul sur sa
  // ligne. Vide sinon.
  //
  // 🔴 SEUL SUR SA LIGNE, sans passer par la saisie : le relais Discord ne
  // remplace le `:nom:` par son image que s'il est TOUT le message. Ce que le
  // joueur avait tapé reste dans la saisie, intact.
  std::string_view sent_emote_token;
  // La palette vient de se refermer après une pioche : le clavier revient à la
  // saisie, comme après un envoi.
  bool give_back_keyboard = false;
  // L'emoji qu'on vient de piocher, à INSÉRER dans la saisie. Vide sinon.
  //
  // ⚠ IL S'INSÈRE, LÀ OÙ L'ÉMOTICONE S'ENVOIE : un emoji est du texte, il vit
  // au milieu d'une phrase. On en pioche volontiers deux ou trois d'affilée,
  // donc la palette reste ouverte.
  std::string_view chose_emoji;
  // Il a lâché une languette : elle passe de `moved_from` au rang `moved_to`.
  //
  // ⚠ LE RANG EST COMPTÉ DANS LA BANDE TELLE QU'ELLE EST DESSINÉE, celle qu'on
  // déplace comprise. C'est ce que le joueur voit, donc la seule numérotation qui
  // puisse correspondre à son geste ; c'est à l'appelant de faire la conversion.
  int moved_from = -1;
  int moved_to = -1;
  // Maj + clic sur une émoticone : son nom `:nom:` à ÉCRIRE dans la saisie, au
  // lieu de l'envoyer. Vide sinon.
  std::string_view chose_emote_token;
};

// LE PLATEAU DU GLISSER, partagé par toutes les bandes d'une même image.
//
// 🔴 UNE LANGUETTE LÂCHÉE NE PEUT PAS SE TRANCHER SUR PLACE. La bande qui
// tient le geste est celle d'où l'on part ; celle où l'on dépose est dessinée
// ailleurs dans l'image, parfois APRÈS. Au moment où le bouton se relâche, la
// bande de départ ne sait donc pas ce qui est sous le curseur. Chacune publie
// ici ce qu'elle occupe, et le verdict se rend une fois toutes dessinées.
//
// ⚠ REFAIT À CHAQUE IMAGE : ce sont des mesures, pas un état. Seul le geste en
// cours traverse, et il est borné par le bouton de la souris.
struct ChatDragBoard {
  // Une bande telle qu'elle vient de se dessiner.
  struct Strip {
    std::uint32_t group = 0;
    ImVec2 min;
    ImVec2 max;
    // La fenêtre qui la porte. ⚠ NÉCESSAIRE POUR DISTINGUER UN GESTE ANNULÉ
    // d'un arrachement : lâcher dans SA propre fenêtre, hors de la bande, ne
    // doit rien faire — sinon un glissement de six pixels vers le fil sortirait
    // l'onglet dans une fenêtre que personne n'a demandée.
    ImVec2 window_min;
    ImVec2 window_max;
    // Le milieu de chaque languette dessinée : c'est le point de bascule qui dit
    // OÙ la déposée se rangera. Relevé par la bande, qui vient de le mesurer.
    std::array<float, kMostTabs> middles{};
    int count = 0;
  };

  std::vector<Strip> strips;
  // D'où part le geste en cours : la fenêtre, et le rang DANS SA bande.
  std::uint32_t from_group = 0;
  int from_tab = -1;
  // Posé par la bande de départ quand la languette tombe AILLEURS qu'en elle.
  bool released = false;
  ImVec2 at;
};

// OÙ UNE LANGUETTE VIENT D'ÊTRE DÉPOSÉE.
struct ChatDrop {
  bool asked = false;
  std::uint32_t from_group = 0;
  int from_tab = -1;
  // La fenêtre d'arrivée, et le rang qu'elle y prend DANS CETTE BANDE-LÀ.
  // `to_new_window` l'emporte : l'onglet fonde alors la sienne.
  std::uint32_t to_group = 0;
  int to_rank = -1;
  bool to_new_window = false;
  // Où le curseur l'a lâchée, en pixels d'écran : une fenêtre qu'elle fonde
  // peut y naître.
  ImVec2 at{0.0F, 0.0F};
};

// UNE PLACE DANS UNE BANDE : sa fenêtre, et le rang qu'on y prendrait.
struct ChatStripSpot {
  std::uint32_t group = 0;
  int rank = 0;
  // L'abscisse du repère d'insertion, et la hauteur de la bande.
  float marker_x = 0.0F;
  float top = 0.0F;
  float bottom = 0.0F;
};

// La bande sous ce point, et la place qu'un dépôt y prendrait — ou rien.
// ⚠ À LIRE UNE FOIS LES BANDES DE L'IMAGE DESSINÉES : le plateau ne porte que
// leurs mesures de l'image en cours.
[[nodiscard]] std::optional<ChatStripSpot> ChatStripUnder(const ChatDragBoard& board, ImVec2 at);

// Rend le verdict du lâcher, et vide le plateau de son geste.
//
// ⚠ APPELÉ UNE FOIS TOUTES LES FENÊTRES DESSINÉES, et c'est le seul moment où
// l'on sait laquelle était sous le curseur.
[[nodiscard]] ChatDrop DecideChatDrop(ChatDragBoard& board);

// Dessine la bande et rend sa hauteur. Le curseur en ressort SOUS elle, prêt
// pour ce qui suit.
//
// `group` dit à quelle fenêtre elle appartient, et `board` reçoit ce qu'elle
// occupe — voir `ChatDragBoard`. Un déplacement DANS la bande est tranché sur
// place, par `asked` : personne d'autre n'a besoin de le savoir.
// Les vignettes des émoticones, dans l'ordre de leur numéro — c'est lui que le
// serveur attend. Une case vide reste cliquable : le sprite peut ne pas porter
// celle-là, et sauter son rang décalerait toutes les suivantes.
//
// ⚠ VIDE, LE BOUTON DE LA GRILLE DISPARAÎT. Sans le sprite d'émoticones, il n'y
// a rien à montrer, et un bouton qui s'ouvre sur rien se lit comme un bouton
// cassé.
// ⚠ `tab_color` EST CELLE DU REPOS. L'onglet montré est le même gris éclairci,
// le survolé l'est à peine : trois teintes tirées d'une seule, pour qu'un
// réglage ne demande pas trois réponses à la même question.
// UNE ÉMOTICÔNE DANS SA CASE : toutes ses couches, à l'échelle qui fait tenir
// l'ACTION ENTIÈRE, centrées sur elle. `now_s` fait tourner l'animation.
//
// ⚠ PARTAGÉE PARCE QUE DEUX GRILLES LA PEIGNENT : celle de la bande, et celle du
// menu des macros. Deux peintres pour un seul sprite finiraient par le cadrer
// différemment, et l'écart ne se verrait que sur les émotes les plus larges.
void PaintEmotion(ImDrawList* into, const ChatEmotion& emotion, ImVec2 corner, float side,
                  double now_s);

// LE BOUTON DES ÉMOTICONES ET SA PALETTE, posés en `at` sur `side` pixels de côté.
//
// ⚠ PARTAGÉ PARCE QUE DEUX SAISIES L'OFFRENT : la bande de la conversation, et
// une conversation 1:1. Chacune garde son `state`, et dit ce que fait le clic.
//
// ⚠ DANS UN BLOC DÉSACTIVÉ, le bouton se grise et ne s'ouvre plus.
//
// Partout, le clic ENVOIE le `:nom:` de l'émoticone et Maj + clic l'ÉCRIT dans
// la saisie. Ce que dit le choix, c'est le sort de celles qui n'ont pas de nom.
enum class ChatEmoteClick : std::uint8_t {
  // Sans nom, le clic l'envoie en geste du jeu, au-dessus de la tête.
  kSend,
  // Sans nom, elle est grisée : dans une conversation 1:1, un geste du jeu ne
  // serait vu que des voisins, jamais du correspondant.
  kTokenOnly,
};
void DrawChatEmotePicker(ChatStripState& state, ImVec2 at, float side, ImU32 tab_color,
                         std::span<const ChatEmotion> emotions,
                         std::span<const ChatEmojiGroup> emojis, ChatStripAction* asked,
                         ChatEmoteClick click = ChatEmoteClick::kSend);

float DrawChatStrip(ChatStripState& state, std::span<const ChatStripTab> tabs, int current,
                    ImU32 tab_color, bool searching, std::uint32_t group,
                    std::span<const ChatEmotion> emotions, std::span<const ChatEmojiGroup> emojis,
                    ChatDragBoard& board, ChatStripAction* asked);

}  // namespace ro::ui
