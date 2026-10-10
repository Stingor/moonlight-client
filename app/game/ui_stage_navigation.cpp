// LA FENÊTRE DE NAVIGATION dans la trame de l'interface, les deux infobulles
// qu'un lien du fil lui emprunte — celle d'un lieu et celle d'une recherche —,
// et la carte du monde.

#include "game/ui_stage.h"

namespace moonclient {

void UiStage::DrawNavigationWindows(Windows::Navigation& navigation) {
  if (navigation.view != nullptr) {
    navigation.answer = ro::ui::DrawNavigationWindow(navigation.state, *navigation.view, skin_);
  }
  // Une infobulle ne vaut que pour l'image où son lien est survolé.
  if (navigation.place_tip.has_value()) {
    ro::ui::DrawNavigationPlaceTip(*navigation.place_tip);
    navigation.place_tip.reset();
  }
  if (navigation.search_tip.has_value()) {
    ro::ui::DrawNavigationSearchTip(*navigation.search_tip);
    navigation.search_tip.reset();
  }
  if (navigation.world_map_view != nullptr) {
    navigation.world_map_answer =
        ro::ui::DrawWorldMapWindow(navigation.world_map, *navigation.world_map_view, skin_);
  }
}

// La navigation et la carte du monde se consultent, puis se referment.
void UiStage::RegisterNavigationEscapes(Windows::Navigation& navigation) {
  escape_.Register(ro::ui::kNavigationWindowId, &navigation.state.open);
  escape_.Register(ro::ui::kWorldMapWindowId, &navigation.world_map.open);
}

}  // namespace moonclient
