// LES FENÊTRES DU CHAMP DE BATAILLE, dans la trame de l'interface : leur dessin
// et leur place dans la pile d'Échap. À part de `ui_stage.cpp`, qui n'a plus la
// place de les porter.

#include "game/ui_stage.h"

namespace moonclient {

void UiStage::DrawBattlegroundWindows(BattlegroundWindows& battleground) {
  // ⚠ DESSINÉ MÊME FERMÉ : l'invitation et la boîte de résultat s'imposent hors
  // de toute fenêtre.
  if (battleground.view != nullptr) {
    battleground.answer =
        ro::ui::DrawEntryQueueWindows(battleground.window, *battleground.view, skin_);
  }
}

void UiStage::RegisterBattlegroundEscapes(BattlegroundWindows& battleground) {
  // L'aide se lit et se referme. « En attente » se ferme sans quitter la file.
  // ⚠ « TERRAIN PRÊT » N'Y EST PAS : une invitation ne disparaît pas d'une
  // touche tapée pour autre chose.
  escape_.RegisterReading(ro::ui::kEntryQueueHelpId, &battleground.window.help_open);
  escape_.Register(ro::ui::kEntryQueueStandbyId, &battleground.window.standby_open);
  escape_.Register(ro::ui::kEntryQueueWindowId, &battleground.window.open);
}

}  // namespace moonclient
