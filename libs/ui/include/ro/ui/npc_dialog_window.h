#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ro/core/i18n.h"
#include "ro/ui/chat_link.h"
#include "ro/ui/npc_dialog_text.h"
#include "ro/ui/skin_art.h"

namespace ro::ui {

// LE DIALOGUE D'UN PNJ : ce qu'il dit, et les boutons qu'il tend.
//
// C'est la fenêtre par laquelle passe tout le jeu qui n'est pas du combat — les
// quêtes, les boutiques, les téléportations —, et c'est celle à laquelle le
// joueur DOIT répondre. Elle ne touche à rien : elle rend ce qu'il a demandé, et
// `app` décide quoi en faire. Une fenêtre qui ne peut pas muter l'état ne peut
// pas devenir un panneau de trois mille lignes qui tripote l'état d'autrui.
//
// ⚠ ELLE NE CONNAÎT PAS `sim`. Le DAG l'interdit — `ui ← core, render` —, donc
// ce qu'elle montre traverse en données pures. Ce qui s'y gagne est concret : un
// dialogue se fabrique à la main dans un essai, sans serveur ni PNJ.

inline constexpr const char* kNpcDialogWindowTitle = core::TrText("Dialogue");
inline constexpr const char* kNpcDialogWindowId = "npc_dialog";

// Ce que le PNJ attend. Le miroir de `sim::NpcPrompt`, recopié par `app` :
// deux couches qui ne se connaissent pas ne partagent pas un type.
enum class NpcDialogPrompt : std::uint8_t { kNone, kNext, kClose, kMenu, kNumber, kText };

// LE NOMBRE D'OPTIONS AU-DELÀ DUQUEL UN CHAMP DE RECHERCHE APPARAÎT.
//
// ⚠ IL COMPTE LES OPTIONS AFFICHÉES, pas la place qu'elles prennent : un menu de
// téléportation en aligne trente, et les parcourir à l'œil coûte plus que de
// taper trois lettres.
inline constexpr std::size_t kNpcMenuSearchThreshold = 8;

// Au-delà, la liste défile au lieu de grandir : dix options tiennent sans
// écraser ce que le PNJ a dit.
inline constexpr std::size_t kNpcMenuMaxRows = 10;

// Ce que la fenêtre montre, recopié par `app`.
struct NpcDialogView {
  // Le nom du PNJ, pour la barre de titre. Vide tant qu'il n'est pas connu.
  std::string_view name;
  // La page AFFICHÉE, balisage compris.
  std::span<const std::string> lines;
  // Les libellés du menu, dans l'ordre d'affichage. ⚠ LEUR RANG N'EST PAS ICI :
  // la fenêtre rend une POSITION, et la traduction en rang appartient à
  // `ro/net/npc_dialog.h`, seul endroit qui sache ce qu'un mauvais rang coûte.
  std::span<const std::string_view> options;
  NpcDialogPrompt prompt = NpcDialogPrompt::kNone;
  // 🔴 UNE RÉPONSE EST PARTIE, celle du serveur n'est pas là. La page reste
  // entière — bouton, liste, champ — mais INERTE : rien ne saute de place, et
  // le joueur ne peut pas répondre deux fois.
  bool awaiting_reply = false;
  // La génération de la page. Elle change quand le texte change, et c'est ce
  // qui décide de remettre en page et de relire par le HAUT.
  std::uint32_t page = 0;
  // La génération du MENU, qui change à chaque liste reçue.
  //
  // 🔴 LA PAGE NE SUFFIT PAS : un script qui bascule un réglage renvoie le même
  // menu réécrit (« ON » devient « OFF ») sans une ligne de texte — même page,
  // même nombre d'options, et c'est ce numéro seul qui dit que la liste a changé.
  std::uint32_t menu = 0;
  // Les octets utiles d'une saisie de texte : au-delà, le serveur jette.
  std::size_t text_limit = 0;
  // Le champ de recherche des longs menus, que le joueur peut couper.
  bool menu_search = true;
  // Ce que le menu d'un item sait d'avance : vote shop, album.
  ItemLinkFacts link_facts;
};

// Ce que la fenêtre garde d'une image à l'autre.
struct NpcDialogWindowState {
  // LE BOUTON « SIGNALER UN BUG » de sa barre de titre, posé par `app` depuis
  // les réglages.
  bool bug_report = false;

  // LA MISE EN PAGE, ET CE POUR QUOI ELLE VAUT.
  //
  // 🔴 CALCULÉE UNE FOIS PAR PAGE ET PAR LARGEUR, jamais par image. Le repli —
  // lecture des balises, mesure de chaque mot — coûte proportionnellement au
  // nombre de lignes : une page de deux cents lignes payait son propre découpage
  // soixante fois par seconde PENDANT qu'elle arrivait, ce qui la faisait
  // apparaître plus lentement que chez le client officiel, dont chaque ligne
  // n'est placée qu'une fois.
  DialogLayout laid;
  std::uint32_t laid_page = 0;
  bool laid_once = false;

  // La dernière page dont on a remonté le défilement. ⚠ UNE PAGE NEUVE SE LIT
  // PAR LE HAUT : déposer le joueur à la FIN d'une liste de monstres le fait
  // remonter pour comprendre ce qu'il lit.
  std::uint32_t scrolled_page = 0;

  std::array<char, 64> menu_filter = {};
  // L'option que le clavier vise, comptée parmi les VISIBLES. Moins un : la
  // première, d'office.
  int menu_hot = -1;
  // 🔴 CE QUE « OK » ENVERRA : la POSITION, dans `options`, de l'option visée.
  // Moins un : aucune.
  //
  // ⚠ ELLE NE SE DÉDUIT PAS DE `menu_hot`, et c'est tout l'intérêt : sous
  // filtre, le rang d'une ligne à l'écran n'a plus rien à voir avec sa place
  // dans la liste. Le pied de fenêtre est dessiné APRÈS la liste, seule à
  // connaître la correspondance.
  int menu_aimed = -1;
  // La page dont vient la liste affichée : en changer vide le filtre et
  // ramène la visée sur la première option. Sans cela, un filtre tapé pour un
  // menu masquerait les options du suivant.
  std::uint32_t menu_page = 0;
  std::uint32_t menu_list = 0;
  // LE MENU LU UNE FOIS PAR LISTE : les morceaux de chaque option, et son
  // libellé en minuscules pour le filtre. Les relire à chaque image coûtait
  // des centaines d'allocations pour un menu de deux cents options.
  std::vector<std::vector<DialogRun>> menu_runs;
  std::vector<std::string> menu_lowered;
  std::uint32_t menu_read_page = 0;
  std::uint32_t menu_read_list = 0;
  bool menu_read = false;

  std::array<char, 16> number = {};
  std::array<char, 128> text = {};
  // La première image d'un champ : c'est là qu'on lui donne le clavier.
  bool focus_input = false;
  // Ce que le PNJ demandait à l'image précédente : c'est le CHANGEMENT qui vide
  // le champ et lui donne le clavier, pas sa présence.
  NpcDialogPrompt shown_prompt = NpcDialogPrompt::kNone;

  // LE TITRE COMPOSÉ, et de quoi savoir s'il est encore bon. ⚠ RECOMPOSÉ
  // SEULEMENT QUAND IL LE FAUT : c'est un nom de PNJ, pas un littéral du
  // catalogue, et le refaire à chaque image coûterait une allocation par image.
  std::string title;
  std::string title_for;
  std::size_t title_language = 0;

  // LA CIBLE SUR LAQUELLE UN MENU VIENT DE S'OUVRIR, par son rang dans la mise
  // en page. Moins un : aucun.
  //
  // ⚠ ELLE SE MET DE CÔTÉ AU CLIC DROIT, et le menu s'ouvre plus bas : le nom
  // d'un popup se hache avec la pile d'identifiants de SA fenêtre, et le clic
  // s'est produit dans la zone défilante du texte.
  int menu_target = -1;
  // Celle dont le menu est OUVERT. Elle survit à l'image de l'ouverture : un
  // popup reste tant qu'on n'a pas cliqué ailleurs.
  int menu_shown = -1;

  // L'INTERRUPTEUR QUE LA PILE D'ÉCHAP BAISSE.
  //
  // 🔴 LA FENÊTRE NE S'OUVRE PAS ELLE-MÊME — elle suit le monde —, mais Échap a
  // besoin de quelque chose à baisser. Ce drapeau est relevé à chaque image ; le
  // voir bas veut dire que la pile vient de le baisser, et c'est une FERMETURE.
  //
  // ⚠ L'ÉTAGE NE L'INSCRIT QUE QUAND LA FERMETURE A UN SENS pour le serveur :
  // pendant un « suivant », Échap doit filer au jeu plutôt que d'être avalé sans
  // rien faire.
  bool open = true;

  // La fenêtre se replace à l'ouverture d'une conversation.
  bool place = true;
};

// Ce que le joueur a demandé au PNJ.
enum class NpcDialogIntent : std::uint8_t { kNothing, kNext, kClose, kChoose, kNumber, kText };

struct NpcDialogAnswer {
  NpcDialogIntent intent = NpcDialogIntent::kNothing;
  // « Signaler un bug » : `app` compose le contexte du PNJ qui parle.
  bool report_bug = false;
  // Le rang choisi dans `options`, à PARTIR DE ZÉRO — une position d'affichage,
  // pas le rang du protocole.
  std::size_t option_index = 0;
  std::int32_t number = 0;
  // ⚠ UNE CHAÎNE PAR VALEUR, PAS UNE VUE : ce qui l'a produite meurt à la fin de
  // l'image, et l'appelant lit cette réponse APRÈS.
  std::string text;

  // LE LIEN DU CLIENT qu'il vient de suivre. Indépendant de ce qui précède : on
  // clique un lien sans répondre au PNJ.
  //
  // 🔴 `ui` NE L'OUVRE PAS. Suivre une adresse lance un programme du système ;
  // c'est un geste vers le DEHORS, déclenché par une donnée du serveur, et il se
  // décide chez `app`.
  DialogLink followed = DialogLink::kNone;
  std::string followed_payload;

  // LA BALISE DE LA MAISON qu'il a visée, et le geste qui l'a fait. Les gestes
  // sont ceux de la chatbox, parce que c'est la même convention partout.
  DialogTarget target;
  ChatLinkGesture gesture = ChatLinkGesture::kNothing;

  // CE QUE LA SOURIS SURVOLE, pour l'aperçu.
  //
  // ⚠ SÉPARÉ DU GESTE, et c'est la même règle que dans la conversation : on
  // survole bien plus souvent qu'on ne clique, et ce qu'un lien MONTRE ne doit
  // pas attendre qu'on appuie. `app` compose l'aperçu à l'image suivante — un
  // retard imperceptible, puisqu'une bulle n'apparaît pas avant que la souris se
  // soit arrêtée.
  DialogTarget hovered;
};

// De quoi peindre ce que le texte porte, fourni par `app` : `ui` ne sait ni
// charger une icône, ni animer un sprite, ni ouvrir une image du jeu.
struct NpcDialogArt {
  DialogIconAspect icon_aspect = nullptr;
  void* icon_owner = nullptr;
  // L'image d'une icône d'objet, au moment de peindre.
  SkinImage (*icon_image)(std::uint32_t item_id, void* owner) = nullptr;
  DialogMediaMeasure media_measure = nullptr;
  void* media_owner = nullptr;
  // Le média d'un fragment, peint dans le rectangle que la mise en page lui a
  // donné. Rend faux s'il n'a rien pu montrer : la fenêtre le DIT alors, plutôt
  // que de laisser un trou — ces balises sont écrites par le staff, et une
  // erreur muette se cherche une heure.
  bool (*draw_media)(const DialogFragment& fragment, std::string_view source, ImVec2 top_left,
                     ImVec2 bottom_right, void* owner) = nullptr;
  // Le portrait de page, dans sa colonne. Faux : aucune colonne n'est réservée.
  bool (*draw_portrait)(std::uint32_t monster_id, ImVec2 top_left, ImVec2 bottom_right,
                        void* owner) = nullptr;

  // CE QUE LE LECTEUR SAIT NOMMER : un réglage, un état. Voir
  // `DialogVocabulary`, et pourquoi ces deux-là ne portent pas leur nom.
  DialogVocabulary words;
};

// Le nombre qu'un joueur a tapé dans le champ d'un PNJ, borné à l'entier 32
// bits que le paquet porte. Vide ou illisible : zéro.
[[nodiscard]] std::int32_t ParsedNumber(std::string_view typed);

// Dessine le dialogue et rend ce que le joueur en a fait.
//
// La croix de la fenêtre vaut « fermer » : le serveur attend une réponse, et une
// fenêtre qui disparaîtrait sans rien dire laisserait le script suspendu.
[[nodiscard]] NpcDialogAnswer DrawNpcDialogWindow(NpcDialogWindowState& state,
                                                  const NpcDialogView& view, const SkinArt& art,
                                                  const NpcDialogArt& media);

}  // namespace ro::ui
