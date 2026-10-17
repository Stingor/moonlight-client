#pragma once

#include <SDL3/SDL_events.h>

#include "world/map_editor_session.h"

namespace moonclient {

// LES TOUCHES DU MODE ÉDITEUR DE CARTE, avant celles du jeu, hors d'un champ de
// saisie : Suppr retire ou masque, Ctrl+flèches poussent, et les raccourcis de
// Ctrl — voir `MapEditorShortcut`. Échap n'y est pas : il garde son rôle du
// jeu, et l'éditeur se quitte par son bouton. Vrai quand la touche a
// servi à l'éditeur, et ne va donc pas au jeu.
[[nodiscard]] bool TakeMapEditorKey(MapEditorSession& editor, const SDL_KeyboardEvent& key,
                                    bool text_input);

}  // namespace moonclient
