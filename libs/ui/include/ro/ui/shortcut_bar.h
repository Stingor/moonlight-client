#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/ui/hud_frame.h"
#include "ro/ui/skin_art.h"

namespace ro::ui {

// LA BARRE DE RACCOURCIS : neuf cases, sous les doigts.
//
// ⚠ ELLE NE LANCE RIEN ET NE RETIENT RIEN. Une case lui arrive avec son icône,
// sa touche et son rang déjà décidés ; elle rend ce que le joueur a fait — une
// case cliquée, une case vidée, une compétence déposée. C'est la boucle qui
// arme, qui envoie, et qui enregistre.

// CE QU'UN GLISSER PORTE, ENTRE LE GRIMOIRE ET LA BARRE.
//
// ⚠ LE NOM EST PARTAGÉ PAR LES DEUX CÔTÉS, et c'est tout ce qui les relie : la
// grille l'émet, la barre l'accepte, et aucune des deux ne connaît l'autre.
inline constexpr const char* kSkillDragPayload = "ro_skill";
struct SkillDrag {
  std::uint16_t skill_id = 0;
  std::uint16_t level = 0;
};

// OÙ UN OBJET SE TROUVE. 🔴 UNE PLACE NE VEUT RIEN DIRE SANS SON CONTENANT : la
// place 3 du sac et la place 3 du coffre désignent deux objets différents, et le
// paquet à émettre n'est pas le même. Une cible qui recevrait la place seule
// devrait deviner, et devinerait faux deux fois sur trois.
enum class ItemHolder : std::uint8_t {
  kInventory,
  kCart,
  kStorage,
  // Le mannequin de la fiche de personnage. ⚠ CE N'EST PAS UN CONTENANT DU
  // SERVEUR : une pièce portée vit dans le sac, avec un drapeau. Il désigne la
  // FENÊTRE d'où le geste part, ce qui suffit à savoir quoi en faire.
  kEquipment,
  // L'ÉQUIPEMENT D'UN AUTRE JOUEUR, montré par « Voir l'équipement ». ⚠ RIEN NE
  // S'Y PREND ET RIEN N'Y VA : il ne sert qu'à décorer une description — la
  // place est celle que le serveur a donnée à la pièce dans le sac de son
  // porteur.
  kViewedEquipment,
  // L'ALBUM DE CARTES. ⚠ UNE POCHETTE N'EST PAS UNE PLACE : elle ne se désigne
  // que par l'identifiant de sa carte. Rien n'en part par un transfert de
  // contenant — le retrait a sa propre charge —, et il ne sert qu'à dire à une
  // description d'où vient la carte qu'on survole.
  kCardAlbum,
  // UN LIEN DE LA CONVERSATION : l'objet n'est à personne ici, et tout ce qu'on
  // en sait voyage dans la balise, que `app` a retenue.
  kChatLink,
};

// UN TRANSFERT DEMANDÉ D'UN CONTENANT VERS UN AUTRE.
//
// ⚠ CE N'EST L'AFFAIRE D'AUCUNE FENÊTRE EN PARTICULIER : le geste part de l'une
// et tombe dans l'autre. C'est la fenêtre qui REÇOIT qui le rend, parce que
// c'est elle qui a vu le lâcher — et `app` sait quel paquet les deux bouts
// appellent.
//
// ⚠ UNE PLACE NULLE VEUT DIRE « RIEN » : le serveur décale ses index, une place
// n'est donc jamais nulle et peut servir de sentinelle.
struct ItemTransferAsk {
  ItemHolder from = ItemHolder::kInventory;
  ItemHolder to = ItemHolder::kInventory;
  std::uint16_t client_slot = 0;
};

// CE QU'UN GLISSER PORTE, ENTRE LE SAC ET LA BARRE.
//
// ⚠ UN SECOND NOM, ET NON LE MÊME. ImGui refuse une charge dont le nom ne
// correspond pas, et c'est ce refus qui empêche de déposer une compétence là où
// l'on attend un objet — sans qu'aucune des deux fenêtres ait à le vérifier.
//
// ⚠ ELLE PORTE LES DEUX, ET CHAQUE DESTINATAIRE PREND CELUI QUI DURE AUTANT
// QUE LUI. Une case de raccourci survit à la session : elle retient
// l'IDENTIFIANT, parce qu'une place se renumérote dès qu'on consomme. Le
// mannequin, lui, agit dans la seconde : il prend la PLACE, parce que deux
// exemplaires du même objet n'ont ni le même raffinement ni les mêmes cartes,
// et que l'identifiant seul ne dirait pas lequel on vient de saisir.
inline constexpr const char* kItemDragPayload = "ro_item";
struct ItemDrag {
  std::uint32_t item_id = 0;
  std::uint16_t client_slot = 0;
  // D'OÙ IL SORT. Voir `ItemHolder` : sans cela, la place ne désigne rien.
  ItemHolder from = ItemHolder::kInventory;
  // 🔴 LA PIÈCE EST-ELLE SUR LE CORPS ? C'est ce qui donne son SENS au geste,
  // et non l'endroit où on la lâche : tirée du sac vers le mannequin elle
  // s'enfile, tirée du mannequin vers le sac elle s'ôte. Sans ce drapeau,
  // chaque cible devrait aller demander à l'autre fenêtre ce qu'elle tient.
  //
  // ⚠ ET C'EST AUSSI CE QUI FAIT QU'UN GESTE INUTILE N'ENVOIE RIEN : un objet
  // du sac lâché dans le sac, une pièce portée relâchée sur le mannequin.
  bool worn = false;
};

// CE QU'ON VIENT DE LÂCHER HORS DE TOUTE FENÊTRE — donc sur le monde.
//
// 🔴 C'EST LA SEULE FAÇON DE LE SAVOIR : un glisser qui n'aboutit nulle part ne
// déclenche aucune cible, par définition. On le lit donc à la source, en
// demandant à ImGui ce qu'il porte encore et si quelqu'un l'a pris.
//
// ⚠ À APPELER APRÈS LE DESSIN DE TOUTES LES FENÊTRES, et le moment fait tout :
// avant, une fenêtre qui n'a pas encore tourné pourrait accepter la charge, et
// l'on jetterait au sol ce qu'on rangeait dans la barre de raccourcis.
//
// ⚠ ET AVANT LA FIN DE L'IMAGE : ImGui efface la charge dans son `EndFrame`.
//
// Rend faux tant que rien n'a été lâché. `carried` n'est alors pas touché.
[[nodiscard]] bool ItemDroppedOnNothing(ItemDrag* carried);

// Une case, telle qu'elle se dessine.
struct ShortcutSlotView {
  std::uint16_t skill_id = 0;
  // L'objet que la case porte, zéro pour une compétence. Les deux ne cohabitent
  // jamais.
  std::uint32_t item_id = 0;
  // Combien il en reste dans le sac. ⚠ ZÉRO N'EFFACE PAS LA CASE : on rachètera,
  // et une case qui disparaît en buvant sa dernière potion serait perdue.
  int amount = 0;
  std::string_view name;
  // La touche qui la déclenche — « F1 ». Vide quand aucune ne lui est liée.
  std::string_view key;
  SkinImage icon;
  int level = 0;
  // LE RANG LE PLUS HAUT QU'ON AIT APPRIS, qui borne le précédent.
  //
  // 🔴 LA CASE NE PEUT PAS LE DEVINER : elle retient le rang qu'on y a posé, et
  // rien ne dit combien on en a. Sans cette borne, Maj + molette monterait
  // au-delà de ce que le serveur accepte — et il refuserait le lancement sans
  // que rien à l'écran n'explique pourquoi.
  //
  // ⚠ NUL QUAND LA COMPÉTENCE N'EST PLUS APPRISE : le réglage se ferme alors de
  // lui-même, comme la case se grise.
  int most_level = 0;
  // ⚠ FAUX QUAND LE SERVEUR NE DONNE PLUS CETTE COMPÉTENCE : changement de
  // métier, oubli, ou simplement une carte où il n'a pas encore parlé. La case
  // se grise et ne part pas, mais ne s'efface pas — on peut réapprendre.
  bool known = false;
  // La part du temps d'attente qui reste, de un à zéro.
  float cooldown = 0.0F;
  // ET COMBIEN DE TEMPS, EN CLAIR.
  //
  // ⚠ LES DEUX, ET CE N'EST PAS UN DOUBLON : la part dessine le voile qui monte,
  // le temps écrit les chiffres. Le voile seul ne dit rien d'une attente de trois
  // minutes — il bouge d'un pixel par seconde — et les chiffres seuls ne se
  // lisent pas du coin de l'œil en combattant.
  std::uint32_t cooldown_ms = 0;
  // ÉCLAIRÉE : elle porte la compétence que le mode clavier du grimoire
  // survole. On voit où elle est déjà avant de la poser ailleurs.
  bool highlighted = false;
};

// HUIT BARRES DE DOUZE CASES.
//
// 🔴 CES DEUX NOMBRES SONT AUSSI DANS `ro::sim`, ET CE N'EST PAS UN DOUBLON
// TOLÉRÉ : `ui` ne dépend pas de `sim` — c'est le graphe des couches, et
// l'éditeur de liens le fait respecter. La couche qui connaît les deux, `app`,
// pose un `static_assert` qui rend la divergence impossible à compiler. Voir la
// fiche 0038.
inline constexpr std::size_t kShortcutBarCount = 8;
inline constexpr std::size_t kShortcutSlotsPerBar = 12;
inline constexpr std::size_t kShortcutSlotTotal = kShortcutBarCount * kShortcutSlotsPerBar;

// CE QUI SE DESSINE DERRIÈRE UN TEXTE DE CASE.
//
// ⚠ TROIS ET PAS DEUX : « aucune » en est une. Sur un fond uni choisi par le
// joueur, un cerne noir autour d'un texte clair est du bruit — et c'est
// justement quand on personnalise qu'on veut pouvoir l'éteindre.
enum class SlotTextShadow : std::uint8_t {
  kNone = 0,
  // Le cerne, tout autour : lisible sur n'importe quelle icône, à n'importe
  // quelle couleur. C'est le défaut, et ce que fait le jeu.
  kHalo,
  // L'ombre portée, décalée d'un côté. Plus légère, et elle garde au texte sa
  // forme — mais elle ne sauve pas un texte posé sur une icône claire.
  kDrop,
};

// LES COULEURS ET LES TAILLES D'ORIGINE, celles que le jeu emploie.
//
// 🔴 ICI ET NON DANS LE DESSIN, parce que ce sont des DÉFAUTS de
// réglage : le fichier lit la valeur présente à la déclaration, donc elle doit
// vivre là où le champ la porte. Deux endroits en feraient deux vérités, et le
// jour où elles divergeraient, c'est la moitié des joueurs — ceux dont le
// fichier n'a pas la clé — qui verrait l'autre.
//
// ⚠ SOMBRES, contrairement au corps des fenêtres : la barre se pose SUR LE
// MONDE, pas dans un panneau, et un aplat clair y ferait un bandeau qui mange
// l'écran.
inline constexpr ImU32 kDefaultSlotEmptyColor = IM_COL32(0, 0, 0, 120);
// ⚠ LES DEUX FONDS DE CONTENU SONT TRANSPARENTS PAR DÉFAUT, et ce n'est pas un
// oubli : une icône les recouvre. Ils existent pour qui veut teinter ses cases
// par genre — la barre de soin en vert, celle d'attaque en rouge —, ce que le
// jeu ne propose pas.
inline constexpr ImU32 kDefaultSlotSkillColor = IM_COL32(0, 0, 0, 120);
inline constexpr ImU32 kDefaultSlotItemColor = IM_COL32(0, 0, 0, 120);
inline constexpr ImU32 kDefaultSlotBorderColor = IM_COL32(150, 150, 158, 200);
inline constexpr float kDefaultSlotBorderThickness = 1.0F;
inline constexpr ImU32 kDefaultSlotLabelColor = IM_COL32(255, 255, 255, 235);
inline constexpr ImU32 kDefaultSlotShadowColor = IM_COL32(0, 0, 0, 220);
inline constexpr ImU32 kDefaultCountdownColor = IM_COL32(255, 235, 150, 255);
inline constexpr float kDefaultShadowOffset = 1.0F;
// Les bornes des tailles de texte. ⚠ UN MULTIPLICATEUR ET NON DES POINTS : une
// case va de seize à soixante-quatre pixels, et une taille absolue déborderait
// des petites tout en se perdant dans les grandes.
inline constexpr float kSmallestLabelScale = 0.5F;
inline constexpr float kLargestLabelScale = 2.0F;
inline constexpr float kLargestShadowOffset = 4.0F;
inline constexpr float kLargestBorderThickness = 5.0F;

// CE QU'UNE BARRE MONTRE, ET COMMENT.
//
// ⚠ NI POSITION NI TAILLE ICI, et c'est délibéré : chaque barre est une fenêtre
// nommée, et ImGui retient déjà sa place dans son fichier de réglages — comme
// pour toutes les autres. Deux mémoires pour une position finiraient par se
// contredire, et c'est celle qu'on n'a pas relue qui gagnerait.
//
// 🔴 L'APPARENCE EST PAR BARRE, ET NON COMMUNE AUX HUIT. Huit barres libres
// servent justement à SÉPARER — les soins d'un côté, l'attaque de l'autre —,
// et une couleur par famille est ce qui rend la séparation lisible du coin de
// l'œil.
// C'est aussi ce qui coûte : régler huit fois la même chose. D'où le bouton qui
// recopie une barre sur les sept autres.
struct ShortcutBarLayout {
  bool shown = false;
  // LE CLIC LA TRAVERSE-T-IL ?
  //
  // 🔴 ET MAJ LA REND VIVANTE LE TEMPS D'UN GESTE, sans quoi le réglage serait
  // un aller sans retour : une barre sourde ne peut plus être ni déplacée, ni
  // remplie, ni même décochée depuis elle-même. La touche est ce qui rend le
  // réglage réversible sans aller rouvrir le panneau.
  //
  // ⚠ PAR BARRE, alors que le verrou est commun aux huit : une barre de parade
  // que l'on regarde sans y toucher n'a rien à prendre à la souris, pendant que
  // celle des soins doit répondre au clic.
  bool click_through = false;
  // Les colonnes de la grille. UNE colonne fait une barre VERTICALE : c'est la
  // même mécanique, et c'est ce qui la rend gratuite.
  int columns = 12;
  // Combien de ses douze cases elle montre.
  //
  // 🔴 CACHER N'EST PAS RENUMÉROTER. Les douze cases appartiennent à la barre :
  // en montrer trois laisse les neuf autres à leur place, avec leurs raccourcis
  // et leurs entrées de fichier. Voir `ro::sim::ShortcutBars`.
  int shown_slots = 12;
  float slot_side_px = 32.0F;
  float gap_px = 2.0F;

  // ── L'APPARENCE ───────────────────────────────────────────────────────────
  // Le fond d'une case, selon ce qu'elle porte. Transparence comprise : c'est
  // souvent elle qu'on vient régler, une case opaque cachant le monde derrière.
  ImU32 empty_color = kDefaultSlotEmptyColor;
  ImU32 skill_color = kDefaultSlotSkillColor;
  ImU32 item_color = kDefaultSlotItemColor;
  ImU32 border_color = kDefaultSlotBorderColor;
  // ⚠ ZÉRO ÉTEINT LE CADRE, et c'est une valeur utile : des cases jointives sans
  // bordure sont une disposition que des joueurs préfèrent.
  float border_thickness = kDefaultSlotBorderThickness;

  // L'étiquette de touche, en haut à gauche.
  ImU32 key_color = kDefaultSlotLabelColor;
  float key_scale = 1.0F;
  // Le nombre en bas à droite : le rang d'une compétence, la quantité d'un
  // objet. ⚠ LES DEUX ENSEMBLE, parce qu'ils occupent la même place et qu'une
  // case ne porte jamais les deux.
  ImU32 count_color = kDefaultSlotLabelColor;
  float count_scale = 1.0F;
  // Le temps qui reste, au centre. ⚠ SA PROPRE COULEUR : c'est le seul texte
  // d'une case qui dise « pas encore », et le confondre avec les autres lui
  // ôterait ce qu'il a d'alerte.
  ImU32 countdown_color = kDefaultCountdownColor;

  // Ce qui détache les textes de ce qu'il y a dessous. ⚠ COMMUN AUX TROIS : un
  // cerne réglé texte par texte serait trois fois le même réglage, et rien ne
  // justifierait qu'ils diffèrent sur une même case.
  ImU32 shadow_color = kDefaultSlotShadowColor;
  int shadow_kind = static_cast<int>(SlotTextShadow::kHalo);
  // De combien l'ombre PORTÉE se décale. Sans effet sur le cerne, qui entoure.
  float shadow_offset = kDefaultShadowOffset;
};

// Le plus petit et le plus grand côté de case qu'on laisse choisir. En deçà
// l'icône n'est plus lisible ; au-delà la barre mange l'écran.
inline constexpr float kSmallestSlotSidePixels = 16.0F;
inline constexpr float kLargestSlotSidePixels = 64.0F;
inline constexpr float kLargestSlotGapPixels = 12.0F;

// LA GRILLE D'UNE BARRE, une fois ses réglages bornés.
//
// ⚠ UNE FONCTION PURE, ET C'EST POUR ÇA QU'ELLE EST ICI. Ce calcul décide de la
// TAILLE de la fenêtre et de la PLACE de chaque case ; faux, il fait déborder
// une barre ou empile ses cases les unes sur les autres — et cela ne se voit
// qu'en jeu, à l'œil.
struct ShortcutBarGrid {
  std::size_t shown = 0;
  std::size_t columns = 0;
  std::size_t rows = 0;
  // Ce que la fenêtre fait de large, en CASES : la plus longue ligne.
  [[nodiscard]] std::size_t across() const noexcept { return shown < columns ? shown : columns; }
};

// ⚠ LES RÉGLAGES VIENNENT D'UN FICHIER QUE LE JOUEUR PEUT ÉCRIRE : zéro colonne
// diviserait par zéro au placement, et vingt cases déborderaient sur la barre
// suivante. On borne ici, une fois, plutôt qu'à chaque lecture.
[[nodiscard]] ShortcutBarGrid GridOfBar(const ShortcutBarLayout& look);

// LE RANG DE LA CASE SOUS UN POINT, en coordonnées LOCALES à la barre — origine
// au coin haut-gauche de la première case. Vide quand le point tombe entre deux
// cases, au-delà de la grille, ou hors de la barre.
//
// 🔴 CE CALCUL DÉCIDE DE CE QUE LA SOURIS TRAVERSE, et il se fait AVANT que la
// fenêtre existe : ImGui ne peut donc pas répondre à sa place. Faux d'une
// gouttière, il rend une barre sourde là où elle devrait répondre — ou l'inverse,
// et le joueur clique le décor au lieu de marcher.
//
// ⚠ PURE, ET C'EST POUR ÇA QU'ELLE EST ICI : sans cela, l'arithmétique des
// gouttières ne se vérifierait qu'à la souris, en jeu.
[[nodiscard]] std::optional<std::size_t> ShortcutSlotUnder(const ShortcutBarGrid& grid,
                                                           float local_x, float local_y, float side,
                                                           float gap);

// L'INDEX GLOBAL de la case de rang `rank` dans la barre `bar`.
//
// 🔴 LA MÊME FORMULE QUE `ro::sim::ShortcutIndex`, et le `static_assert` d'`app`
// est ce qui empêche les deux de diverger : `ui` ne peut pas appeler l'autre.
[[nodiscard]] constexpr std::size_t ShortcutSlotIndex(std::size_t bar, std::size_t rank) {
  return bar * kShortcutSlotsPerBar + rank;
}

struct ShortcutBarsLayout {
  std::array<ShortcutBarLayout, kShortcutBarCount> bars = [] {
    std::array<ShortcutBarLayout, kShortcutBarCount> all{};
    // ⚠ LA PREMIÈRE SEULE EST MONTRÉE. Huit barres vides à l'écran d'une
    // première partie diraient que le client est cassé ; celle-ci est celle du
    // jeu, avec ses touches.
    all[0].shown = true;
    return all;
  }();
  // L'étiquette de touche, en haut à gauche de chaque case.
  bool show_keys = true;
  // LES BARRES SONT-ELLES FIXES ?
  //
  // 🔴 VERROUILLÉ EST LE DÉFAUT, ET LE VERROU NE DÉSARME RIEN. Les cases
  // restent cliquables, glissables et déposables ; ce qu'il empêche, c'est de
  // DÉPLACER la barre. Déverrouillée, une barre se saisit de n'importe où — donc
  // ses cases deviennent inertes, sans quoi il n'y aurait rien à attraper.
  bool locked = true;
  // LA BARRE QU'ON TRAÎNE S'ACCROCHE-T-ELLE AUX AUTRES ? Ses bords se posent sur
  // ceux des barres affichées — alignés, ou bout à bout. Le même aimant que les
  // jauges entre elles, et la grille garde le dernier mot quand elle aimante.
  bool sticky = true;
};

// CE QU'UN GLISSER PORTE ENTRE DEUX CASES.
//
// ⚠ UN TROISIÈME NOM DE CHARGE, et c'est lui qui fait le tri : ImGui refuse
// celle dont le nom ne correspond pas. Une case ne peut donc pas se déposer dans
// le sac, ni un objet du sac se faire prendre pour une case.
//
// ⚠ L'INDEX EST GLOBAL, donc une case de la barre 1 se glisse sur une case de la
// barre 6 sans que rien de particulier ait à être écrit.
inline constexpr const char* kShortcutDragPayload = "ro_shortcut";
struct ShortcutDrag {
  std::uint16_t index = 0;
};

struct ShortcutBarsAction {
  // LES DEUX CASES D'UN GLISSER, ou moins de zéro. Elles s'ÉCHANGENT : lâcher
  // sur une case occupée rend son contenu à la place qu'on quitte, au lieu de
  // l'écraser pour un geste d'un pixel trop à droite.
  int moved_from = -1;
  int moved_to = -1;
  // LA CASE LÂCHÉE HORS DES BARRES, ou moins de zéro : le joueur la VIDE.
  //
  // ⚠ C'EST LE GESTE DU JEU, et il n'y a pas de confirmation — mais l'aperçu se
  // barre d'une croix rouge dès qu'on quitte les barres. L'avertir pendant qu'il
  // le fait est le seul moment où cela sert encore.
  int dropped_away = -1;
  // La case dont il veut POSER LE LIEN dans sa saisie — Maj et clic gauche —, ou
  // moins de zéro.
  //
  // ⚠ UN OBJET SEULEMENT, comme partout ailleurs : une balise de compétence
  // n'existe pas dans le fil du jeu.
  int link = -1;
  // La case dont il demande la DESCRIPTION, au clic droit, ou moins de zéro.
  // Objet ou compétence : `app` lit la case pour savoir quelle fenêtre ouvrir.
  int describe = -1;
  // La case que le joueur veut lancer, ou moins de zéro.
  //
  // 🔴 UN INDEX GLOBAL — barre × douze + rang —, jamais un rang dans sa barre :
  // c'est ce que `ro::sim::ShortcutBars` attend, et le seul moyen qu'une case
  // désigne la même chose des deux côtés.
  int cast = -1;
  // Celle qu'il vide — au clic molette, comme dans le jeu.
  int cleared = -1;
  // Celle où il vient de DÉPOSER une compétence, et laquelle.
  int dropped_on = -1;
  SkillDrag dropped;
  // Celle où il vient de déposer un OBJET, et lequel. ⚠ À PART, parce que les
  // deux dépôts ne peuvent pas arriver dans la même image et que les mêler
  // obligerait chaque lecteur à trancher.
  int item_dropped_on = -1;
  ItemDrag item_dropped;
  // ── LE RANG DE LANCEMENT, RÉGLÉ SUR LA CASE ───────────────────────────────
  //
  // 🔴 MAJ + MOLETTE, ET LE RÉGLAGE VIT DANS LA CASE. Le grimoire règle le rang
  // d'une compétence ; la barre, elle, peut en porter la MÊME deux fois — un
  // Heal de rang 1 pour soigner en marchant, un de rang 10 pour relever. C'est
  // donc la case qui retient le sien, et non la compétence.
  //
  // ⚠ MAJ, ET PAS LA MOLETTE SEULE : sur une barre, la molette nue ne survole
  // rien d'autre qu'une case, et régler par accident ce qu'on lance en combat
  // coûterait cher. Le modificateur dit l'intention.
  int level_for = -1;
  int level = 0;
};

inline constexpr const char* kShortcutBarTitle = core::TrText("Raccourcis");
inline constexpr const char* kShortcutBarId = "shortcut_bar";

// Dessine les barres MONTRÉES. `slots` porte les quatre-vingt-seize cases, dans
// l'ordre de leur index global.
//
// ⚠ TOUTES LES BARRES RENDENT UNE SEULE DEMANDE : le joueur n'a qu'une souris,
// et deux barres ne reçoivent pas deux clics dans la même image.
[[nodiscard]] ShortcutBarsAction DrawShortcutBars(const ShortcutBarsLayout& layout,
                                                  std::span<const ShortcutSlotView> slots,
                                                  const SkinArt& art);

// ── LA PLACE D'UNE BARRE ────────────────────────────────────────────────────
//
// ⚠ LA POSITION RESTE CELLE D'IMGUI, que le placeur des fenêtres retient : ces
// gestes l'écrivent comme la souris l'écrirait, et rien d'autre ne la garde.

// Le bord gauche qui centre une barre de `bar_width` sur l'écran donné. Pure.
[[nodiscard]] float CenteredBarLeft(float screen_left, float screen_width, float bar_width);

// La place d'une barre traînée en `at`, accrochée à ses sœurs AFFICHÉES : même
// bord, ou bout à bout, dans le rayon d'aimantation des cadres de HUD. Pure.
[[nodiscard]] ImVec2 SnappedAmongBars(ImVec2 at, ImVec2 size, const HudSiblings& bars);

// Accroche la barre qu'on traîne à ses sœurs, quand `layout.sticky` le veut.
//
// 🔴 À APPELER AVANT `SnapMovingWindow`, comme le cadre de HUD aimante ses frères
// avant la grille : une grille aimantée garde ainsi le dernier mot.
void SnapMovingShortcutBar(const ShortcutBarsLayout& layout);

// Centre la barre `bar` sur la largeur de l'écran, à la même hauteur. Les autres
// ne bougent pas. Sans effet tant que la barre n'a jamais été dessinée.
void CenterShortcutBar(std::size_t bar);

}  // namespace ro::ui
