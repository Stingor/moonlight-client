#pragma once

#include <imgui.h>

#include "ro/ui/skin_art.h"

namespace ro::ui {

// L'ÉCRAN DE CHARGEMENT : une illustration, et dessous une bande qui dit où en
// est le chargement.
//
// 🔴 IL PASSE AVANT CE QUI BLOQUE, et c'est sa seule raison d'être. Monter une
// carte fige la fenêtre une seconde et demie ; ce figement, montré au milieu
// d'un formulaire ou d'une ville, se lit comme une panne. Montré sur cet
// écran-ci, il se lit comme ce qu'il est.
//
// ⚠ IL NE DÉCIDE RIEN : `ui` ne possède aucun état, et c'est `app` qui sait
// qu'une scène se prépare, quelle illustration montrer, où en est le chargement
// et quelle heure il est. L'animation se calcule depuis cette heure.

// Le fond, opaque : rien de ce qui est derrière ne doit se voir.
inline constexpr ImU32 kLoadingBackground = IM_COL32(0, 0, 0, 255);
// Le texte, un gris clair plutôt qu'un blanc pur qui éblouit sur le noir.
inline constexpr ImU32 kLoadingTextColor = IM_COL32(210, 210, 210, 255);

// LA BANDE DU BAS, hors de l'illustration : celle-ci porte en pied le nom de
// ses auteurs, qu'une barre posée dessus effacerait.
inline constexpr float kLoadingBandHeightPx = 96.0F;

struct LoadingScreenView {
  // L'illustration, montrée en entier, proportions gardées, au-dessus de la
  // bande. Inutilisable : le fond noir seul.
  SkinImage backdrop;
  // L'image en cours de l'animation d'attente, choisie par `app` à son heure.
  // Inutilisable : des points qui tournent, calculés sur `clock_s`.
  SkinImage spinner;
  // L'étape, déjà traduite : « Modèles 312 / 540 ».
  const char* step_text = "";
  // La part faite, déjà mise en forme : « 57 % ». Vide avec une barre
  // indéterminée.
  const char* percent_text = "";
  // De 0 à 1, ou négatif quand l'avancement ne se mesure pas : la barre
  // avance alors d'elle-même.
  float fraction = -1.0F;
  // L'opacité de tout l'écran par-dessus le noir, pour le fondu d'apparition.
  float opacity = 1.0F;
  // L'heure de l'animation, en secondes depuis qu'elle a commencé.
  float clock_s = 0.0F;
};

// OÙ SE POSE CHAQUE PIÈCE, en pixels de l'écran.
struct LoadingLayout {
  // L'illustration ; vide sans illustration.
  ImVec2 art_min;
  ImVec2 art_max;
  // La bande du bas.
  ImVec2 band_min;
  ImVec2 band_max;
};

// Le placement pour cet écran et une illustration de cette taille : la bande
// au pied, l'illustration centrée dans le reste, aussi grande qu'elle y tient.
[[nodiscard]] LoadingLayout LayoutLoadingScreen(ImVec2 screen_min, ImVec2 screen_size,
                                                float art_width, float art_height);

// Peint l'écran dans l'image d'ImGui en cours.
void DrawLoadingScreen(const LoadingScreenView& view);

// Le fond noir seul, et une ligne au milieu : l'écran d'avant que rien ne soit
// lisible. `text` est déjà traduit.
void DrawLoadingScreen(const char* text);

}  // namespace ro::ui
