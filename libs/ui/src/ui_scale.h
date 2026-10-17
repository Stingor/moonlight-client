#pragma once

// L'ÉCHELLE DE L'INTERFACE, en un seul endroit : celle que le joueur règle, et
// qu'ImGui applique déjà aux polices.
//
// En-tête PRIVÉ de `libs/ui/src`. 🔴 AUCUNE COPIE LOCALE : une fenêtre qui
// recopierait l'une de ces deux fonctions dans son espace anonyme la rendrait
// ambiguë partout où cet en-tête est inclus.

#include <imgui.h>

namespace ro::ui {

// Le facteur de l'interface : 1 à la taille d'origine.
[[nodiscard]] inline float Scale() { return ImGui::GetStyle().FontScaleMain; }

// Une longueur de l'art d'origine, en pixels, portée à l'échelle de l'interface.
[[nodiscard]] inline float Px(float pixels) { return pixels * Scale(); }

}  // namespace ro::ui
