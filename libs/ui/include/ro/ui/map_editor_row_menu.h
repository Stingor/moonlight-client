#pragma once

#include <array>
#include <optional>
#include <vector>

#include "ro/ui/map_editor_panel.h"

namespace ro::ui {

// LE MENU DU CLIC DROIT D'UNE LIGNE DE LISTE de l'éditeur de carte — un
// modèle de l'arbre de la scène, une lumière, un effet. Il désigne la ligne,
// puis agit sur elle par les intentions que la session connaît déjà, qui
// toutes visent ce qui est désigné.
struct MapEditorRowMenu {
  // Ce qui désigne la ligne.
  MapEditorIntent select;
  // « Supprimer », « Masquer » ou « Afficher », déjà traduit ; nul : pas de
  // ligne. Voir `map_editor::Delete`.
  const char* remove_label = nullptr;
  // Rendre l'objet à la carte ; rien : pas de ligne.
  std::optional<MapEditorIntent> revert;
  // Sa position en cases, pour « Copier la position ».
  std::array<float, 3> cells{};
};

// Le menu de la ligne que l'on vient de dessiner, s'il est ouvert ; ses choix
// rejoignent `intents`.
void DrawMapEditorRowMenu(const MapEditorRowMenu& menu, std::vector<MapEditorIntent>& intents);

}  // namespace ro::ui
