#pragma once

#include <array>

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
// un réglage au curseur, une pastille glissée sur une autre, un verrou. La
// liste ne touche pas à la recette.
[[nodiscard]] PiecesAnswer DrawPalettePieces(PaletteEditorState& state,
                                             const PaletteEditorView& view, const SkinArt& art,
                                             PaletteEditorAction& action);

// LE CHOIX D'UNE COULEUR, À L'ŒIL OU AU CODE : le carré et sa barre de teinte,
// puis le champ de son code `0xRRGGBB`, qui se copie et se remplace. Sans
// curseur : pour un nuancier qui porte les siens — une pièce, un effet —, où
// ceux de la couleur visée écriraient deux fois la teinte et la saturation.
// Rend vrai quand la couleur a changé.
[[nodiscard]] bool DrawPaletteColorChoice(std::array<float, 3>& rgb);

// LE NUANCIER D'UNE COULEUR EXACTE : ce même choix, puis la teinte, la
// saturation et la clarté de cette couleur en curseurs. Rend vrai quand la
// couleur a changé.
[[nodiscard]] bool DrawPaletteColorPicker(std::array<float, 3>& rgb, const SkinArt& art);

}  // namespace ro::ui
