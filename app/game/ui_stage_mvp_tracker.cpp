// LES PIÈCES DU CARNET DE CHASSE MVP, dans la trame de l'interface : la
// fenêtre et ses surcouches, l'aperçu d'un `<MVPL>` survolé, et la place de la
// fenêtre dans la pile d'Échap. À part de `ui_stage.cpp`, qui n'a plus la place
// de les porter.

#include "game/ui_stage.h"

namespace moonclient {

void UiStage::DrawMvpTrackerWindows(MvpTrackerWindows& mvp) {
  // ⚠ DESSINÉ MÊME FERMÉ : l'alerte, les lignes détachées et l'invitation
  // reçue vivent hors de la fenêtre.
  if (mvp.view != nullptr) {
    mvp.answer = ro::ui::DrawMvpTrackerWindow(mvp.window, *mvp.view, skin_);
  }
  // L'aperçu ne vaut que pour l'image où le lien est survolé.
  if (mvp.link_tip.has_value()) {
    ro::ui::DrawMvpLinkTip(*mvp.link_tip);
    mvp.link_tip.reset();
  }
}

void UiStage::RegisterMvpTrackerEscapes(MvpTrackerWindows& mvp) {
  // Épinglée, Échap ne la ferme plus ; sinon elle se ferme sans un octet.
  if (mvp.window.pinned) {
    escape_.SkipNext();
  }
  escape_.Register(ro::ui::kMvpTrackerWindowId, &mvp.window.open);
}

}  // namespace moonclient
