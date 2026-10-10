#pragma once

#include <imgui.h>

namespace ro::ui {

// LA GRILLE D'ALIGNEMENT DE L'INTERFACE : un quadrillage plein écran, et
// l'aimant qui pose les fenêtres dessus pendant qu'on les déplace.
//
// ⚠ ELLE NE CONNAÎT AUCUNE FENÊTRE EN PARTICULIER, et c'est tout son intérêt :
// un aimant écrit dans chaque fenêtre serait un aimant de plus à écrire à
// chaque fenêtre neuve, et deux d'entre elles finiraient par ne pas s'aligner
// sur la même chose. Il y a UN point de passage — celui qu'ImGui appelle
// `MovingWindow` — et toute fenêtre qu'on traîne y passe.
//
// ⚠ CE QUI EST ACCROCHÉ À UNE ENTITÉ N'EN EST PAS. Une barre de vie au-dessus
// d'une tête, une bulle de parole, un nom : ils suivent quelque chose qui
// bouge dans le monde, et les aimanter à l'écran les décrocherait de ce qu'ils
// décrivent. Ils ne sont d'ailleurs pas des fenêtres.

// Le plus petit pas qui reste une grille. En deçà, les lignes se touchent et
// l'aimant ne retient plus rien.
inline constexpr int kSmallestGridCellPixels = 1;
inline constexpr int kLargestGridCellPixels = 128;

// De combien la croix centrale est plus visible que le reste du maillage.
inline constexpr float kGridCenterBoost = 2.5F;

struct AlignGrid {
  bool shown = false;
  // ⚠ SÉPARÉE DE L'AFFICHAGE : on aligne une fois à la grille visible, puis on
  // l'éteint en gardant l'aimant — ou l'inverse, on la regarde sans que rien ne
  // colle.
  bool magnetic = false;
  int cell_pixels = 32;
  ImU32 color = IM_COL32(255, 255, 255, 38);

  // Le pas, borné. ⚠ CE N'EST NI UN INDICE NI UNE COORDONNÉE DE CELLULE : c'est
  // une distance en pixels.
  [[nodiscard]] float step() const noexcept;

  // UNE COORDONNÉE D'ÉCRAN RAMENÉE SUR LA LIGNE LA PLUS PROCHE.
  //
  // ⚠ `extent` EST LA TAILLE DE L'ÉCRAN SUR CET AXE, et elle est obligatoire :
  // la grille est centrée, donc l'endroit des lignes dépend de la largeur. La
  // passer différente de celle du dessin ferait dériver l'aimant et le
  // quadrillage l'un par rapport à l'autre, sans que rien ne le dise.
  //
  // ⚠ S'APPLIQUE À UN BORD, jamais à une largeur : c'est le bord qui doit
  // tomber sur une ligne.
  //
  // Rend `at` inchangé quand l'aimant est éteint, pour que l'appelant puisse
  // l'appeler sans condition.
  [[nodiscard]] float Snap(float at, float extent) const noexcept;
};

// Peint le quadrillage SOUS toutes les fenêtres et par-dessus le monde.
void DrawAlignGrid(const AlignGrid& grid);

// Pose la fenêtre qu'on est en train de traîner sur la grille.
//
// 🔴 À APPELER JUSTE APRÈS L'OUVERTURE DE L'IMAGE, avant que quoi que ce soit
// n'ait lu une position. ImGui replace la fenêtre déplacée depuis la souris à
// chaque image, en absolu : corriger après coup ne dérive donc jamais, mais
// corriger TROP TARD laisserait une image sur deux montrer la position brute.
void SnapMovingWindow(const AlignGrid& grid);

}  // namespace ro::ui
