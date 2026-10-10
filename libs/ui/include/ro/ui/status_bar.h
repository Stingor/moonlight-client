#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/ui/status_cell.h"

namespace ro::ui {

// LA BARRE DE MES ÉTATS : la rangée de `status_cell`, posée sur le monde.
//
// 🔴 ELLE NE PREND RIEN À LA SOURIS, et c'est ce qui la distingue de la barre de
// raccourcis. Une case d'état ne se clique pas : il n'y a rien à lancer, rien à
// déposer, rien à vider. La fenêtre est donc SOURDE en permanence, sauf quand on
// la déverrouille pour la déplacer — sans quoi elle avalerait les clics de
// déplacement du joueur sur toute son emprise.
//
// ⚠ NI POSITION NI TAILLE ICI : c'est une fenêtre nommée, et ImGui retient sa
// place comme pour toutes les autres. Deux mémoires pour une position finiraient
// par se contredire.

inline constexpr const char* kStatusBarTitle = core::TrText("États");
inline constexpr const char* kStatusBarId = "status_bar";

// LE COIN D'OÙ LA RANGÉE GRANDIT.
//
// 🔴 CE N'EST PAS UNE COQUETTERIE DE MISE EN PAGE. Une rangée grandit dans un
// sens ; ancrée du mauvais côté, elle RECULE quand un état tombe, et toutes les
// icônes glissent sous le curseur. Une barre posée à droite de l'écran veut
// pousser vers la gauche, celle du bas vers le haut.
//
// ⚠ L'ORDRE DE CES VALEURS EST PERSISTÉ : ne pas les intercaler.
enum class StatusAnchor : std::uint8_t {
  kTopLeft = 0,
  kTopRight,
  kBottomLeft,
  kBottomRight,
};

// Les bornes offertes. ⚠ LES RÉGLAGES VIENNENT D'UN FICHIER QUE LE JOUEUR PEUT
// ÉCRIRE : zéro ligne diviserait par zéro au placement, et deux mille icônes
// réserveraient un pan d'écran entier. On borne ici, une fois.
inline constexpr float kSmallestStatusSidePixels = 8.0F;
inline constexpr float kLargestStatusSidePixels = 48.0F;
inline constexpr float kLargestStatusGapPixels = 12.0F;
inline constexpr int kFewestStatusIcons = 1;
inline constexpr int kMostStatusIcons = 100;
inline constexpr int kMostStatusRows = 6;
inline constexpr float kSmallestStatusCountdownPixels = 7.0F;
inline constexpr float kLargestStatusCountdownPixels = 20.0F;
// ⚠ PAS ZÉRO : une barre parfaitement transparente est une barre qu'on croit en
// panne, et dont on ne retrouve plus le réglage.
inline constexpr float kFaintestStatusOpacity = 0.1F;
// Les mêmes réglages, en pour cent, pour les surfaces qui les tiennent en
// entiers (la grille et la fenêtre du groupe).
inline constexpr int kFaintestStatusOpacityPercent = 10;
inline constexpr int kFullStatusOpacityPercent = 100;
// La taille du temps restant sous une icône, par défaut.
inline constexpr int kDefaultStatusCountdownPixels = 11;

// L'opacité d'un réglage en pour cent, bornée : un fichier écrit à la main ne
// rend pas les icônes invisibles.
[[nodiscard]] constexpr float StatusOpacityOf(int percent) noexcept {
  const int bounded = percent < kFaintestStatusOpacityPercent ? kFaintestStatusOpacityPercent
                      : percent > kFullStatusOpacityPercent   ? kFullStatusOpacityPercent
                                                              : percent;
  return static_cast<float>(bounded) / static_cast<float>(kFullStatusOpacityPercent);
}

inline constexpr ImU32 kDefaultStatusSweepColor = IM_COL32(0, 0, 0, 140);
inline constexpr ImU32 kDefaultStatusCountdownBackground = IM_COL32(0, 0, 0, 0);
// Le fond : un bleu-noir à peine opaque. ⚠ SOMBRE, et non clair comme le corps
// d'une fenêtre : la barre se pose SUR LE MONDE.
inline constexpr ImU32 kDefaultStatusBackgroundColor = IM_COL32(13, 13, 18, 140);

// CE QUE LA BARRE MONTRE, ET COMMENT.
//
// ⚠ LES TROIS ÉNUMÉRATIONS SONT GARDÉES EN `int`, comme l'ombre d'une case de
// raccourci : ce sont des valeurs de FICHIER, et le fichier ne connaît que des
// entiers. Le dessin les borne à la lecture — voir `StatusAnchorOf` et ses
// jumelles —, de sorte qu'un fichier écrit à la main ne peut pas faire sortir le
// programme de ses propres valeurs.
struct StatusBarLayout {
  // 🔴 MONTRÉE D'EMBLÉE : il n'y a pas d'autre barre d'états, et une barre
  // éteinte par défaut rendrait invisibles tous les états du joueur.
  bool shown = true;
  bool locked = true;

  // LE FOND ET LE FILET, derrière les icônes.
  //
  // ⚠ ILS COUVRENT LES CASES MONTRÉES, PAS L'EMPRISE. La barre réserve la
  // grille entière pour que l'ancrage tienne, et un aplat de cette taille
  // serait un bandeau noir en travers du monde.
  bool border = true;
  ImU32 background_color = kDefaultStatusBackgroundColor;

  float side_px = 24.0F;
  float gap_px = 2.0F;
  // Le plafond TOTAL, toutes lignes confondues.
  int most = 40;
  int rows = 2;

  int anchor = static_cast<int>(StatusAnchor::kTopLeft);
  int sort = static_cast<int>(StatusSort::kArrival);

  int sweep = static_cast<int>(StatusSweep::kVertical);
  ImU32 sweep_color = kDefaultStatusSweepColor;

  bool show_countdown = true;
  float countdown_px = static_cast<float>(kDefaultStatusCountdownPixels);
  // ⚠ SOUS L'ICÔNE, LES CHIFFRES TOMBENT SUR LE MONDE : le fond n'orne pas, il
  // rend lisible. Transparent par défaut, comme le client.
  ImU32 countdown_background = kDefaultStatusCountdownBackground;

  float opacity = 1.0F;
  ImU32 icon_background = kDefaultStatusIconBackground;
};

// Les trois lectures bornées d'un réglage venu du fichier.
[[nodiscard]] StatusAnchor StatusAnchorOf(int stored) noexcept;
[[nodiscard]] StatusSort StatusSortOf(int stored) noexcept;
[[nodiscard]] StatusSweep StatusSweepOf(int stored) noexcept;

// LA GRILLE DE LA BARRE, une fois ses réglages bornés.
//
// 🔴 SES COLONNES SONT CELLES DE `PlaceStatusRow`, ET LA FORMULE EST LA MÊME :
// le plafond réparti sur les lignes, arrondi vers le haut. Si les deux
// divergeaient, l'emprise réservée ne serait plus celle qu'on dessine — la
// dernière colonne tomberait hors du cadre, ou le cadre traînerait une colonne
// vide. Un essai les compare.
struct StatusBarGrid {
  std::size_t columns = 0;
  std::size_t rows = 0;
};

[[nodiscard]] StatusBarGrid GridOfStatusBar(const StatusBarLayout& look);

// L'EMPRISE DE LA BARRE : la grille ENTIÈRE, et non ce qu'elle porte aujourd'hui.
//
// 🔴 C'EST CE QUI DONNE SON SENS À L'ANCRAGE. Une fenêtre qui rétrécirait avec le
// nombre d'états ferait bouger trois de ses quatre coins ; le coin ancré ne
// serait fixe que par hasard, celui du haut-gauche. L'emprise est donc constante,
// et c'est la rangée qui grandit dedans.
//
// ⚠ ELLE EST INVISIBLE — fond transparent, clics traversants. Ce n'est pas un
// panneau, c'est une place réservée.
//
// ⚠ ET ELLE PORTE LA PLACE DU COMPTE À REBOURS, qui se pose SOUS chaque case :
// la fenêtre découpe à son bord, et une hauteur au ras des icônes rendrait la
// dernière ligne de chiffres invisible.
[[nodiscard]] ImVec2 StatusBarSize(const StatusBarLayout& look, float scale);

// La place réservée sous chaque case — celle du compte à rebours —, en pixels
// d'écran. Zéro quand il ne s'affiche pas.
[[nodiscard]] float StatusCountdownRoom(const StatusBarLayout& look, float scale);

// LE COIN D'OÙ LA RANGÉE PART, dans une fenêtre posée en `window_pos`.
[[nodiscard]] ImVec2 StatusBarOrigin(ImVec2 window_pos, ImVec2 size, StatusAnchor anchor);

// La rangée que ces réglages décrivent, échelle d'interface comprise.
[[nodiscard]] StatusRowLayout RowOfStatusBar(const StatusBarLayout& look, float scale);

// Dessine la barre. Ne rend rien : il n'y a aucun geste à recueillir.
void DrawStatusBar(const StatusBarLayout& look, std::span<const StatusCellView> cells);

}  // namespace ro::ui
