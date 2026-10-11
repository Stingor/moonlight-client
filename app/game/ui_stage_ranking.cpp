// LE TABLEAU DES CLASSEMENTS, dans la trame de l'interface : son dessin et sa
// place dans la pile d'Échap. À part de `ui_stage.cpp`, qui n'a plus la place
// de le porter.

#include "game/ui_stage.h"

namespace moonclient {

void UiStage::DrawRankingWindows(RankingBoardWindows& ranking) {
  if (ranking.view != nullptr) {
    ranking.answer = ro::ui::DrawRankingWindow(ranking.window, *ranking.view, skin_);
  }
}

void UiStage::RegisterRankingEscapes(RankingBoardWindows& ranking) {
  // Échap baisse l'interrupteur : la fenêtre se ferme, sans un octet.
  if (ranking.view != nullptr) {
    escape_.Register(ro::ui::kRankingWindowId, &ranking.window.open);
  }
}

}  // namespace moonclient
