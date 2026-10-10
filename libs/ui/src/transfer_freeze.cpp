// Les transferts gelés, vus des fenêtres. Voir l'en-tête.

#include "transfer_freeze.h"

#include <imgui.h>

#include "ro/core/i18n.h"

#include "item_grid.h"

namespace ro::ui {

const char* TransfersFrozenText() {
  return core::Tr("Échoppe en préparation : les transferts sont gelés.");
}

void DrawTransfersFrozenBanner(bool frozen) {
  if (frozen) {
    ImGui::TextColored(kDragRefusalColor, "%s", TransfersFrozenText());
  }
}

}  // namespace ro::ui
