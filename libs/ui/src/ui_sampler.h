#pragma once

// LE FILTRAGE DES IMAGES D'UNE LISTE DE DESSIN, en un seul endroit : net ou
// lissé, imposé avant les images, puis rendu après elles.
//
// 🔴 IL S'IMPOSE DANS LES DEUX CAS, pas seulement quand on lisse. Le moteur de
// rendu d'ImGui lie un OBJET SAMPLER avant chaque lot de commandes, et un
// sampler lié ÉCRASE le filtrage de la texture : le `GL_NEAREST` posé au
// téléversement ne décide de rien. Le sampler ambiant est le LINÉAIRE — ne rien
// faire, c'est lisser, et une case « Lisser » paraîtrait morte puisque son état
// décoché donnerait déjà ce que fait son état coché.
//
// ⚠ IL SE REND : la liste de dessin continue après les images — traits,
// ascenseur, habillage de la fenêtre — et hériterait sinon du mode posé.
//
// En-tête PRIVÉ de `libs/ui/src`. 🔴 AUCUNE COPIE LOCALE : une copie gardée
// dans un espace anonyme ou dans un sous-espace de famille masque l'aide
// commune SANS ERREUR dans tout l'espace qui l'entoure. Elle ne devient
// ambiguë — donc visible — que déclarée dans `ro::ui` même, avec la même
// signature, et pour les appels faits hors de son espace anonyme.

#include <imgui.h>

namespace ro::ui {

// Impose l'échantillonneur des images, lissé ou net, pour la suite de `into`.
//
// ⚠ UN MOTEUR DE RENDU PEUT NE PAS OFFRIR CES CALLBACKS : on dessine alors avec
// son filtrage ambiant, sans rien casser.
inline void ImposeImageSampler(ImDrawList* into, bool smooth) {
  const ImGuiPlatformIO& platform = ImGui::GetPlatformIO();
  const ImDrawCallback wanted =
      smooth ? platform.DrawCallback_SetSamplerLinear : platform.DrawCallback_SetSamplerNearest;
  if (wanted != nullptr) {
    into->AddCallback(wanted, nullptr);
  }
}

// Rend l'échantillonneur ambiant, le linéaire, à ce qui suit dans `into`.
inline void RestoreAmbientSampler(ImDrawList* into) {
  const ImDrawCallback ambient = ImGui::GetPlatformIO().DrawCallback_SetSamplerLinear;
  if (ambient != nullptr) {
    into->AddCallback(ambient, nullptr);
  }
}

}  // namespace ro::ui
