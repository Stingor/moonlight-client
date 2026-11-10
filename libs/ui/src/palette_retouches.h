#pragma once

#include "ro/ui/palette_editor.h"
#include "ro/ui/skin_art.h"

namespace ro::ui {

// LES RETOUCHES EXACTES : le mode qui fait viser au pantin une couleur de la
// palette, la liste des couleurs retouchées avec leur croix, et le nuancier de
// l'index visé.
//
// 🔴 UNE RETOUCHE PEINT UN INDEX, PAS UNE PIÈCE : la couleur choisie s'y pose
// telle quelle, par-dessus tout le reste. Rend vrai quand un nuancier est
// ouvert.
[[nodiscard]] bool DrawPaletteRetouches(PaletteEditorState& state, const PaletteEditorView& view,
                                        const SkinArt& art, PaletteEditorAction& action);

}  // namespace ro::ui
