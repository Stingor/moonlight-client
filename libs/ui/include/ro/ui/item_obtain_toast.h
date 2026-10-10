#pragma once

#include <cmath>
#include <cstdint>
#include <span>
#include <string_view>

#include <imgui.h>

#include "ro/ui/skin_art.h"

namespace ro::ui {

// LE BANDEAU « <objet> - N obtained. », EN HAUT DE L'ÉCRAN AU RAMASSAGE.
//
// C'est la fenêtre 58 du client d'origine (`UINotifyItemObtainWnd`) : un cadre
// clair, l'icône de l'objet, son nom composé, et le suffixe du jeu.
//
// 🔴 CELLE DU CLIENT N'EMPILE PAS. Sa fabrique est idempotente : un deuxième
// ramassage ÉCRASE le premier au lieu de s'ajouter, si bien qu'en récolte on
// voit passer une fraction de ce qu'on ramasse. C'est le défaut qui justifie
// d'en écrire une plutôt que de la recopier trait pour trait.
//
// ⚠ `ui` NE SAIT RIEN DES OBJETS. Elle reçoit un nom déjà composé, un suffixe
// déjà formaté et une image — `app` croise le sac, la table des objets et le
// cache d'icônes. Le tri, la fusion et l'expiration se font en amont aussi : ce
// qui arrive ici est la pile telle qu'elle doit être peinte, dans l'ordre.

// ── LA GÉOMÉTRIE DU CLIENT ──────────────────────────────────────────────────
//
// Celles de la fenêtre 58 du client. Ce sont elles qui font que le
// bandeau se pose au même endroit et de la même taille que celui qu'il
// remplace.

// La hauteur du cadre natif. Documentaire : c'est la grille de tuiles qui
// décide de la nôtre, et elle tombe sur 28 (voir `ItemObtainRowHeight`).
inline constexpr float kNativeToastRowPixels = 32.0F;
// L'abscisse de l'icône, et sa taille de repli quand l'art n'en donne pas.
inline constexpr float kToastIconLeftPixels = 13.0F;
inline constexpr float kToastIconPixels = 24.0F;
// L'abscisse du texte, avec icône puis sans : le client recule le texte quand
// il n'a rien à poser devant.
inline constexpr float kToastTextLeftPixels = 41.0F;
inline constexpr float kToastTextLeftNoIconPixels = 13.0F;
// La marge à droite du suffixe, et le plancher de largeur du client.
inline constexpr float kToastRightPadPixels = 13.0F;
inline constexpr float kLeastToastWidthPixels = 34.0F;

// L'abscisse du cadre natif sur sa largeur de référence de 640 : ce que
// « décentrer » repose, faute d'une autre valeur qui veuille dire quelque chose.
inline constexpr int kNativeToastLeftPixels = 220;
// Son ordonnée, qui ne dépend d'aucune largeur.
inline constexpr int kNativeToastTopPixels = 75;

// ── LES BORNES DES RÉGLAGES ─────────────────────────────────────────────────

// Le plafond DUR de la pile, au-dessus du réglage : un nombre aberrant relu
// d'un fichier trafiqué ne doit pas faire grossir la file sans fin.
inline constexpr int kMostToastLines = 20;
inline constexpr int kLeastToastLines = 1;
// Une ligne qui ne tient pas un quart de seconde clignote au lieu de se lire.
inline constexpr int kLeastToastDurationMs = 250;
inline constexpr int kShortestToastSettingMs = 500;
inline constexpr int kLongestToastSettingMs = 20000;
inline constexpr int kMostToastRowGapPixels = 16;
inline constexpr int kMostToastPadVerticalPixels = 12;
inline constexpr int kMostToastRoundingPixels = 16;
inline constexpr int kMostToastBorderPixels = 4;
inline constexpr int kLeastToastFontScalePercent = 60;
inline constexpr int kMostToastFontScalePercent = 200;
// En dessous, les glyphes ne se distinguent plus : la même borne que le module
// d'origine.
inline constexpr int kLeastDrawableFontScalePercent = 50;

// L'objet montré en exemple quand on place l'ancre sans rien avoir ramassé. Un
// objet RÉEL — la potion rouge — pour voir la vraie icône et la vraie largeur.
inline constexpr std::uint32_t kToastPreviewItemId = 501;

// ── LES RÉGLAGES ────────────────────────────────────────────────────────────
//
// L'initialisateur de chaque champ EST son défaut, et la seule source : le
// fichier de réglages n'écrit que ce qui s'en écarte.
struct ItemObtainToastLook {
  // Combien de lignes tiennent à l'écran. Celle du client est bloquée à une.
  int most_lines = 8;
  // La pile pousse vers le bas, la plus récente en tête ; sinon les nouvelles
  // s'ajoutent sous les anciennes, comme un journal.
  bool newest_on_top = true;
  // Additionner les ramassages du MÊME objet tant que sa ligne est à l'écran.
  // Sans cela, récolter des consommables remplit la pile d'une seule et même
  // herbe.
  bool merge_same = true;
  // Combien de temps une ligne tient. 5000 est la valeur en dur du client.
  int duration_ms = 5000;

  // ── L'ANCRAGE ─────────────────────────────────────────────────────────────
  //
  // La position ne se règle pas au curseur : elle se prend à la souris, sur une
  // poignée qui n'apparaît que déverrouillée. `pos_x` NÉGATIF veut dire centré
  // — on ne peut pas à la fois centrer et placer.
  bool locked = true;
  int pos_x = kNativeToastLeftPixels;
  int pos_y = kNativeToastTopPixels;

  // ── LA COMPACITÉ ──────────────────────────────────────────────────────────
  //
  // L'espace ENTRE deux lignes, puis la marge verticale à l'INTÉRIEUR d'une
  // ligne. ⚠ La seconde n'agit que sans le cadre : avec, la hauteur est imposée
  // par la grille de tuiles.
  int row_gap_px = 0;
  int pad_vertical_px = 0;
  int font_scale_percent = 130;

  // ── L'APPARENCE ───────────────────────────────────────────────────────────
  bool show_icon = false;
  // Le cadre clair en neuf morceaux du client. Décoché, le fond libre prend sa
  // place.
  bool show_frame = false;

  // ── LE FOND LIBRE, quand le cadre est masqué ──────────────────────────────
  //
  // Sans lui le texte se retrouve à nu sur la scène : lisible sur un sol
  // sombre, illisible dès qu'un mur clair passe dessous. ⚠ CES CHAMPS SONT
  // INERTES tant que `show_frame` est vrai, et le panneau les grise en
  // conséquence : ils ne se contredisent jamais à l'écran.
  bool background_shown = true;
  ImU32 background = IM_COL32(0x00, 0x00, 0x00, 255);
  int rounding_px = 16;
  bool border_shown = true;
  ImU32 border = IM_COL32(0x4A, 0x52, 0x1E, 255);
  int border_px = 1;

  // ── LES COULEURS DU TEXTE ─────────────────────────────────────────────────
  //
  // Le nom, puis le suffixe « - N obtained. » : une couleur à part pour que la
  // quantité se détache du nom.
  ImU32 text = IM_COL32(0xFF, 0xFF, 0xFF, 255);
  ImU32 quantity = IM_COL32(0x00, 0xFF, 0x00, 255);

  friend bool operator==(const ItemObtainToastLook&, const ItemObtainToastLook&) = default;
};

// ── L'OPACITÉ, DES DEUX CÔTÉS ───────────────────────────────────────────────
//
// 🔴 L'ALPHA VOYAGE DANS LA COULEUR, et le curseur d'opacité n'est qu'une
// SECONDE VUE du même octet. Deux champs séparés donneraient deux sources qui
// se contrediraient dès le premier chargement — et c'est bien un curseur qu'il
// faut garder : le nuancier du panneau porte sa barre d'alpha, mais on règle la
// transparence d'un fond en la regardant, pas en ouvrant un sélecteur.
inline constexpr int kWholeAlphaPercent = 100;
inline constexpr int kWholeAlphaChannel = 255;

[[nodiscard]] inline int ColorAlphaPercent(ImU32 color) {
  const int channel = static_cast<int>((color >> IM_COL32_A_SHIFT) & 0xFFU);
  // Arrondi au plus proche : sans lui, 255 rendrait 100 mais 178 rendrait 69,
  // et un aller-retour par le curseur ferait DÉRIVER l'opacité vers le bas.
  return (channel * kWholeAlphaPercent + kWholeAlphaChannel / 2) / kWholeAlphaChannel;
}

[[nodiscard]] inline ImU32 WithAlphaPercent(ImU32 color, int percent) {
  const int clamped =
      percent < 0 ? 0 : (percent > kWholeAlphaPercent ? kWholeAlphaPercent : percent);
  const auto channel = static_cast<ImU32>((clamped * kWholeAlphaChannel + kWholeAlphaPercent / 2) /
                                          kWholeAlphaPercent);
  return (color & ~IM_COL32_A_MASK) | (channel << IM_COL32_A_SHIFT);
}

// ── CE QU'IL Y A À PEINDRE ──────────────────────────────────────────────────

// UNE LIGNE DU BANDEAU.
//
// ⚠ LES DEUX TEXTES SONT DES VUES : ce qui les porte doit survivre à l'image.
// Elles ne se désignent donc qu'une fois la composition TERMINÉE — une vue
// prise pendant le remplissage du vecteur meurt à sa première réallocation.
struct ItemObtainToastEntry {
  // Le nom composé de l'objet, en UTF-8, tel que le client l'écrirait : affinage,
  // grade, mots de cartes. ⚠ SANS le suffixe d'emplacements « [N] », que le
  // chemin du bandeau ne compose pas.
  std::string_view name;
  // « - 5 obtained. », déjà formaté depuis le libellé de l'installation.
  // ⚠ SON ESPACE DE TÊTE LUI APPARTIENT : on le colle au nom tel quel.
  std::string_view suffix;
  // Vide quand le réglage tait l'icône, ou quand l'art ne la rend pas.
  SkinImage icon;
  // Un équipement CASSÉ ne change pas de couleur : c'est une ombre rouge
  // décalée d'un pixel qui le fait paraître rouge, comme partout ailleurs.
  bool damaged = false;
};

// CE QUE LE BANDEAU DEMANDE EN RETOUR : la position que la poignée vient de
// prendre. `moved` reste faux tant que rien n'a bougé — l'appelant n'écrit
// alors rien dans ses réglages.
struct ItemObtainToastAsk {
  bool moved = false;
  int pos_x = 0;
  int pos_y = 0;
};

// L'ÉTAT DE LA POIGNÉE, POSSÉDÉ PAR L'APPELANT.
//
// ⚠ IL SURVIT D'UNE IMAGE À L'AUTRE, et n'a donc pas sa place dans une variable
// de fonction : c'est `app` qui tient ce qui dure, comme pour la poignée d'un
// cadre d'interface.
struct ItemObtainAnchorGrip {
  // La largeur de la poignée, relue d'une image sur la suivante : on ne peut pas
  // centrer une fenêtre dont la taille n'est pas encore connue, et elle dépend
  // de la langue. Le centrage converge donc en une image, ce qui ne se voit pas.
  float width_px = 200.0F;
  // L'abscisse que le dessin a POSÉE, la dernière fois qu'il en a posé une.
  //
  // 🔴 C'EST ELLE QUI DIT SI LE JOUEUR A TIRÉ LA POIGNÉE, et surtout pas
  // l'abscisse recalculée de l'image courante : celle-ci se déduit d'une largeur
  // mesurée APRÈS le placement, si bien qu'à la première image la poignée a été
  // posée d'après une largeur de repli qui n'est pas la sienne. Comparer à la
  // valeur recalculée revient alors à prendre sa propre approximation pour un
  // geste — et le centrage se décoche tout seul, une fois pour toutes.
  float applied_x = 0.0F;
  bool applied = false;
  // Replacer la poignée pour UNE image, quand un réglage a déplacé le bandeau
  // sous elle. Sans cela ImGui garderait la position qu'elle avait à sa
  // création, et le retour au centrage ne se verrait pas.
  bool replace = false;
};

// ── LA GÉOMÉTRIE, EN FONCTIONS PURES ────────────────────────────────────────
//
// 🔴 SÉPARÉES DU DESSIN POUR ÊTRE VÉRIFIABLES. La hauteur sur grille de tuiles
// et l'arrondi de largeur sont exactement ce que personne ne revérifie à l'œil :
// une couture d'un pixel au bord droit ne se voit qu'à la bonne résolution.
//
// ⚠ AUCUNE NE LIT L'ÉTAT D'IMGUI : tout ce qu'elles ont à savoir du contexte de
// dessin leur est passé. C'est ce qui permet de les éprouver sans fenêtre.

// CE QUE L'IMAGE COURANTE IMPOSE, en pixels d'écran.
struct ItemObtainMetrics {
  // Le corps de police déjà mis à l'échelle et arrondi, celui qu'on dessinera.
  float font_px = 0.0F;
  // Le côté d'une tuile du cadre, tel que l'art le donne. ⚠ IL EST DÉJÀ À
  // L'ÉCHELLE de l'interface : le remultiplier ferait grossir le cadre deux
  // fois. Zéro quand l'habillage ne porte pas les pièces du cadre.
  float tile_px = 0.0F;
  // L'échelle de l'interface, celle qui décide de la taille des pièces comme de
  // celle du texte. Les mesures relevées sur le client sont en pixels d'un
  // écran non redimensionné : c'est elle qui les y ramène.
  float scale = 1.0F;
  float screen_width_px = 0.0F;
};

// TOUT TOMBE SUR UN PIXEL ENTIER.
//
// 🔴 Les tuiles du cadre sont échantillonnées AU PLUS PROCHE. Une borne à x.5
// tombe alors entre deux texels : au lieu d'adoucir, le point DUPLIQUE ou MANGE
// une colonne — d'où des coutures aux angles et un bord qui bave d'un pixel
// selon la position. Les sources de demi-pixel ne sont pas évidentes : la
// largeur vient d'une MESURE DE TEXTE, le centrage divise par deux, et la
// hauteur suit l'échelle de la police.
[[nodiscard]] inline float SnapToPixel(float value) { return std::floor(value + 0.5F); }

// LA HAUTEUR D'UNE LIGNE, toujours entière : elle sert d'incrément d'une ligne à
// la suivante, donc une fraction décalerait la pile de plus en plus.
//
// ⚠ AVEC LE CADRE, ELLE N'EST PAS LIBRE : elle se cale sur la grille de tuiles,
// sinon le neuf-morceaux échantillonne entre ses texels. On s'écarte alors des
// 32 px du client, en connaissance de cause — 32, c'est 28 de bordure plus 4
// d'intérieur, et écraser une tuile de 14 dans 4 px donne la couture la plus
// visible du cadre. À 28 il ne reste que les quatre coins, posés un pour un.
[[nodiscard]] float ItemObtainRowHeight(const ItemObtainToastLook& look,
                                        const ItemObtainMetrics& metrics);

// LA LARGEUR D'UNE LIGNE.
//
// ⚠ `ceil` ET PAS `SnapToPixel` : arrondir vers le bas rognerait le dernier
// glyphe d'un demi-pixel.
//
// ⚠ AVEC LE CADRE, L'INTÉRIEUR SE CALE SUR LA GRILLE DE TUILES, parce que la
// bande centrale est ÉTIRÉE : une largeur intérieure qui n'est pas un multiple
// de la tuile fait échantillonner la texture entre ses texels sur toute la
// longueur du bandeau. C'est l'intention de l'arrondi du client — le sien
// AJOUTE la tuile au lieu d'arrondir, si bien que sa largeur n'est presque
// jamais un multiple de quoi que ce soit.
[[nodiscard]] float ItemObtainRowWidth(const ItemObtainToastLook& look,
                                       const ItemObtainMetrics& metrics, bool has_icon,
                                       float name_px, float suffix_px);

// L'ABSCISSE DU TEXTE : le client le recule quand il n'y a pas d'icône devant.
[[nodiscard]] float ItemObtainTextLeft(const ItemObtainMetrics& metrics, bool has_icon);

// L'ABSCISSE DU BANDEAU. `pos_x` négatif = centré, ce que le client fait à sa
// façon : un x pensé pour 640, recentré à la résolution courante.
[[nodiscard]] float ItemObtainOriginX(const ItemObtainToastLook& look,
                                      const ItemObtainMetrics& metrics, float row_width_px);

// LE CORPS DE POLICE, toujours ENTIER : un corps fractionnaire fait
// rééchantillonner les glyphes de l'atlas et rend tout le texte pâteux.
[[nodiscard]] float ItemObtainFontPixels(const ItemObtainToastLook& look, float base_font_px);

// ── LE DESSIN ───────────────────────────────────────────────────────────────

// Peint la pile, et la poignée d'ancre si le verrou est ouvert.
//
// ⚠ SUR LA LISTE DE PREMIER PLAN : une notification passe au-dessus des
// fenêtres, sinon celle qu'on vient d'ouvrir la cacherait. Elle ne capte donc
// aucun clic — c'est la poignée, une vraie fenêtre, qui en capte, et elle
// n'existe que déverrouillée.
//
// ⚠ À APPELER DANS L'IMAGE D'IMGUI, hors de toute fenêtre.
//
// ⚠ LA LIGNE D'EXEMPLE VIENT DE L'APPELANT, comme toutes les autres : placer
// l'ancre sur une pile vide suppose de voir ce qu'on règle, mais `ui` ne sait
// composer ni le nom ni l'icône d'un objet. C'est `app` qui pose l'exemple dans
// `lines` quand le verrou est ouvert et que rien n'a été ramassé.
void DrawItemObtainToast(std::span<const ItemObtainToastEntry> lines,
                         const ItemObtainToastLook& look, const SkinArt& art,
                         ItemObtainAnchorGrip& grip, ItemObtainToastAsk* ask);

}  // namespace ro::ui
