// La cellule d'un objet cliquable. Voir l'en-tête.

#include "ro/ui/item_link_cell.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>

namespace ro::ui {
namespace {

constexpr ImVec4 kNoTint(1.0F, 1.0F, 1.0F, 1.0F);

// L'icône, ajustée sans déformation dans son carré et centrée. Sans icône, la
// place se garde ou non.
void DrawIcon(const ItemLinkCell& cell, float side) {
  const bool drawn = cell.icon.usable() && cell.icon.width > 0.0F && cell.icon.height > 0.0F;
  if (!drawn && !cell.keep_icon_place) {
    return;
  }
  if (drawn) {
    const float fit = std::min(side / cell.icon.width, side / cell.icon.height);
    const ImVec2 size(cell.icon.width * fit, cell.icon.height * fit);
    const ImVec2 corner = ImGui::GetCursorScreenPos();
    const ImVec2 least(std::floor(corner.x + (side - size.x) * 0.5F),
                       std::floor(corner.y + (side - size.y) * 0.5F));
    const ImU32 tint = ImGui::ColorConvertFloat4ToU32(
        cell.damaged ? ImGui::ColorConvertU32ToFloat4(kChatDamagedShadow) : kNoTint);
    ImGui::GetWindowDrawList()->AddImage(cell.icon.id, least,
                                         ImVec2(least.x + size.x, least.y + size.y),
                                         ImVec2(0.0F, 0.0F), ImVec2(1.0F, 1.0F), tint);
  }
  ImGui::Dummy(ImVec2(side, side));
  ImGui::SameLine(0.0F, ImGui::GetStyle().ItemInnerSpacing.x);
}

// La couleur du nom : la sienne, ou — nulle — celle des liens de l'habillage.
[[nodiscard]] ImVec4 InkOf(const ItemLinkCell& cell) {
  return cell.color != 0 ? ImGui::ColorConvertU32ToFloat4(cell.color)
                         : ImGui::GetStyleColorVec4(ImGuiCol_TextLink);
}

// Le nom, l'ombre rouge d'un objet cassé dessous.
void DrawName(const ItemLinkCell& cell) {
  const char* begin = cell.text.data();
  const char* end = begin + cell.text.size();
  if (cell.damaged) {
    const ImVec2 at = ImGui::GetCursorScreenPos();
    ImGui::GetWindowDrawList()->AddText(
        ImVec2(at.x + kChatDamagedShadowOffset, at.y + kChatDamagedShadowOffset),
        kChatDamagedShadow, begin, end);
  }
  ImGui::PushStyleColor(ImGuiCol_Text, InkOf(cell));
  ImGui::TextUnformatted(begin, end);
  ImGui::PopStyleColor();
}

}  // namespace

ChatLinkGesture DrawItemLinkCell(const ItemLinkCell& cell, ItemLinkMenu& menu, ChatLinkAsk& asked) {
  ImGui::PushID(static_cast<int>(cell.item_id));
  ImGui::PushID(static_cast<int>(cell.token));
  const float line = ImGui::GetTextLineHeight();
  const float side = cell.icon_side > 0.0F ? cell.icon_side : line;
  ImGui::BeginGroup();
  DrawIcon(cell, side);
  if (side > line && (cell.icon.usable() || cell.keep_icon_place)) {
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + std::floor((side - line) * 0.5F));
  }
  DrawName(cell);
  ImGui::EndGroup();

  const bool hovered = ImGui::IsItemHovered();
  if (hovered || cell.underlined) {
    // Souligné au survol, comme tout lien de l'interface.
    const ImVec2 least = ImGui::GetItemRectMin();
    const ImVec2 most = ImGui::GetItemRectMax();
    ImGui::GetWindowDrawList()->AddLine(ImVec2(least.x, most.y), most,
                                        ImGui::ColorConvertFloat4ToU32(InkOf(cell)));
  }
  if (hovered) {
    asked.hovered_id = cell.item_id;
    asked.hovered_kind = cell.kind;
    asked.hovered_token = cell.token;
    asked.hovering = true;
  }
  const ChatLinkGesture gesture = HitChatLink(hovered);
  switch (gesture) {
    case ChatLinkGesture::kDescribe:
    case ChatLinkGesture::kRepost:
      asked.gesture = gesture;
      asked.kind = cell.kind;
      asked.item_id = cell.item_id;
      asked.token = cell.token;
      break;
    case ChatLinkGesture::kMenu:
      menu.armed = true;
      menu.kind = cell.kind;
      menu.item_id = cell.item_id;
      menu.token = cell.token;
      menu.label = std::string(cell.menu_label.empty() ? cell.text : cell.menu_label);
      break;
    default:
      break;
  }
  ImGui::PopID();
  ImGui::PopID();
  return gesture;
}

void DrawItemLinkMenu(const char* popup_id, ItemLinkMenu& menu, ChatLinkAsk& asked,
                      std::span<const VoteShopOffer> vote_shop) {
  if (std::exchange(menu.describe_at_once, false)) {
    asked.gesture = ChatLinkGesture::kDescribe;
    asked.kind = menu.kind;
    asked.item_id = menu.item_id;
    asked.token = menu.token;
  }
  if (menu.armed) {
    menu.armed = false;
    ImGui::OpenPopup(popup_id);
  }
  const ChatLinkAsk chosen =
      DrawChatLinkMenu(popup_id, menu.kind, menu.item_id, menu.token, menu.label, vote_shop);
  if (chosen.gesture != ChatLinkGesture::kNothing) {
    asked.gesture = chosen.gesture;
    asked.kind = chosen.kind;
    asked.item_id = chosen.item_id;
    asked.token = chosen.token;
  }
}

}  // namespace ro::ui
