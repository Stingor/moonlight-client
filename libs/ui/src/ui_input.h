#pragma once

// LES CHAMPS DE SAISIE DE L'INTERFACE, vus de l'extérieur.
//
// En-tête PRIVÉ de `libs/ui/src`. 🔴 AUCUNE COPIE LOCALE : une copie gardée
// dans un espace anonyme ou dans un sous-espace de famille masque l'aide
// commune SANS ERREUR dans tout l'espace qui l'entoure. Elle ne devient
// ambiguë — donc visible — que déclarée dans `ro::ui` même, avec la même
// signature, et pour les appels faits hors de son espace anonyme.

#include <imgui.h>
#include <imgui_internal.h>

namespace ro::ui {

// 🔴 ÉCRIRE DANS LE TAMPON D'UN CHAMP SANS LE LUI DIRE laisse son curseur posé
// sur l'ancien texte, et le champ actif tient sa propre copie, qui écraserait la
// nôtre à l'image suivante. Celle-ci prévient le champ, s'il est en cours
// d'édition, et pose son curseur en FIN : le texte posé se complète, au lieu
// d'être sélectionné et remplacé à la première touche.
inline void ReloadInputField(ImGuiID field) {
  if (ImGuiInputTextState* edited = ImGui::GetInputTextState(field)) {
    edited->ReloadUserBufAndMoveToEnd();
  }
}

}  // namespace ro::ui
