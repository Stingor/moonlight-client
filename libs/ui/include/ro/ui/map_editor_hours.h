#pragma once

namespace ro::ui {

// LA PLAGE D'HEURES d'un objet « la nuit seulement », telle que le panneau de
// l'éditeur de carte la montre et la rend : choisie ou non, et ses bornes en
// heures du serveur.
struct MapEditorHours {
  bool chosen = false;
  float from_hour = 0.0F;
  float to_hour = 0.0F;
};

}  // namespace ro::ui
