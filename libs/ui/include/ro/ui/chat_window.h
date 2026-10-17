#pragma once

#include <array>
#include <cstddef>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/ui/chat_frame.h"
#include "ro/ui/chat_input.h"
#include "ro/ui/chat_log.h"
#include "ro/ui/chat_picture_viewer.h"
#include "ro/ui/chat_rich_art.h"
#include "ro/ui/chat_strip.h"
#include "ro/ui/chat_url_prompt.h"
#include "ro/ui/damage_meter_window.h"
#include "ro/ui/skin_art.h"
#include "ro/ui/window_name.h"

namespace ro::ui {

// LA CHATBOX : ce qui s'est dit, et ce qu'on répond.
//
// ⚠ ELLE NE CLASSE RIEN, ET NE FILTRE RIEN. Une ligne lui arrive avec SA COULEUR
// déjà choisie, et la liste qu'elle reçoit est DÉJÀ celle de l'onglet courant :
// `ui` ne connaît pas `sim`, donc pas le genre d'un message. C'est `app` qui
// traduit l'un en l'autre et qui trie — la fenêtre ne sait que compter des
// languettes.
//
// ⚠ ELLE N'A PAS DE BARRE DE TITRE, comme dans le client : un rectangle
// translucide, ses languettes peintes, ses lignes, sa saisie. On la déplace en
// l'attrapant là où il n'y a rien à cliquer, et on la redimensionne par ses
// QUATRE BORDS — voir `chat_frame.h` et `chat_strip.h`, où vivent les raisons.
//
// Ce qui lui manque encore par rapport au client d'origine est inventorié dans
// ADR 0033 : les genres de message que le client COMPOSE lui-même, et les liens
// d'objet.

inline constexpr const char* kChatWindowTitle = core::TrText("Conversation");
inline constexpr const char* kChatWindowId = "chat";

// L'IDENTITÉ IMGUI DE LA FENÊTRE D'UN GROUPE. Zéro rend celle de la principale.
//
// 🔴 C'EST ELLE QUI PORTE LA GÉOMÉTRIE : ImGui retrouve la position et la
// taille d'une fenêtre par son identité, et par rien d'autre. Elle ne doit donc
// jamais changer pour une fenêtre donnée — c'est aussi pourquoi un numéro de
// groupe ne se réemploie pas (voir `ChatTab::group`).
[[nodiscard]] std::string ChatWindowId(std::uint32_t group);

// Ce qu'un nom d'onglet peut porter. Une languette large mange la place des
// autres : le client s'arrête bien avant.
inline constexpr std::size_t kChatTabNameSize = 32;
// Ce qu'on peut chercher. Une recherche est un mot ou deux, pas une phrase.
inline constexpr std::size_t kChatSearchSize = 64;

// Les bornes de la taille du texte du fil, en pourcentage. Sous 70 on ne lit
// plus, au-dessus de 160 deux lignes tiennent à l'écran.
inline constexpr int kSmallestChatFontPercent = 70;
inline constexpr int kLargestChatFontPercent = 160;
// Un cran de Ctrl + molette, en points de pourcentage de la taille du texte.
inline constexpr int kChatFontStepPercent = 5;
inline constexpr int kLargestChatPaddingPixels = 12;
inline constexpr int kLargestChatLineGapPixels = 16;

// Un type de message dans les filtres d'un onglet : son libellé DÉJÀ TRADUIT,
// et s'il passe. L'ordre est celui de l'appelant, et c'est lui qui donne son
// sens à `toggled_type`.
struct ChatTypeToggle {
  std::string_view label;
  // Son numéro, « t07 », montré devant le libellé en mode diagnostic : c'est
  // celui que porte en tête chaque ligne du fil, et il relie l'une à l'autre.
  std::string_view tag;
  bool on = false;
  // UNE CASE EN ATTENTE : aucun message ne l'alimente encore. Elle se coche
  // comme les autres, mais son libellé se grise et `waiting_tip` dit pourquoi
  // — un joueur qui la coche et ne voit rien venir ne doit pas accuser son
  // filtre.
  bool waiting = false;
  std::string_view waiting_tip;
};

// L'apparence que le joueur a réglée. Elle vit dans les réglages, donc chez
// `app` : `ui` la reçoit et n'en garde rien.
//
// ⚠ L'ÉCHELLE DE L'HABILLAGE N'EST PAS ICI : c'est l'échelle générale de
// l'interface qui la tient, et la dédoubler donnerait deux commandes pour une
// seule question. Reste donc le réglage que l'échelle générale ne rend PAS — la
// taille du texte du fil seul.
struct ChatAppearance {
  int font_percent = 100;
  // Le fond du cadre. ⚠ UN RÉGLAGE COMME LES AUTRES depuis qu'un onglet peut
  // avoir le sien : le laisser dans le style du cadre obligerait à deux
  // chemins pour la même couleur.
  ImU32 body = kChatBodyColor;
  // Le filet du cadre, et le gris des languettes. ⚠ GÉNÉRAUX, tous les deux :
  // un onglet peut avoir SON fond parce qu'il décrit ce qu'on lit dedans, mais
  // la bande et le cadre appartiennent à la FENÊTRE, qui en porte plusieurs.
  ImU32 border = kChatBorderColor;
  ImU32 tabs = kChatTabColor;
  int padding_pixels = static_cast<int>(kChatPadding);
  int line_gap_pixels = static_cast<int>(kChatLineGap);
  bool timestamps = false;
  // Le champ de saisie grandit-il quand la phrase est longue ?
  //
  // ⚠ ALLUMÉ PAR DÉFAUT, et c'est l'inverse du client : un champ d'une ligne
  // qui défile cache le DÉBUT de ce qu'on écrit, alors que la longueur est
  // bornée et que le dépassement est refusé par le serveur sans un mot.
  bool grow_input = true;
  // ── LE BATTLE MODE ──────────────────────────────────────────────
  // La barre de saisie se replie, et le clavier revient au jeu. C'est le mode
  // du client d'origine — `/bm`, `/battlemode` — et ses règles vivent dans
  // `chat_bar.h`, en fonctions pures : la fenêtre ne fait que les appliquer.
  //
  // ⚠ DÉCOCHÉ PAR DÉFAUT, comme dans le client : une barre qu'on ne voit pas
  // est un service pour qui se bat, et une énigme pour qui découvre le jeu.
  bool battle_mode = false;
  // Fige la position et la taille. Les languettes, les menus et la saisie
  // continuent de fonctionner — c'est la GÉOMÉTRIE qui est verrouillée.
  //
  // 🔴 IL NE SE RÈGLE PAS ICI, IL S'Y LIT. Il y a un verrou PAR
  // FENÊTRE, et `app` remplit celui de la fenêtre qu'il fait dessiner ; un
  // panneau de réglages ne sait pas de laquelle on parle, et sa case unique
  // les figeait les trois d'un coup — y compris la flottante qu'on venait
  // d'arracher. La case vit dans le menu d'un onglet, qui s'ouvre toujours
  // depuis une fenêtre précise.
  bool locked = false;
  // ── CE QUI SURVIT À LA PARTIE ───────────────────────────────────
  // 🔴 DÉCOCHÉ PAR DÉFAUT, et ce n'est pas de la timidité : le fichier porte
  // les CHUCHOTEMENTS EN CLAIR. Garder l'historique est un service ; écrire des
  // conversations privées sur le disque sans l'avoir demandé n'en est pas un, et
  // personne ne doit le découvrir après coup.
  bool keep_history = false;
  int kept_lines = 200;
  // ── CE QU'ON A TAPÉ, ET QU'ON RETROUVE À LA FLÈCHE ↑ ────────────
  // 🔴 COCHÉ PAR DÉFAUT, À REBOURS DES DEUX AUTRES, et c'est un choix qu'il
  // faut savoir : ce qu'on retape le plus sont les `@commandes`, et un service
  // qu'il faut d'abord aller cocher n'existe pas pour qui ignore qu'il existe.
  //
  // ⚠ LE PRIX EST DIT, et il est le même que pour l'historique : une ligne
  // tapée est une ligne tapée, `/w Alice …` comprise. Décocher efface le
  // fichier, comme au-dessus.
  bool keep_typed = true;
  int kept_typed = kChatHistoryKept;
  // ⚠ CE QUE LA CONVERSATION GARDE EN MÉMOIRE N'EST PAS ICI, et l'éditeur de
  // liens l'a tranché : `ui` ne connaît pas `sim`. C'est juste — une borne
  // d'éviction n'est pas une apparence, elle décrit un TAMPON. Elle vit donc
  // dans `app`, qui la pousse au monde.
  // ── LE JOURNAL SUR LE DISQUE ────────────────────────────────────
  // ⚠ CE N'EST PAS LA MÊME CHOSE QUE L'HISTORIQUE, et les deux se cochent
  // séparément. L'historique se relit DANS la fenêtre au démarrage suivant ; le
  // journal est un fichier texte que le joueur ouvre AILLEURS, rangé par jour,
  // et qui survit à une sortie brutale parce qu'il s'écrit ligne à ligne.
  //
  // 🔴 Décoché par défaut pour la même raison : il porte les chuchotements en
  // clair.
  bool keep_log = false;
  // ── CHERCHER POURQUOI UNE LIGNE NE PARAÎT PAS ───────────────
  // Tous les filtres levés, et le type de chaque ligne écrit en tête.
  //
  // ⚠ IL RÉPOND À UNE QUESTION QUI N'A QUE DEUX RÉPONSES : une ligne visible
  // ici mais absente d'un onglet a été écartée par un filtre ; une ligne
  // absente MÊME ici n'est jamais arrivée jusqu'à nous, et c'est alors le
  // réseau qu'il faut regarder, pas la fenêtre.
  bool diagnostic = false;
  // DE QUOI PEINDRE LE TEXTE RICHE DU FIL. ⚠ PAS UN RÉGLAGE : il se pose à
  // chaque image, comme `locked`. Nul : le texte nu.
  const ChatRichArt* rich = nullptr;
  // LE `@warp` DU STAFF au menu d'un lieu. ⚠ PAS UN RÉGLAGE NON PLUS : ce sont
  // les droits du compte, posés à chaque image.
  bool staff_warp = false;
};

// L'APPARENCE PROPRE À UN ONGLET.
//
// 🔴 UN DRAPEAU, ET NON UNE VALEUR SENTINELLE PAR CHAMP. Le joueur veut
// « comme les autres » ou « à moi », et dans le premier cas ses onglets
// doivent SUIVRE un changement du réglage général — pas en garder une copie
// figée le jour où ils l'ont recopié. Une sentinelle par champ obligerait à
// répondre à la question quatre fois, et laisserait des onglets à moitié
// propres dont personne ne saurait dire ce qu'ils suivent.
//
// ⚠ C'EST CELLE DE L'ONGLET ACTIF QUI VAUT pour toute sa fenêtre : les marges
// et le fond décrivent un cadre, pas un fil. Changer d'onglet change donc
// l'allure de la fenêtre, et c'est voulu — c'est ce que le joueur a demandé en
// donnant une apparence à CET onglet-là.
struct ChatTabLook {
  bool own = false;
  int font_percent = 100;
  int padding_pixels = static_cast<int>(kChatPadding);
  int line_gap_pixels = static_cast<int>(kChatLineGap);
  // Le fond du cadre. ⚠ LU SEULEMENT QUAND `own` EST VRAI : zéro est une
  // couleur valide — le noir transparent — et ne peut donc pas dire « rien ».
  ImU32 body = kChatBodyColor;
};

// Ce que la fenêtre garde d'une image à l'autre. Comme le reste, il vit chez
// l'appelant : une fenêtre en mode immédiat ne retient rien d'elle-même.
struct ChatWindowState {
  bool open = true;
  // ── CE QUI IDENTIFIE CETTE FENÊTRE-CI ───────────────────────
  // Le groupe qu'elle montre — zéro pour la principale — et le nom ImGui qui
  // en découle. ⚠ POSÉS À LA CRÉATION ET JAMAIS APRÈS : le nom porte la
  // géométrie, et la changer en cours de route perdrait la place que le joueur
  // avait donnée.
  std::uint32_t group = 0;
  WindowName name{kChatWindowTitle, kChatWindowId};
  // ⚠ SEULE LA PRINCIPALE S'EN SERT, comme dans le client d'origine : une
  // fenêtre détachée n'a pas de saisie, elle renvoie le clavier à celle-ci.
  // Une barre par flottante donnerait aussi un historique de frappe par
  // fenêtre, que personne ne demande.
  ChatInputState input;
  // LA BARRE EST PRÊTÉE À LA SALLE D'UN SALON, posé à chaque image par qui le
  // sait. Elle s'y dessine, et la conversation montre à sa place le mot qui y
  // ramène : sans lui, le joueur la croirait disparue.
  bool input_lent = false;
  // ── CE QUE LA FENÊTRE A GAGNÉ EN HAUTEUR pour loger une phrase longue ─────
  // `grown_pixels` est le cumul déjà accordé, `grow_step` ce qu'il faut ajouter
  // (ou rendre) à la prochaine image.
  //
  // ⚠ UN PAS APPLIQUÉ UNE FOIS, PAS UNE GÉOMÉTRIE IMPOSÉE À CHAQUE IMAGE : la
  // fenêtre reste déplaçable et redimensionnable pendant qu'on écrit, et un
  // redimensionnement fait à l'état grandi ne casse rien — on rendra exactement
  // ce qu'on a pris.
  float grown_pixels = 0.0F;
  float grow_step = 0.0F;
  // L'onglet montré. ⚠ BORNÉ À CHAQUE IMAGE par la fenêtre : un onglet retiré
  // laisserait sinon un rang qui désigne le vide.
  int tab = 0;
  // L'onglet dont on règle les genres, ou moins de zéro. Le réglage est un ÉTAT
  // et non un appel : on ouvre, on coche, on referme, et cela dure plusieurs
  // images.
  int configuring = -1;
  // Le geste en cours dans la bande.
  ChatStripState strip;
  // La rangée de recherche est-elle dépliée, et le fil est-il en texte nu ?
  bool searching = false;
  bool selecting = false;
  std::array<char, kChatSearchSize> search = {};
  // ── DEUX TAMPONS DE TRAVAIL, refaits à chaque image ────────────────────
  // Ils vivent ici parce que tout ce que la fenêtre garde vit chez l'appelant —
  // ce ne sont pas un état, mais ils ont besoin d'une adresse stable : le champ
  // de sélection lit son texte pendant qu'ImGui le dessine.
  std::vector<ChatLineView> shown;
  std::string selected;
  // ⚠ MESURÉES À L'IMAGE PRÉCÉDENTE, et c'est le seul moyen : la quantification
  // de la hauteur doit être posée AVANT `Begin`, alors que le pas d'une ligne et
  // la place que prend tout le reste ne se connaissent qu'après le dessin.
  float row_pixels = 0.0F;
  float chrome_pixels = 0.0F;
  std::array<char, kChatTabNameSize> tab_name = {};
};

// Ce que le joueur a demandé. ⚠ TOUT EST FACULTATIF : une image ordinaire ne
// demande rien du tout.
struct ChatWindowAction {
  // La ligne validée, déjà vidée de la saisie.
  std::optional<std::string> said;
  // CE QU'IL VIENT DE FAIRE D'UN LIEN — dans le fil ou sur une pastille de la
  // saisie, c'est la même convention et le même menu.
  ChatLinkAsk link;
  // Il veut un onglet de plus.
  bool add_tab = false;
  // Le rang de celui qu'il veut retirer, ou moins de zéro.
  int remove_tab = -1;
  // Le rang de celui qu'il vient de renommer, et son nom neuf.
  int renamed_tab = -1;
  std::string new_name;
  // Il veut vider cet onglet, ou lui rendre ce qu'il masque.
  int clear_tab = -1;
  int unclear_tab = -1;
  // Une languette déplacée : de ce rang-là, vers celui-ci.
  int moved_from = -1;
  int moved_to = -1;
  // Le rang, DANS LA LISTE DES TYPES REÇUE, de celui qu'il vient de basculer
  // sur l'onglet en cours de réglage. Moins de zéro s'il n'a rien coché.
  int toggled_type = -1;
  // Il a demandé à oublier les destinataires récents.
  bool forget_whispers = false;
  // ÉCHAP VIENT DE REPLIER LA BARRE DE SAISIE (battle mode). ⚠ L'APPELANT DOIT
  // ALORS AVALER LA TOUCHE : sans cela, une seule frappe refermerait la barre
  // ET la fenêtre derrière elle — deux effets pour un geste.
  bool folded = false;
  // TOUS LES TYPES D'UN COUP, quand il a demandé le bloc. Zéro n'est pas
  // « rien » ici — c'est « tout décocher » — d'où l'optionnel plutôt qu'un
  // entier à trois valeurs dont la troisième se retient mal.
  std::optional<bool> set_every_type;
  // LE VERROU DE GÉOMÉTRIE DE CETTE FENÊTRE-CI, quand il vient d'être basculé.
  //
  // 🔴 LA FENÊTRE, PAS L'ONGLET. Le menu s'ouvre toujours depuis une
  // fenêtre précise, et c'est le seul endroit de l'interface qui sache
  // laquelle : un panneau de réglages figerait les trois d'un coup.
  std::optional<bool> set_locked;
  // ── LES FENÊTRES ──────────────────────────────────────
  // L'onglet qu'il veut sortir dans une fenêtre à lui, ou rendre à la
  // principale. ⚠ LES RANGS SONT CEUX DE LA FENÊTRE QUI LES REND : chacune ne
  // connaît que ses propres onglets, et c'est à l'appelant de traduire.
  int detach_tab = -1;
  int attach_tab = -1;
  // Le rang du mode d'envoi qu'il vient d'armer, ou moins de zéro. ⚠ SEULE LA
  // PRINCIPALE EN REND UN : elle seule a une saisie.
  int chose_mode = -1;
  // L'apparence qu'il vient de donner à l'onglet qu'il règle. ⚠ RENDUE EN
  // ENTIER, pas champ par champ : quatre valeurs et un drapeau se lisent d'un
  // coup, et distinguer celui qui a bougé n'apprendrait rien à l'appelant.
  bool tuned_look = false;
  ChatTabLook look;
  // L'émoticone sans nom de chat qu'il vient de choisir dans la grille, à
  // envoyer en geste du jeu, ou moins de zéro : voir
  // `ChatStripAction::chose_emotion`.
  int chose_emotion = -1;
  // La ligne `:nom:` d'une émoticone cliquée, à envoyer telle quelle par le
  // mode d'envoi armé. Vide sinon : voir `ChatStripAction::sent_emote_token`.
  std::string sent_emote;
  // L'emoji pioché, à insérer dans la saisie.
  std::string_view chose_emoji;
  // La palette s'est refermée après une pioche : le clavier revient au champ.
  bool give_back_keyboard = false;
  // ── LES RÉGLAGES GÉNÉRAUX DE LA CONVERSATION ──────────
  //
  // 🔴 CE QUE LA PUCE D'UNE BARRE DE TITRE FAIT AILLEURS, et la chatbox ne peut
  // pas l'avoir : son cadre n'est pas une fenêtre habillée — pas de barre de
  // titre, donc pas de puce. L'entrée du menu contextuel en tient lieu.
  //
  // ⚠ CE N'EST PAS UN RÉGLAGE DE PLUS mais une PORTE : ce champ n'emporte
  // aucune valeur, il demande l'ouverture du panneau Options sur la section qui
  // parle de la conversation.
  bool settings_asked = false;
  // Le joueur a cliqué le mot qui remplace la barre prêtée : la salle du
  // salon doit passer devant.
  bool lent_input_asked = false;
  // CE QUE LE COMPTEUR DE DÉGÂTS A DEMANDÉ, quand il est rangé dans cette
  // fenêtre et que son onglet est montré.
  DamageMeterAction meter;
  // Ctrl + molette sur le fil : un cran de taille du texte, plus ou moins.
  int zoom_steps = 0;
  // CE QU'UNE FLOTTANTE A PIOCHÉ DANS SA PALETTE — un emoji, un `:nom:` —, à
  // poser dans la saisie de la PRINCIPALE par `AppendPickedToChatInput` : une
  // flottante n'a pas de saisie à elle. Vide sinon.
  std::string insert_into_main;
};

// TOUTES LES FENÊTRES DE CONVERSATION : la principale, et les flottantes.
//
// ⚠ LA PRINCIPALE EST À PART, et pas une entrée de plus dans la table : elle
// seule porte la saisie, elle ne se ferme pas parce qu'on lui a pris son
// dernier onglet, et tout ce qui vise « la conversation » vise elle.
//
// ⚠ UNE TABLE ORDONNÉE, ET C'EST NÉCESSAIRE : les états sont pris par RÉFÉRENCE
// pendant le dessin, et un vecteur qui grandit les déplacerait sous la main de
// qui les tient.
struct ChatWindows {
  ChatWindowState main;
  std::map<std::uint32_t, ChatWindowState> floating;
  // Le plateau du glisser, partagé par toutes les bandes d'une image. Sa place
  // est ici parce qu'un geste traverse les fenêtres : il part de l'une et tombe
  // dans l'autre.
  ChatDragBoard board;
  // Ce que le menu d'un item sait d'avance, posé à chaque image et rendu au fil de chaque bande.
  ItemLinkFacts link_facts;
  // ── LES LIENS ET LES IMAGES DU FIL ────────────────────────────
  // De quoi peindre le texte riche, posé à chaque image et prêté au fil de
  // chaque fenêtre par `ChatAppearance::rich`.
  ChatRichArt rich;
  // Les deux confirmations — ouvrir une adresse, admettre un site — et la
  // visionneuse : une de chaque pour toutes les fenêtres, comme la souris.
  ChatUrlPrompt url_prompt;
  ChatHostPrompt host_prompt;
  ChatPictureViewerState viewer;
  ChatPictureViewerView viewer_view;
  // LE CHEMIN DU LIEN DE RÉGLAGE SURVOLÉ, composé par `app` ; rien quand aucun
  // ne l'est. ⚠ IL NE VAUT QUE POUR UNE IMAGE : voir `DrawSettingLinkTip`.
  std::optional<std::string> setting_tip;

  // L'état de ce groupe, créé s'il manque — avec son nom, qui ne bougera plus.
  [[nodiscard]] ChatWindowState& For(std::uint32_t group);

  // Oublie les fenêtres dont plus aucun onglet ne relève.
  //
  // 🔴 SANS CELA C'EST UNE FUITE SILENCIEUSE : chaque détachement laisse un
  // état derrière lui, avec son tampon de recherche et sa liste de lignes, et
  // rien à l'écran ne le signale.
  void Keep(std::span<const std::uint32_t> living);
};

// CE QUE LE MENU D'UN ONGLET AUTORISE, et pourquoi.
//
// 🔴 LES TROIS RÈGLES SE TIENNENT, et l'une sans l'autre ouvre un trou :
// c'est parce que la fenêtre PRINCIPALE garde toujours un onglet qu'une
// flottante peut fermer le sien sans que la conversation disparaîsse.
//
// ⚠ LE PLAFOND COMPTE TOUTES LES FENÊTRES : c'est le chargeur du client qui
// refuse au-delà, et un onglet de trop ne planterait rien — il disparaîtrait
// à la reconnexion suivante, sans un mot, avec ce qu'on avait mis dedans.
// Compter par bande en laisserait créer trois fois trop.
struct ChatTabPowers {
  bool can_add = true;
  bool can_close = true;
  bool can_detach = true;
};
[[nodiscard]] ChatTabPowers ChatTabMenuPowers(std::uint32_t group, int tabs_here,
                                              int tabs_everywhere);

// Dessine la conversation. `lines` est déjà triée pour l'onglet courant, et
// `types` décrit les filtres de l'onglet en cours de réglage — vide sinon.
//
// `log_layout` est le cache de ce fil, tenu par qui en distribue les identifiants
// — voir `ChatLogLayout`. Nul, le fil se remet en page entier à chaque image.
[[nodiscard]] ChatWindowAction DrawChatWindow(
    ChatWindowState& state, const ChatAppearance& look, const ChatLinePreview& preview,
    std::span<const ChatLineView> lines, ChatLogLayout* log_layout,
    std::span<const ChatStripTab> tabs, int tabs_everywhere, std::span<const ChatTypeToggle> types,
    const ChatTabLook& tuned, std::span<const ChatEmotion> emotions,
    std::span<const ChatEmojiGroup> emojis, ChatDragBoard& board, const SkinArt& art,
    // LES LIENS DEJA POSES DANS LA SAISIE, dans l'ordre de pose. Vide pour une
    // fenetre detachee : elle n'a pas de saisie.
    std::span<const ChatInputLink> input_links = {},
    // Ce que le menu d'un item du fil et des pastilles sait d'avance.
    ItemLinkFacts link_facts = {},
    // LE COMPTEUR DE DÉGÂTS, peint à la place du fil quand l'onglet montré est
    // le sien — voir `ChatStripTab::meter`. Nul s'il n'est rangé nulle part.
    const DamageMeterView* meter = nullptr);

}  // namespace ro::ui
