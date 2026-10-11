#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>

#include "ro/core/i18n.h"
#include "ro/ui/skin_art.h"

namespace ro::ui {

// LES OPTIONS DU JEU.
//
// ⚠ ELLE NE CONNAÎT AUCUN RÉGLAGE. Une ligne lui arrive décrite — un libellé, une
// forme de contrôle, une valeur — et elle rend L'IDENTIFIANT de celle qui a
// bougé, avec sa valeur neuve. C'est la boucle qui sait ce que « le volume de la
// musique » veut dire et qui le pose dans le moteur.
//
// C'est la même frontière que pour les raccourcis, et elle achète la même chose :
// la fenêtre se fabrique et se vérifie dans un essai, sans moteur audio, sans
// carte et sans serveur.
//
// ⚠ L'IDENTIFIANT VIENT DE L'APPELANT, ce n'est pas un rang. Les lignes sont
// réparties en sections, et un rang global serait à recalculer à chaque section
// insérée — le genre de numéro qui finit par désigner le réglage d'à côté.

// Ce qu'une ligne montre. Une forme s'ajoute quand un réglage la demande, pas
// avant.
enum class OptionKind : std::uint8_t {
  kToggle,  // une case à cocher
  kSlider,  // un curseur, entre deux bornes
  kChoice,  // une liste déroulante
  kColor,   // un nuancier, transparence comprise
  // UN BOUTON QUI FAIT QUELQUE CHOSE, plutôt qu'un réglage qui décrit un
  // état. ⚠ SON LIBELLÉ EST SUR LE BOUTON, pas à côté : un mot posé à la
  // droite d'un bouton se lit comme la légende de ce qu'il vient de faire.
  kAction,
  // UNE LIGNE QUI SE CONTENTE DE DIRE QUELQUE CHOSE. Pas de contrôle, rien à
  // rendre.
  //
  // ⚠ ELLE PORTE CE QU'UN RÉGLAGE NE PEUT PAS DIRE DE LUI-MÊME : la CONSÉQUENCE
  // chiffrée de plusieurs curseurs à la fois. « Largeur max : 640 » et « Durée :
  // 6 » ne disent ni combien d'images cela fait, ni ce que cela pèsera ; et
  // l'infobulle de l'un des deux ne peut pas le dire non plus, puisque la
  // réponse dépend de l'autre.
  kNote,
  // UN CADRE OÙ L'APPELANT PEINT CE QUE LE RÉGLAGE FAIT, à la largeur de la
  // page — une entité et ses barres, posées comme en jeu. Rien à rendre : on
  // regarde, on ne clique pas.
  //
  // 🔴 UN APERÇU DANS LA PAGE, ET NON AU MILIEU DE L'ÉCRAN : on règle une
  // couleur en la regardant, sans chercher des yeux ce qu'elle change.
  kCanvas,
  // UN CHAMP OÙ L'ON TAPE UNE LIGNE — un nom de fichier. Rendu à chaque frappe,
  // comme un curseur à chaque cran : la fenêtre ne garde pas de brouillon.
  kText,
  // UN PANNEAU QUE L'APPELANT DESSINE LUI-MÊME, widgets compris : des réglages
  // qu'un autre écran dessine déjà, et qui s'APPELLENT ici plutôt que de se
  // recopier en rangées — deux listes divergent au premier ajout. Il rend
  // « changé », et la ligne le dit comme les autres.
  //
  // ⚠ CE QU'IL RÈGLE N'EST PAS À LA FENÊTRE : le panneau écrit lui-même dans
  // ce qu'il a reçu. La demande ne porte que l'identifiant de la ligne.
  kPanel,
};

// LA PLUS LONGUE LIGNE QU'UN CHAMP `kText` ACCEPTE, terminateur compris.
inline constexpr std::size_t kOptionTextBytes = 260;

// Ce que dessine un panneau, avec l'habillage de la fenêtre ; vrai quand un
// réglage a changé. Voir `kPanel`.
using OptionPanelDraw = std::function<bool(const SkinArt& art)>;

// Ce que peint un cadre : sa liste de dessin, et le rectangle qu'on lui donne.
using OptionCanvasPaint = std::function<void(ImDrawList& into, ImVec2 min, ImVec2 max)>;

// CE QU'UN CADRE RÉPOND quand on y tire une ancre : laquelle — un identifiant de
// l'appelant, rendu dans `OptionsWindowAction::changed` — et où elle est
// lâchée, dans les unités de l'appelant.
//
// UNE COPIE, et non un déplacement, quand `copy_to` n'est pas moins un : ce que
// porte `anchor` est copié vers cette cible de l'appelant, et `point` ne sert
// pas.
struct OptionCanvasMove {
  int anchor = -1;
  ImVec2 point{0.0F, 0.0F};
  int copy_to = -1;
};

// UN CADRE QU'ON MANIPULE : il peint, puis rend l'ancre qu'on a déplacée à cette
// image, ou rien.
using OptionCanvasEdit =
    std::function<std::optional<OptionCanvasMove>(ImDrawList& into, ImVec2 min, ImVec2 max)>;

struct OptionRowView {
  int id = -1;
  // ⚠ DÉJÀ TRADUIT par l'appelant, comme les libellés de touche de la
  // configuration. C'est lui qui tient les littéraux — donc le catalogue les
  // connaît —, et il refait ces lignes à chaque image de fenêtre ouverte : un
  // changement de langue se voit à l'image suivante, sans rien de figé.
  const char* label = "";
  // L'infobulle, ou nul. C'est là que va ce qu'un libellé ne peut pas dire.
  const char* help = nullptr;
  // CE QUI PEUT SURPRENDRE LE JOUEUR À SES DÉPENS, ou nul : un paragraphe que
  // l'infobulle pose en dernier, derrière un signe d'avertissement peint. ⚠ DÉJÀ
  // TRADUIT, comme l'aide, et SANS son signe : c'est la bulle qui le peint.
  const char* help_warning = nullptr;
  // La commande de chat qui fait la même chose, ou nul. Montrée en gris à côté
  // du libellé : un joueur qui la connaît la retrouve, un autre l'apprend.
  const char* command = nullptr;

  OptionKind kind = OptionKind::kToggle;
  bool on = false;
  float value = 0.0F;
  float lowest = 0.0F;
  float highest = 1.0F;
  const char* format = "%.2f";
  int chosen = 0;
  std::span<const char* const> choices;
  // Ce que porte un champ `kText`. Au-delà de `kOptionTextBytes`, coupé à une
  // frontière de caractère UTF-8.
  const char* text = "";
  // ⚠ TRANSPARENCE COMPRISE, et c'est souvent elle qu'on vient régler : une
  // grille à quinze pour cent se voit sans couvrir ce qu'elle sert à aligner.
  ImU32 color = 0;
  // ⛔ PAS DE REPORT AU RELÂCHEMENT pour un réglage qui coûte cher à poser :
  // la molette et les flèches éditent un curseur sans jamais le rendre actif
  // (voir `skin_slider.cpp`), donc aucun relâchement ne viendrait, et la
  // valeur ne serait jamais posée. Un réglage qui coûte cher se dit
  // par PALIERS, ce qui ne demande aucun report : voir
  // `kInterfaceScalePercents`.

  // LA QUESTION À POSER AVANT D'AGIR, ou nul. ⚠ ELLE VIT SUR LA RANGÉE parce
  // que c'est elle qui sait si son geste se rattrape : la fenêtre, qui ne voit
  // qu'un bouton, ne peut pas le deviner.
  const char* confirm = nullptr;

  // CE SUR QUOI UN BOUTON AGIT, écrit à sa droite, ou nul — puis sa précision,
  // en texte secondaire. ⚠ PAS DANS LE LIBELLÉ : le bouton dit le GESTE, la
  // rangée dit l'OBJET. C'est ce qui permet une liste où chaque entrée porte le
  // même « Débloquer » devant un nom différent.
  const char* object = nullptr;
  const char* object_note = nullptr;
  // LE PETIT BOUTON DU JEU plutôt que le grand : celui d'un geste répété
  // le long d'une liste.
  bool small_button = false;

  // LE RETOUR À LA RÉFÉRENCE : un petit bouton derrière le libellé, qui ramène
  // la rangée à ce que l'appelant tient pour sa valeur de référence — le thème
  // chargé, un défaut. Son libellé, ou nul quand la rangée n'en a pas, et ce
  // que dit sa bulle. Voir `OptionsWindowAction::revert`.
  //
  // ⚠ LES BOUTONS D'UN BLOC SE POSENT EN COLONNE, derrière l'aide : au fil de
  // chaque libellé, ils danseraient d'une rangée à l'autre.
  const char* revert = nullptr;
  const char* revert_help = nullptr;
  // ⚠ DÉJÀ À SA RÉFÉRENCE : le bouton se tait, mais la rangée compte encore dans
  // la mesure de la colonne — sans quoi elle sauterait à chaque réglage qui
  // s'écarte ou revient.
  bool at_reference = false;

  // ── CE QUI RANGE UNE SECTION LONGUE ───────────────────────────────────────
  //
  // 🔴 QUINZE RÉGLAGES EN COLONNE NE SE LISENT PAS. Une section qui décrit UN
  // objet sous plusieurs angles — sa place, ses icônes, son écoulement — se
  // parcourt en cherchant, et rien ne dit où finit un angle et où commence le
  // suivant. Ces trois champs sont ce qui découpe la section, et
  // c'est tout ce qui sépare une liste d'un formulaire.

  // Un titre tracé AU-DESSUS de cette rangée, ou nul. ⚠ DÉJÀ TRADUIT, comme le
  // libellé.
  const char* heading = nullptr;

  // UN BLOC REPLIABLE S'OUVRE SUR CETTE RANGÉE, sous ce titre, jusqu'au bloc
  // suivant ou la fin de la suite — ou nul. ⚠ DÉJÀ TRADUIT.
  //
  // 🔴 AU-DESSUS DE L'INTERTITRE : un bloc range des THÈMES — les barres, le
  // nom, le cast —, un intertitre les angles d'un thème. Deux niveaux qui se
  // dessinent pareil ne se distinguent plus, et c'est ce qui rendait la page des
  // entités illisible.
  //
  // ⚠ OUVERT OU FERMÉ SELON SON TITRE, PAS SELON L'ONGLET : fermer « Nom » sur
  // les monstres le ferme aussi sur les boss — on compare une espèce à l'autre
  // sur le thème qu'on règle.
  const char* fold = nullptr;

  // La rangée se décale : elle DÉPEND de celle du dessus. ⚠ L'INDENTATION EST
  // LA SEULE CHOSE QUI LE DISE — « Taille du texte » sous « Temps restant » n'a
  // aucun sens toute seule.
  bool indented = false;

  // La rangée se pose À LA SUITE de la précédente, sur la même ligne. ⚠ POUR
  // DES CASES SŒURS SEULEMENT — « Joueurs · Monstres · PNJ » — qui se lisent
  // comme un seul réglage en trois choix. Un curseur à la suite d'un autre
  // pousserait son libellé hors de la fenêtre.
  bool same_line = false;

  // LE CADRE : sa hauteur en pixels, et ce qu'on y peint. Voir `kCanvas`. Zéro :
  // toute la hauteur qui reste — dans un volet, voir `side_rows`.
  float canvas_height = 0.0F;
  OptionCanvasPaint paint;
  // OU CE QU'ON Y MANIPULE, à la place de `paint` : le cadre prend alors la
  // souris — un geste dans le cadre ne déplace plus la fenêtre.
  OptionCanvasEdit edit;
  // LE PANNEAU : voir `kPanel`.
  OptionPanelDraw panel;

  // Grisée, et sourde. ⚠ CE N'EST PAS UNE ABSENCE : un réglage qui disparaît
  // quand son maître s'éteint fait sauter la mise en page, et le joueur cherche
  // ce qu'il vient de perdre. Grisé, il dit « pas maintenant », et OÙ il est.
  //
  // 🔴 ET IL DIT « PAS MAINTENANT », JAMAIS « PAS ICI ». Un réglage qui ne
  // pourra RIEN faire sur l'objet de cet onglet, quoi qu'on règle, ne se grise
  // pas : il ne se compose pas du tout. Le griser promettrait à l'objet une
  // propriété qu'il n'a pas — et changer d'onglet est déjà changer de page,
  // donc rien ne saute sous la main. Voir `app/options/entity_name_fields.h`,
  // où la coupure entre les deux est écrite champ par champ.
  bool disabled = false;

  // UNE NOTE QUI MET EN GARDE : rouge, et non en texte secondaire. Réservée à
  // ce qui peut coûter au joueur — une arme détruite —, sinon le rouge cesse
  // d'alerter.
  bool warning = false;
};

// LES ONGLETS QU'UNE PAGE DESSINE ELLE-MÊME, entre le panneau et elle : ce que
// le panneau lui demande, et ce qu'elle lui rend. Un rang d'onglet est celui
// que la page donne à ses onglets ; le panneau ne sait pas ce qu'il désigne.
struct OptionBodyTabs {
  // L'onglet à ouvrir — un lien qu'on suit —, ou moins un. Une image, pas plus :
  // voir `OptionsWindowState::section_tab_wanted`.
  int wanted = -1;
  // Le Maj + clic d'un onglet pose son lien dans le chat.
  bool offers_links = false;
  // RENDU PAR LA PAGE : l'onglet dont on veut le lien, ou moins un.
  int link = -1;
};

// CE QU'UNE PAGE DESSINE ELLE-MÊME, sous ses rangées. Voir
// `OptionSectionView::body`.
using OptionSectionBody = std::function<void(const SkinArt& art, OptionBodyTabs& tabs)>;

// Une section : son titre, et ses lignes. Les sections rangent la fenêtre —
// sept réglages en vrac se cherchent déjà.
struct OptionSectionView {
  const char* title = "";
  // Le titre de la FAMILLE que cette section ouvre, ou nul. ⚠ PORTÉ PAR SON
  // PREMIER MEMBRE : voir `NavEntry`, qui dit pourquoi.
  const char* group = nullptr;
  // 🔴 UNE VUE, À PRENDRE APRÈS QUE LE VECTEUR EST REMPLI. Prise avant, elle
  // survit tant qu'il ne se réalloue pas — jusqu'au premier onglet qui compose
  // plus de lignes que le précédent —, puis elle pointe sur un tampon libéré.
  // C'est un plantage qui l'a dit, sur un intertitre lu à 0xDD.
  std::span<const OptionRowView> rows;

  // CE QUI VAUT POUR TOUS LES ONGLETS, dessiné AU-DESSUS d'eux.
  //
  // 🔴 AU-DESSUS, ET C'EST TOUT CE QUI DISTINGUE UN RÉGLAGE COMMUN D'UN AUTRE.
  // Posés sous les onglets, les deux réglages qui valent pour les huit barres se
  // lisaient comme ceux de la barre ouverte — et le joueur qui décoche
  // « verrouillées » sur l'onglet 3 croit n'avoir déverrouillé que la barre 3.
  // Rien dans le texte ne pouvait le détromper : c'est la PLACE qui le dit.
  std::span<const OptionRowView> shared_rows;

  // CE QUI NE DÉFILE PAS : posé tout en haut, au-dessus des réglages communs et
  // des onglets, qui défilent dessous.
  //
  // 🔴 POUR UN CADRE D'ESSAI, et c'est sa raison d'être : on règle une couleur
  // tout en bas de la page en regardant l'entité qu'elle change. Un cadre qui
  // défile avec les rangées sort de l'écran au moment où on en a besoin.
  std::span<const OptionRowView> pinned_rows;

  // UN VOLET COLLÉ À DROITE DE LA FENÊTRE, de toute sa hauteur, ou vide.
  //
  // 🔴 POUR UN CADRE QU'ON MANIPULE : les ancres d'une trajectoire se tirent
  // mieux en grand, et la page garde sa place pour les valeurs. Un cadre dont
  // la hauteur est nulle y prend toute la place qui reste.
  std::span<const OptionRowView> side_rows;

  // UNE SECONDE NAVIGATION, DANS LA SECTION, ou vide quand elle n'en a pas.
  //
  // 🔴 CE QU'ELLE RÉSOUT : une section qui décrit HUIT objets semblables — les
  // huit barres de raccourcis, et leurs dix-sept réglages chacune — fait cent
  // trente lignes qui se ressemblent toutes. Le joueur y cherche « la taille des
  // cases de la barre 5 » en faisant défiler, et il compte.
  //
  // ⚠ LA FENÊTRE NE SAIT PAS CE QUE C'EST : elle montre des onglets et retient
  // celui qu'on a choisi. C'est l'appelant qui ne compose alors QUE les lignes
  // de cet onglet-là — donc la liste reste courte, et rien ici n'a besoin de
  // connaître les barres.
  std::span<const char* const> tabs;

  // UNE PAGE QUI SE DESSINE ELLE-MÊME, ou vide. Appelée sous les rangées, dans
  // le style resserré du volet.
  //
  // ⚠ POUR CE QUI N'EST PAS UNE RANGÉE DE RÉGLAGE : une liste qu'on édite, des
  // nœuds repliables, des champs qu'on tape. La fenêtre ne sait toujours rien de
  // ce qu'elle montre — c'est l'appelant qui a fermé sur sa vue et sur ce qu'il
  // attend en retour.
  OptionSectionBody body;

  // LA PAGE A DES RÉGLAGES QU'ON PEUT RENDRE À L'ORIGINE, elle seule : le bouton
  // « Rétablir cette page » ne se montre que là. ⚠ FAUX PAR DÉFAUT : une page
  // tenue ailleurs — par le serveur, par un autre fichier — n'a rien à rendre,
  // et un bouton qui ne fait rien se lit comme une panne.
  bool restorable = false;
};

struct OptionsWindowState {
  bool open = false;
  // COMBIEN DE SECONDES AVANT QUE L'AFFICHAGE NE REVIENNE EN ARRIÈRE, ou moins
  // un quand rien n'attend. ⚠ POSÉ PAR L'APPELANT : c'est lui qui tient le
  // compte, et lui qui sait défaire.
  int revert_seconds = -1;
  // LA BOÎTE DU FILET A-T-ELLE DÉJÀ ÉTÉ DEMANDÉE POUR CE COMPTE-LÀ ?
  //
  // 🔴 UN SURGISSANT NE S'OUVRE PAS PARCE QU'ON LE DESSINE. Sans une demande
  // explicite, `BeginPopupModal` rend faux à chaque image : la question n'est
  // jamais peinte, personne ne peut y répondre, et c'est le TEMPS qui répond à
  // la place du joueur — son réglage d'affichage se défaisait tout seul au bout
  // de quinze secondes, sans que rien à l'écran ne l'ait annoncé.
  //
  // ⚠ UNE SEULE DEMANDE PAR ARMEMENT, d'où ce souvenir : la redemander à
  // chaque image rouvrirait la boîte que le joueur vient de fermer.
  bool revert_asked = false;

  // La section montrée, indice dans la liste reçue. ⚠ CHEZ L'APPELANT, et non
  // dans une statique de la fenêtre : c'est ce qui permettra à une commande
  // d'ouvrir le panneau SUR une section plutôt que là où on l'avait laissé.
  int section = 0;
  // L'ONGLET OUVERT dans la section courante, quand elle en a. ⚠ LU PAR
  // L'APPELANT : c'est lui qui compose les lignes, donc lui qui doit savoir
  // lesquelles.
  //
  // 🔴 EN LECTURE, ET NON EN COMMANDE. C'est ImGui qui tient l'onglet ouvert ;
  // celui-ci le RELÈVE à chaque image. Lui écrire un rang n'ouvre rien — voir le
  // champ suivant, qui est la commande.
  int section_tab = 0;
  // L'ONGLET QU'ON DEMANDE À OUVRIR, ou moins un. Consommé dès qu'il est
  // appliqué : une image, pas plus.
  //
  // 🔴 DEUX CHAMPS ET NON UN, ET C'EST UN BUG QUI L'A MONTRÉ. Imposer le rang
  // courant à chaque image annule le clic aussitôt reçu — les onglets restent
  // bloqués sur le premier. Ce qu'on impose doit donc être DEMANDÉ, distinct de
  // ce qui est ouvert, et s'effacer après usage.
  //
  // ⚠ POSÉ À ZÉRO EN CHANGEANT DE SECTION : l'onglet 6 d'une section n'a rien à
  // voir avec l'onglet 6 d'une autre, et en garder le rang ferait ouvrir une
  // section sur une page que le joueur n'a pas demandée.
  int section_tab_wanted = -1;
};

// Ce que le joueur a changé. ⚠ UNE SEULE LIGNE PAR IMAGE, et c'est suffisant :
// il n'a qu'une souris.
// 🔴 UN SEUL CHAMP PORTE LA RÉPONSE, ET C'EST LE GENRE DE LA LIGNE QUI DIT
// LEQUEL. Les autres gardent leur valeur d'origine — zéro, faux — et se lisent
// donc parfaitement bien : un réglage qui interroge le mauvais champ ne casse
// rien, il reçoit zéro. Une liste déroulante lue dans `value` choisit toujours
// sa PREMIÈRE entrée, et l'on cherche la panne du côté de ce qu'on applique.
//
//   kToggle   ->  on
//   kSlider   ->  value
//   kChoice   ->  chosen      ⚠ PAS `value`
//   kColor    ->  color
//   kAction   ->  rien, `changed` suffit
//   kCanvas   ->  dragged et point, ou copy_to ; `changed` est l'ANCRE
//   kText     ->  text
//
// Le bouton de retour d'une rangée rend `revert`, quel que soit son genre.
struct OptionsWindowAction {
  // La ligne qui a bougé, ou moins un. C'est un `GameOption` chez l'appelant :
  // cette couche n'en connaît pas les noms.
  int changed = -1;
  bool on = false;
  float value = 0.0F;
  int chosen = 0;
  ImU32 color = 0;
  std::string text;
  // UNE ANCRE TIRÉE DANS UN CADRE, et où elle est. ⚠ `dragged` DIT QUE `point`
  // FAIT FOI : une ancre peut porter l'identifiant d'un curseur, qui rend
  // `value` — voir `OptionCanvasMove`.
  bool dragged = false;
  ImVec2 point{0.0F, 0.0F};
  // Ou CE QU'ELLE PORTE, COPIÉ vers cette cible : voir `OptionCanvasMove`.
  int copy_to = -1;
  // LA RANGÉE `changed` REVIENT À SA RÉFÉRENCE : voir `OptionRowView::revert`.
  // ⚠ LES CHAMPS DE VALEUR NE DISENT ALORS RIEN.
  bool revert = false;
  bool restore_defaults = false;
  // LA SEULE PAGE À RENDRE À L'ORIGINE, par son rang dans les sections, ou
  // moins un.
  int restore_section = -1;
  // Le joueur a confirmé le réglage d'affichage, ou l'a refusé. ⚠ LES DEUX
  // EXISTENT : ne rien dire n'est pas refuser — c'est le temps qui refuse, et
  // l'appelant le compte lui-même.
  bool keep_display = false;
  bool revert_display = false;
  // MAJ + CLIC : le lien de cette page — par son rang dans les sections — dans
  // la saisie du chat, ou moins un. Et l'onglet de la page qu'il désigne, ou
  // moins un pour la page entière. Voir `chat_link_offer.h`.
  int link_section = -1;
  int link_tab = -1;
};

// ── UNE SEULE FENÊTRE, DEUX FAMILLES ────────────────────────────────────────
//
// ⛔ IL Y EN AVAIT DEUX : les Options « du jeu » et les Réglages « Moonlight »,
// séparés selon que le client d'origine sait régler la chose ou non.
//
// 🔴 C'ÉTAIT UNE FRONTIÈRE DE DÉVELOPPEUR. Elle disait QUI avait écrit le
// réglage, pas ce qu'il règle — et un joueur qui cherche la police de sa
// conversation n'a aucun moyen de deviner laquelle des deux fenêtres la porte.
// L'affichage l'a rompue tout seul : le client d'origine règle sa résolution
// dans son lanceur, donc ni d'un côté ni de l'autre.
//
// Ce qui les remplace se lit du point de vue du JOUEUR : ce que le programme
// fait — l'image, le son, le monde —, et ce que chaque fenêtre fait.
inline constexpr const char* kOptionsWindowTitle = core::TrText("Options");
inline constexpr const char* kOptionsWindowId = "options";

// ⚠ LES TITRES DE FAMILLE NE SONT PAS ICI : cette couche ne connaît aucune
// section. C'est l'appelant qui les nomme, avec les siennes.
//
// ⚠ LES OPTIONS DU JEU OFFRENT LE LIEN DANS LE CHAT, sur leurs entrées et leurs
// onglets ; les réglages de l'écran de connexion, qui n'a pas de chat, non.

[[nodiscard]] OptionsWindowAction DrawOptionsWindow(OptionsWindowState& state,
                                                    std::span<const OptionSectionView> sections,
                                                    const SkinArt& art);

// LES RÉGLAGES DE L'ÉCRAN DE CONNEXION : la même fenêtre, sur les seules pages
// qu'on lui donne, avec une note qui dit où trouver le reste.
//
// ⚠ SOUS SON PROPRE NOM : sa place et sa taille ne se confondent pas avec celles
// des Options du jeu, qu'on ouvre sur un autre écran.
inline constexpr const char* kLoginSettingsWindowTitle = core::TrText("Réglages");
inline constexpr const char* kLoginSettingsWindowId = "login_settings";

[[nodiscard]] OptionsWindowAction DrawLoginSettingsWindow(
    OptionsWindowState& state, std::span<const OptionSectionView> sections, const SkinArt& art);

}  // namespace ro::ui
