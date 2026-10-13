#include "ro/ui/doll_overlay.h"

namespace ro::ui {

void PaintDollOverlay(const DollOverlay& overlay, const DollOverlayCall& call) {
  ImDrawList* into = ImGui::GetWindowDrawList();
  if (overlay.paint == nullptr || into == nullptr) {
    return;
  }
  // ⚠ LA TAILLE PASSÉE : la liste COPIE l'appel, qui ne vit pas jusqu'au dessin.
  into->AddCallback(overlay.paint, const_cast<DollOverlayCall*>(&call), sizeof(call));
  into->AddCallback(ImDrawCallback_ResetRenderState, nullptr);
}

}  // namespace ro::ui
