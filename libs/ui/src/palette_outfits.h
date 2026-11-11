#pragma once

#include "ro/ui/palette_editor.h"
#include "ro/ui/skin_art.h"

namespace ro::ui {

// LES TENUES DE L'ÉDITEUR DE STYLE : le code court qui partage tout le style
// porté, et la garde-robe du compte que tous ses personnages relisent.
//
// 🔴 CE QUE LE SERVEUR A VALIDÉ : on publie et on range ce que le personnage
// PORTE, jamais un brouillon. Et une tenue reprise ne fait qu'une proposition,
// qu'il faudra valider comme le reste.
//
// ⚠ EN-TÊTE PRIVÉ : rien hors de `libs/ui/src` n'appelle ceci.

// Dessine la section repliable ; ce que le joueur y demande part dans `action`.
void DrawPaletteOutfits(PaletteEditorState& state, const PaletteEditorView& view,
                        const SkinArt& art, PaletteEditorAction& action);

}  // namespace ro::ui
