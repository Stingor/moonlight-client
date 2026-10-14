#pragma once

// LA FENÊTRE « BASE DE DONNÉES », telle que les fenêtres de l'interface la
// portent : à part de son étage, pour que l'interface la tienne sans le
// connaître. Voir `game/database_stage.h` et l'ADR 0116.

#include "ro/ui/database_window.h"

namespace moonclient {

struct DatabaseWindows {
  ro::ui::DatabaseWindowState state;
  // Nulle : rien ne se dessine.
  const ro::ui::DatabaseView* view = nullptr;
  ro::ui::DatabaseAnswer answer;
  // L'ICÔNE DU MENU ET LA TOUCHE, qui n'atteignent que les fenêtres : leur
  // bascule se dépose ici, et l'étage la joue à l'image suivante.
  bool toggle_asked = false;
};

}  // namespace moonclient
