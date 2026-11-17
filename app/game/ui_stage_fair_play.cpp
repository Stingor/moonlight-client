// LE RAPPEL DE FAIR-PLAY EN GUERRE DE GUILDE, dans la trame de l'interface : une
// boîte modale qui dit, et qu'un seul bouton referme. À part de `ui_stage.cpp`,
// qui n'a plus la place de la porter.

#include <utility>

#include "ro/core/i18n.h"
#include "ro/ui/modal_box.h"

#include "game/ui_stage.h"
#include "game/ui_windows.h"

namespace moonclient {
namespace {

constexpr const char* kFairPlayId = "rappel_fair_play";

}  // namespace

void UiStage::DrawFairPlayNotice(FairPlayBox& box) {
  if (std::exchange(box.ask, false)) {
    ro::ui::AskModal(kFairPlayId);
  }
  if (box.text.empty()) {
    return;
  }
  if (ro::ui::DrawNoticeBox(kFairPlayId, box.text.c_str(), ro::core::Tr("J'ai compris"), skin_)) {
    box.text.clear();
  }
}

}  // namespace moonclient
