#pragma once

#include "ro/ui/skin_art.h"

namespace ro::ui {

// L'APERÇU DE L'HABILLAGE, épinglé en tête de la page Skin : ce que la page
// elle-même ne montre pas — les textes de chaque rôle, la sélection, le papier
// des bulles et des descriptions —, peint avec l'habillage en cours. On règle
// une couleur tout en bas de la page en la regardant changer.
//
// Rien à cliquer : rend toujours faux (voir `OptionPanelDraw`).
bool DrawSkinLookSample(const SkinArt& art);

}  // namespace ro::ui
