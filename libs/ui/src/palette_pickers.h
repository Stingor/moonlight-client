#pragma once

#include "ro/ui/palette_editor.h"
#include "ro/ui/skin_art.h"

namespace ro::ui {

// LES TROIS SÉLECTEURS DE L'ÉDITEUR DE STYLE : coiffure, couleur de cheveux,
// teinte de base — leurs boutons et les grilles qui surgissent dessous.
//
// ⚠ À PART DE LA FENÊTRE, et pour une raison de TAILLE plutôt que de
// frontière : une grille de vignettes est un dessin à elle seule — géométrie,
// pagination, tranche visible, garde de l'ascenseur — et la fenêtre qui
// l'héberge a ses propres huit cents lignes à tenir.
//
// ⚠ EN-TÊTE PRIVÉ : rien hors de `libs/ui/src` n'appelle ceci. Ce que `app`
// voit de ces grilles tient dans `PalettePickerState` et les vignettes de la
// vue.

// Dessine les trois boutons, chacun portant son choix courant, et les grilles
// qu'ils ouvrent.
//
// ⚠ ELLE ÉCRIT DANS `state.picker` la grille ouverte et la tranche de numéros
// réellement visible : c'est ce que `app` composera à l'image suivante.
void DrawChoiceButtons(PaletteEditorState& state, const PaletteEditorView& view,
                       const SkinArt& art);

}  // namespace ro::ui
