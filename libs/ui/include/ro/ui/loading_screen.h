#pragma once

#include <imgui.h>

namespace ro::ui {

// L'ÉCRAN DE CHARGEMENT : un fond noir et une ligne de texte, rien d'autre.
//
// 🔴 IL PASSE AVANT CE QUI BLOQUE, et c'est sa seule raison d'être. Monter une
// carte fige la fenêtre une seconde et demie ; ce figement, montré au milieu
// d'un formulaire ou d'une ville, se lit comme une panne. Montré sur cet
// écran-ci, il se lit comme ce qu'il est.
//
// ⚠ IL NE DÉCIDE PAS QUAND IL PARAÎT : `ui` ne possède aucun état, et c'est
// `app` qui sait qu'une scène se prépare.

// Le fond, opaque : rien de ce qui est derrière ne doit se voir.
inline constexpr ImU32 kLoadingBackground = IM_COL32(0, 0, 0, 255);
// Le texte, un gris clair plutôt qu'un blanc pur qui éblouit sur le noir.
inline constexpr ImU32 kLoadingTextColor = IM_COL32(210, 210, 210, 255);

// Peint l'écran dans l'image d'ImGui en cours. `text` est déjà traduit.
void DrawLoadingScreen(const char* text);

}  // namespace ro::ui
