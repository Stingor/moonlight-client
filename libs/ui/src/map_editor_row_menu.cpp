#include "ro/ui/map_editor_row_menu.h"

#include <array>
#include <optional>
#include <utility>

#include "ro/core/i18n.h"
#include "ro/ui/skin_popup.h"
#include "ro/ui/triple_clipboard.h"

#include "imgui.h"

namespace ro::ui {

void DrawMapEditorRowMenu(const MapEditorRowMenu& menu, std::vector<MapEditorIntent>& intents) {
  // Le menu porte l'identifiant de SA ligne : un nom commun ferait d'un clic
  // droit le menu de toutes les lignes visibles, lumières et effets compris.
  if (!BeginSkinPopupContextItem(nullptr)) {
    return;
  }
  // Désigner, puis agir : la session joue les intentions dans l'ordre.
  const auto act = [&](MapEditorIntent action) {
    intents.push_back(menu.select);
    intents.push_back(std::move(action));
  };
  if (ImGui::MenuItem(core::Tr("Dupliquer"))) {
    act(map_editor::Duplicate{});
  }
  if (menu.remove_label != nullptr && ImGui::MenuItem(menu.remove_label)) {
    act(map_editor::Delete{});
  }
  if (menu.revert.has_value() && ImGui::MenuItem(core::Tr("Rétablir"))) {
    act(*menu.revert);
  }
  ImGui::Separator();
  if (ImGui::MenuItem(core::Tr("Centrer"))) {
    act(map_editor::Focus{});
  }
  if (ImGui::MenuItem(core::Tr("Copier la position"), nullptr, false, menu.cells.has_value())) {
    ImGui::SetClipboardText(TripleText(*menu.cells).c_str());
  }
  const std::optional<std::array<float, 3>> pasted = ClipboardTriple();
  if (ImGui::MenuItem(core::Tr("Coller la position"), nullptr, false,
                      menu.move_to != nullptr && pasted.has_value())) {
    act(menu.move_to(*pasted));
  }
  EndSkinPopup();
}

}  // namespace ro::ui
