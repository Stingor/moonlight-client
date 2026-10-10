#include "ro/ui/chat_link_offer.h"

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/ui/skin_tooltip.h"

namespace ro::ui {

bool ShiftClickedLastItem() {
  const ImGuiIO& io = ImGui::GetIO();
  if (!io.KeyShift || !ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
    return false;
  }
  // ⚠ LA SEULE GARDE QU'ON GARDE D'IMGUI : une AUTRE fenêtre n'est pas
  // par-dessus. La géométrie seule poserait un lien à travers la conversation
  // posée sur le panneau. Celle-ci ne regarde que la fenêtre survolée, et
  // l'élément actif ne la trompe pas.
  if (!ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows |
                              ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)) {
    return false;
  }
  return ImGui::IsMouseHoveringRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
}

bool HoveredForLinkTooltip() {
  // ⚠ LE DÉLAI VIENT DU STYLE, d'où `ForTooltip` : une liste se survole sans
  // cesse, et ne doit pas clignoter au passage.
  return ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip |
                              ImGuiHoveredFlags_AllowWhenBlockedByActiveItem |
                              ImGuiHoveredFlags_NoNavOverride);
}

const char* ChatLinkGestureHint() { return core::Tr("Maj+clic : lien dans le chat"); }

bool OfferChatLink(bool offered) {
  if (!offered) {
    return false;
  }
  if (ShiftClickedLastItem()) {
    return true;
  }
  if (HoveredForLinkTooltip()) {
    SkinTooltipFormat("%s", ChatLinkGestureHint());
  }
  return false;
}

}  // namespace ro::ui
