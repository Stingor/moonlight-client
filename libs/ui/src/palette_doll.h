#pragma once

#include "ro/ui/palette_editor.h"

namespace ro::ui {

// LE PANTIN D'UN STYLE, et les deux fenêtres qui le montrent.
//
// ⚠ PARTAGÉ PLUTÔT QUE RECOPIÉ, et ce n'est pas de l'économie de lignes :
// l'éditeur et l'aperçu d'un style reçu montrent LA MÊME CHOSE — notre corps,
// peint d'une recette, qu'on tourne à la molette. Deux dessins divergeraient
// au premier réglage retouché d'un seul côté, et le joueur comparerait alors
// deux pantins qui ne se ressemblent plus.
//
// 🔴 ET LA BOÎTE EST MESURÉE ICI, LUE PAR `app` À L'IMAGE SUIVANTE : c'est
// elle qui dit à quelle taille composer, et elle dépend d'une mise en page
// qu'on ne connaît qu'au dessin.
void DrawPaletteDoll(PaletteDollState& doll, float side, const SkinLook& look);

}  // namespace ro::ui
