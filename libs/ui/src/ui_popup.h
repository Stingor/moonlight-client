#pragma once

// LES SURGISSANTS DE L'INTERFACE, vus de l'extérieur : en est-il un d'ouvert, et
// comment en refermer un par son identifiant.
//
// En-tête PRIVÉ de `libs/ui/src`. 🔴 AUCUNE COPIE LOCALE : une copie gardée
// dans un espace anonyme ou dans un sous-espace de famille masque l'aide
// commune SANS ERREUR dans tout l'espace qui l'entoure. Elle ne devient
// ambiguë — donc visible — que déclarée dans `ro::ui` même, avec la même
// signature, et pour les appels faits hors de son espace anonyme.

#include <imgui.h>

namespace ro::ui {

// Un surgissant est-il ouvert, où que ce soit ? Boîte modale, menu ou liste
// déroulante, à n'importe quel niveau de la pile.
[[nodiscard]] inline bool AnyPopupOpen() {
  return ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);
}

// Referme le surgissant `id` et tous ceux ouverts au-dessus de lui ; sans effet
// s'il n'est pas ouvert. Ferme « de l'extérieur », sans entrer dans le
// surgissant. `id` se nomme dans la pile d'identités COURANTE, comme à
// l'ouverture : l'appel se fait au niveau où le surgissant a été demandé.
// Définie hors ligne, dans `modal_box.cpp` : la pile des surgissants est un
// détail interne d'ImGui, qui n'a pas à gagner chaque fichier incluant ceci.
void CloseModal(const char* id);

}  // namespace ro::ui
