// LE TABLEAU DES CLASSEMENTS, dans la trame de l'interface : son dessin et sa
// place dans la pile d'Échap. À part de `ui_stage.cpp`, qui n'a plus la place
// de le porter.

#include <utility>

#include "ro/core/i18n.h"
#include "ro/ui/modal_box.h"

#include "game/ui_stage.h"

namespace moonclient {
namespace {

constexpr const char* kStarPlaceId = "designation_etoile";

}  // namespace

void UiStage::DrawRankingWindows(RankingBoardWindows& ranking) {
  if (ranking.view != nullptr) {
    ranking.answer = ro::ui::DrawRankingWindow(ranking.window, *ranking.view, skin_);
  }
  // LA QUESTION DE DÉSIGNATION : la boîte s'ouvre à son arrivée, et Entrée y
  // vaut OK.
  StarPlaceBox& star_place = ranking.star_place;
  if (std::exchange(star_place.ask, false)) {
    ro::ui::AskModal(kStarPlaceId);
  }
  if (!star_place.question.empty()) {
    const ro::ui::ModalAnswer agreed =
        ro::ui::DrawModalBox(kStarPlaceId, star_place.question.c_str(), ro::core::Tr("OK"),
                             ro::core::Tr("Annuler"), skin_, ro::ui::ModalEnter::kYes);
    if (agreed != ro::ui::ModalAnswer::kWaiting) {
      star_place.answer = agreed == ro::ui::ModalAnswer::kYes;
    }
  }
}

void UiStage::RegisterRankingEscapes(RankingBoardWindows& ranking) {
  // Échap baisse l'interrupteur : la fenêtre se ferme, sans un octet.
  if (ranking.view != nullptr) {
    escape_.Register(ro::ui::kRankingWindowId, &ranking.window.open);
  }
}

}  // namespace moonclient
