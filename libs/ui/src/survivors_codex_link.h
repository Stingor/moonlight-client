#pragma once

// UNE ENTRÉE DU CODEX DESSINÉE HORS DE SA FICHE : en couleur une fois vue, en
// silhouette sinon. Le Codex s'en sert pour ses fiches et ses liens, le choix de
// niveau pour les recettes d'une offre.
//
// En-tête PRIVÉ de `libs/ui/src`.

#include <imgui.h>

#include "ro/ui/skin_art.h"
#include "ro/ui/survivors_codex_window.h"

namespace ro::ui {

// L'image ramenée dans sa boîte, son rapport gardé, en couleur ou peinte en
// noir ; la boîte vide quand l'image manque.
void PaintCodexImage(const SkinImage& image, ImVec2 box, bool seen);

// L'icône du lien dans un carré de `icon_side`, puis son nom, ou sa silhouette
// et « ??? » tant qu'il n'a pas été vu.
void DrawCodexLink(const SurvivorsCodexLink& link, float icon_side);

}  // namespace ro::ui
