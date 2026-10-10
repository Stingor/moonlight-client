// LES QUÊTES ET LE PANNEAU DE SUIVI, dans la trame de l'interface : le
// journal, les panneaux de suivi communs aux succès, et la place du journal dans
// la pile d'Échap. À part de `ui_stage.cpp`, qui n'a plus la place de les porter.

#include "game/tracker_stage.h"
#include "game/ui_stage.h"

namespace moonclient {

void UiStage::DrawQuestWindows(QuestWindows& quests, TrackerWindows& tracker) {
  // LES PANNEAUX D'ABORD : le journal, quand on l'ouvre, passe par-dessus.
  constexpr std::array<const char*, kTrackerPanelCount> kPanelIds = {ro::ui::kTrackerPanelId,
                                                                     ro::ui::kTrackerSecondPanelId};
  for (std::size_t panel = 0; panel < kTrackerPanelCount; ++panel) {
    tracker.answers[panel] =
        ro::ui::DrawTrackerPanel(kPanelIds[panel], tracker.panels[panel], tracker.sections[panel],
                                 tracker.look, TrackerPanelFirstTop(panel), skin_);
  }
  if (quests.view != nullptr && quests.art != nullptr) {
    quests.answer = ro::ui::DrawQuestWindow(quests.window, *quests.view, *quests.art, skin_);
  }
}

void UiStage::RegisterQuestEscapes(QuestWindows& quests) {
  // Échap baisse l'interrupteur : la fenêtre se ferme, sans un octet.
  if (quests.view != nullptr) {
    escape_.Register(ro::ui::kQuestWindowId, &quests.window.open);
  }
}

}  // namespace moonclient
