#pragma once

// L'ÉCHELLE DE L'INTERFACE, en un seul endroit : celle que le joueur règle, et
// qu'ImGui applique déjà aux polices.
//
// En-tête PRIVÉ de `libs/ui/src`. 🔴 AUCUNE COPIE LOCALE : une copie gardée
// dans un espace anonyme ou dans un sous-espace de famille masque l'aide
// commune SANS ERREUR dans tout l'espace qui l'entoure. Elle ne devient
// ambiguë — donc visible — que déclarée dans `ro::ui` même, avec la même
// signature, et pour les appels faits hors de son espace anonyme.

#include <imgui.h>

namespace ro::ui {

// Le facteur de l'interface : 1 à la taille d'origine.
[[nodiscard]] inline float Scale() { return ImGui::GetStyle().FontScaleMain; }

// Une longueur de l'art d'origine, en pixels, portée à l'échelle de l'interface.
[[nodiscard]] inline float Px(float pixels) { return pixels * Scale(); }

}  // namespace ro::ui
