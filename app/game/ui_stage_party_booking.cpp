// LA RECHERCHE DE GROUPE, dans la trame de l'interface : son dessin et sa place
// dans la pile d'Échap. À part de `ui_stage.cpp`, qui n'a plus la place de la
// porter.

#include "game/ui_stage.h"

namespace moonclient {

void UiStage::DrawPartyBookingWindows(PartyBookingWindows& booking) {
  if (booking.view != nullptr) {
    booking.answer = ro::ui::DrawPartyBookingWindow(booking.window, *booking.view, skin_);
  }
}

void UiStage::RegisterPartyBookingEscapes(PartyBookingWindows& booking) {
  // Échap baisse l'interrupteur : la fenêtre se ferme, sans un octet.
  if (booking.view != nullptr) {
    escape_.Register(ro::ui::kPartyBookingWindowId, &booking.window.open);
  }
}

}  // namespace moonclient
