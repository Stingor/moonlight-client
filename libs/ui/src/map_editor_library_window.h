#pragma once

#include <string>
#include <vector>

#include "ro/ui/map_editor_panel.h"
#include "ro/ui/skin_art.h"

namespace ro::ui {

// LA FENÊTRE « BIBLIOTHÈQUE DE MODÈLES » : la grille de vignettes, l'arbre des
// dossiers, les favoris et les récents. Ne dessine rien quand elle est fermée,
// mais refait toujours ce qu'elle rend à l'appelant.
void DrawModelLibraryWindow(const MapEditorView& view, MapEditorPanelState& panel,
                            std::vector<MapEditorIntent>& intents, const SkinArt& art);

// Vide ce que la fenêtre rend à l'appelant : le mode est fermé.
void ForgetModelLibraryWishes(ModelLibraryWindowState& window);

// L'APERÇU AU SURVOL de l'élément qu'on vient de poser : le fichier survolé est
// noté pour l'appelant, et la bulle montre sa vignette si la vue la porte —
// sinon qu'elle vient, ou que le fichier ne se lit pas.
void PreviewOnHover(const MapEditorView& view, MapEditorPanelState& panel, const std::string& file);

}  // namespace ro::ui
