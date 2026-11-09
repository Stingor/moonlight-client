#pragma once

#include "ro/ui/palette_editor.h"
#include "ro/ui/skin_art.h"

namespace ro::ui {

// Ce que la liste des pièces a relevé.
struct PiecesAnswer {
  // La pièce que la souris survole, ou moins un.
  int hovered = -1;
  // Un nuancier est ouvert : ce qui suit la liste passe dessous.
  bool picker_open = false;
};

// LA LISTE DES PIÈCES : une pastille par pièce, son nom, sa croix.
//
// 🔴 CHAQUE GESTE DEVIENT UNE INTENTION dans `action` : une couleur désignée,
// un réglage avancé, une pastille glissée sur une autre, un verrou. La liste
// ne touche pas à la recette.
[[nodiscard]] PiecesAnswer DrawPalettePieces(PaletteEditorState& state,
                                             const PaletteEditorView& view, const SkinArt& art,
                                             PaletteEditorAction& action);

}  // namespace ro::ui
