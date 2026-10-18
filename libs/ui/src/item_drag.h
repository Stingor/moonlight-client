#pragma once

// L'OBJET LÂCHÉ SUR UNE CIBLE : la charge d'un objet qu'on tire, relue en
// entier. Une charge d'une autre taille n'est pas un objet.
//
// En-tête PRIVÉ de `libs/ui/src`. L'appel se fait entre `BeginDragDropTarget`
// et `EndDragDropTarget`, que le site garde.

#include <cstring>
#include <optional>

#include <imgui.h>

#include "ro/ui/shortcut_bar.h"

namespace ro::ui {

// L'objet lâché à l'instant, ou rien.
[[nodiscard]] inline std::optional<ItemDrag> AcceptItemDrag() {
  const ImGuiPayload* dropped = ImGui::AcceptDragDropPayload(kItemDragPayload);
  if (dropped == nullptr || dropped->DataSize != sizeof(ItemDrag)) {
    return std::nullopt;
  }
  ItemDrag carried;
  std::memcpy(&carried, dropped->Data, sizeof(carried));
  return carried;
}

}  // namespace ro::ui
