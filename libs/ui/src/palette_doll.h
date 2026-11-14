#pragma once

#include <functional>

#include "ro/ui/palette_editor.h"
#include "ro/ui/skin_art.h"

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
// `pipette` : un clic y choisit une pièce, et la bulle le dit.
void DrawPaletteDoll(PaletteDollState& doll, float side, const SkinLook& look,
                     bool pipette = false);

// SES COMMANDES, sous lui dans l'éditeur : avant/après, rotation, pose et son
// animation, fond, loupe.
//
// `differs` : ce qu'on compose diffère de ce que le personnage porte ; sans
// quoi « Avant » n'aurait rien à montrer, et se grise. `between` se dessine
// sur la même ligne, entre « Avant » et la rotation : l'historique de
// l'éditeur.
void DrawPaletteDollControls(PaletteDollState& doll, const SkinArt& art, bool differs,
                             const std::function<void()>& between = {});

}  // namespace ro::ui
