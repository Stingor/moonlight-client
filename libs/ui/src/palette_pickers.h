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

// Dessine les trois boutons, chacun portant le choix de la recette, et les
// grilles qu'ils ouvrent. Une case choisie part dans `action`, jamais dans la
// recette : `app` seul l'écrit.
//
// ⚠ ELLE ÉCRIT DANS `state.picker` la grille ouverte et la tranche de numéros
// réellement visible : c'est ce que `app` composera à l'image suivante.
//
// 🔴 CHAQUE ONGLET A LES SIENS : la coupe et la couleur sur l'onglet des
// cheveux, la teinte de base sur celui du corps — qui reprend les trois quand
// les cheveux n'ont pas d'onglet.
struct ChoiceButtons {
  bool hair = true;
  bool body_palette = true;
};
void DrawChoiceButtons(PaletteEditorState& state, const PaletteEditorView& view, const SkinArt& art,
                       PaletteEditorAction& action, ChoiceButtons shown);
// La coupe et la couleur de cheveux seules.
void DrawHairChoices(PaletteEditorState& state, const PaletteEditorView& view, const SkinArt& art,
                     PaletteEditorAction& action);

}  // namespace ro::ui
