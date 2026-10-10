#pragma once

#include <optional>

#include "ro/ui/navigation_window.h"
#include "ro/ui/world_map_window.h"

namespace moonclient {

// LA NAVIGATION ET LA CARTE DU MONDE dans la trame de l'interface : un état, une
// vue composée par la partie, une réponse rendue. Vue nulle, rien ne se dessine.
struct NavigationWindows {
  ro::ui::NavigationWindowState state;
  const ro::ui::NavigationView* view = nullptr;
  ro::ui::NavigationAnswer answer;
  // L'infobulle d'un lieu de chat survolé, dessinée une image puis rendue.
  std::optional<ro::ui::NavigationPlaceTip> place_tip;
  // De même, celle d'une recherche de chat survolée.
  std::optional<ro::ui::NavigationSearchTip> search_tip;
  // LA CARTE DU MONDE, que la minimap ouvre.
  ro::ui::WorldMapWindowState world_map;
  const ro::ui::WorldMapView* world_map_view = nullptr;
  ro::ui::WorldMapAction world_map_answer;
};

}  // namespace moonclient
