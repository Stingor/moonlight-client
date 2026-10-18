#pragma once

// LE GESTE SUR UNE LIGNE DE COMMERCE, commun aux fenêtres de troc, d'achat et
// d'échoppe : le survol se note, puis le geste de lien d'objet se joue, et un
// Maj+clic demande la description et le relais du lien.
//
// En-tête PRIVÉ de `libs/ui/src`. Les fenêtres ne diffèrent que par leur action
// et par ce qui désigne l'objet : son identifiant de catalogue, ou sa place.
// L'infobulle des empêchements est la même partout.

#include <cstdint>
#include <span>
#include <string_view>

#include <imgui.h>

#include "ro/ui/chat_link.h"
#include "ro/ui/item_link_cell.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/trade_widgets.h"

#include "ui_strings.h"

namespace ro::ui {

// ⚠ L'OBJET SE DÉSIGNE PAR `item_id`, celui de la ligne : le jeton du menu reste
// nul, un objet de troc ou d'achat n'est à personne.
template <typename Action>
void NoteItemById(const TradeItemGesture& gesture, const TradeItemView& item, std::uint32_t item_id,
                  ItemLinkMenu& menu, Action& action) {
  if (gesture.hovered) {
    action.hovered_id = item_id;
  }
  TradeItemView aimed = item;
  aimed.item_id = item_id;
  if (NoteTradeItemLink(gesture, aimed, 0, menu)) {
    action.describe_id = item_id;
    action.item_gesture = ChatLinkGesture::kRepost;
  }
}

// L'objet se désigne par sa place dans le chariot ou dans l'échoppe.
template <typename Action>
void NoteItemBySlot(const TradeItemGesture& gesture, const TradeItemView& item, std::uint16_t slot,
                    ItemLinkMenu& menu, Action& action) {
  if (gesture.hovered) {
    action.hovered_slot = slot;
  }
  if (NoteTradeItemLink(gesture, item, slot, menu)) {
    action.describe_slot = slot;
    action.item_gesture = ChatLinkGesture::kRepost;
  }
}

// CE QUI EMPÊCHE LE BOUTON QU'ON VIENT DE POSER, une raison par ligne, au
// survol — même grisé. Rien quand rien n'empêche.
inline void TipWhenHeld(std::span<const std::string_view> reasons) {
  if (!reasons.empty() && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    SkinTooltip(JoinedLines(reasons).c_str());
  }
}

}  // namespace ro::ui
