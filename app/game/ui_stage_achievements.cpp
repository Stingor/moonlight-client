// LES PIÈCES DES SUCCÈS, dans la trame de l'interface : la fenêtre, la
// bannière, et la place de la fenêtre dans la pile d'Échap. Le panneau de suivi,
// commun aux quêtes, se dessine dans `ui_stage_quests.cpp`. À part de
// `ui_stage.cpp`, qui n'a plus la place de les porter.

#include "game/ui_stage.h"

namespace moonclient {

void UiStage::DrawAchievementWindows(AchievementWindows& achievements) {
  // LA BANNIÈRE D'ABORD : la fenêtre, quand on l'ouvre, passe par-dessus.
  ro::ui::DrawAchievementBanner(achievements.banner, achievements.banner_style,
                                achievements.banner_elapsed_ms);
  if (achievements.view != nullptr) {
    achievements.answer =
        ro::ui::DrawAchievementWindow(achievements.window, *achievements.view, skin_);
  }
}

void UiStage::RegisterAchievementEscapes(AchievementWindows& achievements) {
  // Échap baisse l'interrupteur : la fenêtre se ferme, sans un octet.
  if (achievements.view != nullptr) {
    escape_.Register(ro::ui::kAchievementWindowId, &achievements.window.open);
  }
}

}  // namespace moonclient
