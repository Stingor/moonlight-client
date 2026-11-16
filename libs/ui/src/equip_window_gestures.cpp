#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <optional>
#include <span>
#include <string>

#include <imgui.h>
#include <imgui_internal.h>

#include "ro/core/i18n.h"
#include "ro/ui/equip_window.h"
#include "ro/ui/shortcut_bar.h"
#include "ro/ui/skin_popup.h"
#include "ro/ui/skin_tooltip.h"
#include "ro/ui/skin_widgets.h"

#include "achievement_paint.h"
#include "equip_window_parts.h"
#include "ui_scale.h"
#include "ui_text.h"

namespace ro::ui::equip_parts {

using achievement::kBlack;
using achievement::kOnRelease;
using render::SkinPiece;

// ── LES BOUTONS ─────────────────────────────────────────────────────────────

// Un bouton peint : son image au repos, une autre survolée ou enfoncée. Rend
// vrai au relâché sur lui.
[[nodiscard]] achievement::HitResult PaintedButton(const Painter& paint, const char* id,
                                                   const SkinImage& out, const SkinImage& over,
                                                   float left, float top) {
  const achievement::HitResult hit = paint.Hit(id, left, top, out.width, out.height, kOnRelease);
  paint.Art(hit.hovered || hit.held ? over : out, left, top);
  return hit;
}

// LA BULLE D'UNE PLACE, posée sur sa rangée comme le jeu la pose.
void ShowPlaceTip(const Painter& paint, const EquipWindowLayout& layout, ImVec2 mouse,
                  const EquipWindowPiece& shown) {
  std::array<char, 256> text{};
  const std::string name(shown.piece.name);
  if (shown.piece.amount < 2) {
    if (shown.option_count != 0) {
      std::snprintf(text.data(), text.size(), core::Tr("%s [%d option(s)]"), name.c_str(),
                    shown.option_count);
    } else {
      std::snprintf(text.data(), text.size(), "%s", name.c_str());
    }
  } else if (shown.option_count != 0) {
    std::snprintf(text.data(), text.size(), core::Tr("%s [%d option(s)] : %d exemplaires"),
                  name.c_str(), shown.option_count, shown.piece.amount);
  } else {
    std::snprintf(text.data(), text.size(), core::Tr("%s : %d ex."), name.c_str(),
                  shown.piece.amount);
  }
  float left = kTipLeft;
  if (mouse.x >= kRightEntryStart) {
    left = kTipRight;
  } else if (mouse.x >= kLeftEntryEnd) {
    left = CenterOf(layout) - kTipFromCenter;
  }
  const float top =
      std::floor((mouse.y - kZoneTop) / kTipRowPitch) * kRowPitch - kTipLift + kContentTop;
  ImGui::SetNextWindowPos(paint.At(left, top));
  if (BeginSkinTooltip()) {
    Unformatted(text.data());
    EndSkinTooltip();
  }
}

// ── LES GESTES ──────────────────────────────────────────────────────────────

// LE HAUT DE LA BANDE DE CLIC d'une place : la rangée entière, telle que la
// zone de clic la découpe.
[[nodiscard]] float BandTop(std::size_t place) {
  if (place >= kFirstLeftPlace) {
    return kZoneTop;
  }
  return kZoneTop + kRowPitch * static_cast<float>(place % kRowsPerColumn + 1);
}

// LE FANTÔME D'UN GLISSER : l'icône, puis le nom.
void PaintDragGhost(const EquipSlotView& piece) {
  if (piece.icon.usable()) {
    const ImVec2 at = ImGui::GetCursorScreenPos();
    const float side = kIconPixels * Scale();
    ImGui::Dummy(ImVec2(side, side));
    ImGui::GetWindowDrawList()->AddImage(piece.icon.id, at, ImVec2(at.x + side, at.y + side));
    ImGui::SameLine();
  }
  Unformatted(piece.name);
}

// LES GESTES D'UNE ICÔNE : la prendre, l'ôter d'un double-clic, la lier d'un
// Maj + clic. ⚠ L'ICÔNE SEULE EN EST LA PRISE, comme dans le jeu.
void IconGesture(const Painter& paint, const EquipWindowPiece& shown, std::size_t place,
                 ImVec2 top_left, ImVec2 size, EquipWindowAction& action) {
  if (!shown.piece.filled()) {
    return;
  }
  std::array<char, 16> id{};
  std::snprintf(id.data(), id.size(), "##icone%zu", place);
  const achievement::HitResult hit = paint.Hit(id.data(), top_left.x, top_left.y, size.x, size.y);
  if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
    const ItemDrag carried{.item_id = shown.piece.item_id,
                           .client_slot = shown.piece.client_slot,
                           .from = ItemHolder::kEquipment,
                           .worn = true};
    ImGui::SetDragDropPayload(kItemDragPayload, &carried, sizeof(carried));
    PaintDragGhost(shown.piece);
    ImGui::EndDragDropSource();
  }
  if (!hit.hovered) {
    return;
  }
  if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && ImGui::GetIO().KeyShift) {
    action.sheet.link_slot = shown.piece.client_slot;
  }
  if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && !ImGui::GetIO().KeyShift) {
    action.sheet.verb = CharacterVerb::kTakeOff;
    action.sheet.slot = shown.piece.client_slot;
  }
}

void DrawIconGestures(const Painter& paint, const EquipWindowLayout& layout,
                      const EquipWindowView& view, EquipWindowTab tab, EquipWindowAction& action) {
  for (std::size_t place = 0; place < kEquipWindowPlaceCount; ++place) {
    IconGesture(paint, PieceAt(view, tab, place), place,
                ImVec2(IconLeft(layout, place), BandTop(place)), ImVec2(kIconPixels, kRowPitch),
                action);
  }
  if (tab == EquipWindowTab::kGeneral) {
    const float left = CenterOf(layout) - kAmmoFromCenter + kAmmoZoneInset;
    const float side = kAmmoFromCenter - 2.0F * kAmmoZoneInset;
    IconGesture(paint, view.ammo, kEquipWindowAmmoPlace,
                ImVec2(left, kAmmoIconTop + kAmmoZoneInset), ImVec2(side, side), action);
  }
}

// LA RANGÉE SOUS LA SOURIS : sa bulle, et sa description d'un clic droit.
void DrawEntryGestures(const Painter& paint, const EquipWindowLayout& layout,
                       const EquipWindowView& view, EquipWindowTab tab, EquipWindowAction& action) {
  if (!ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)) {
    return;
  }
  const ImVec2 mouse = ImGui::GetIO().MousePos;
  const ImVec2 origin = paint.At(0.0F, 0.0F);
  const ImVec2 at((mouse.x - origin.x) / paint.scale(), (mouse.y - origin.y) / paint.scale());
  const std::optional<std::size_t> place = EquipWindowEntryAt(layout, at);
  if (!place.has_value()) {
    return;
  }
  const EquipWindowPiece& shown = PieceAt(view, tab, *place);
  if (!shown.piece.filled()) {
    return;
  }
  // ⚠ RIEN PENDANT UN GLISSER, ni bouton tenu : la bulle cacherait la fenêtre
  // où l'on vise.
  if (!ImGui::IsMouseDown(ImGuiMouseButton_Left) && ImGui::GetDragDropPayload() == nullptr) {
    ShowPlaceTip(paint, layout, at, shown);
  }
  // 🔴 AU RELÂCHEMENT : ouverte dès le clic, la description passerait derrière
  // cette fenêtre sur un appui prolongé.
  if (ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
    action.sheet.describe_id = shown.piece.item_id;
    action.sheet.describe_slot = shown.piece.client_slot;
  }
}

// LE LÂCHER, SUR TOUTE LA FENÊTRE : une pièce du sac s'y porte où le serveur la
// range. Et tant qu'elle survole, sa place se dit à `app`, qui entoure celles
// où elle peut aller.
void DrawDropTarget(const Painter& paint, const EquipWindowLayout& layout,
                    EquipWindowAction& action) {
  const ImRect body(paint.At(0.0F, kEquipWindowTitleBarPixels),
                    paint.At(layout.width, kEquipWindowHeightPixels));
  if (!ImGui::BeginDragDropTargetCustom(body, ImGui::GetID("##lacher"))) {
    return;
  }
  constexpr ImGuiDragDropFlags kPeek =
      ImGuiDragDropFlags_AcceptBeforeDelivery | ImGuiDragDropFlags_AcceptNoDrawDefaultRect;
  if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kItemDragPayload, kPeek);
      payload != nullptr && payload->DataSize == static_cast<int>(sizeof(ItemDrag))) {
    const auto& carried = *static_cast<const ItemDrag*>(payload->Data);
    if (!carried.worn && carried.from == ItemHolder::kInventory) {
      action.dragged_slot = carried.client_slot;
      if (payload->IsDelivery()) {
        action.sheet.verb = CharacterVerb::kWear;
        action.sheet.slot = carried.client_slot;
        action.sheet.alternate = ImGui::GetIO().KeyCtrl;
      }
    }
  }
  ImGui::EndDragDropTarget();
}

// ── LE BAS ET LES BOUTONS SUR LE PERSONNAGE ─────────────────────────────────

// LA CASE À COCHER DE L'ONGLET : elle se dessine sur ce que le serveur a dit,
// et le clic demande l'autre état.
void DrawSettingBox(const Painter& paint, const SkinArt& art, const char* label, bool ticked,
                    std::optional<bool>& asked) {
  const SkinImage& box = art.Get(ticked ? SkinPiece::kCheckBoxTicked : SkinPiece::kCheckBoxEmpty);
  const float top = kEquipWindowHeightPixels - kCheckFromBottom;
  const float box_width = box.usable() ? box.width : kCheckHeight;
  const float width = box_width + kCheckTextGap + paint.TextWidth(label, kTextBody);
  if (paint.Hit("##case", kCheckLeft, top, width, kCheckHeight).pressed) {
    asked = !ticked;
  }
  paint.Art(box, kCheckLeft, top + std::floor((kCheckHeight - box.height) / 2.0F));
  paint.Text(kCheckLeft + box_width + kCheckTextGap - kGlyphBearing, top, label, kTextBody,
             paint.Ink(kBlack));
}

[[nodiscard]] bool TabWorn(const EquipWindowView& view, EquipWindowTab tab) {
  const auto& pieces = tab == EquipWindowTab::kCostume ? view.costume : view.general;
  const bool any = std::any_of(pieces.begin(), pieces.end(),
                               [](const EquipWindowPiece& shown) { return shown.piece.filled(); });
  return any || (tab == EquipWindowTab::kGeneral && view.ammo.piece.filled());
}

// « TOUT RETIRER » : éteint sans rien à retirer, et pendant que le serveur
// répond.
void DrawTakeOffAll(const Painter& paint, const SkinArt& art, const EquipWindowView& view,
                    EquipWindowTab tab, EquipWindowAction& action) {
  if (!TabWorn(view, tab) || view.take_off_all_pending) {
    paint.Art(art.Get(SkinPiece::kEquipTakeOffAllOff), kTakeOffAllLeft, kTakeOffAllTop);
    return;
  }
  const achievement::HitResult hit =
      PaintedButton(paint, "##toutretirer", art.Get(SkinPiece::kEquipTakeOffAllOut),
                    art.Get(SkinPiece::kEquipTakeOffAllOver), kTakeOffAllLeft, kTakeOffAllTop);
  if (hit.hovered) {
    SkinTooltip(core::Tr("Tout retirer"));
  }
  if (hit.pressed) {
    action.take_off_all_costume = tab == EquipWindowTab::kCostume;
  }
}

// LE BOUTON DE MONTURE : un compagnon le renvoie d'un clic ; plusieurs ouvrent
// le menu qui demande lequel.
void DrawRiderButton(const Painter& paint, const EquipWindowLayout& layout, const SkinArt& art,
                     std::span<const EquipWindowRider> riders, EquipWindowAction& action) {
  if (riders.empty()) {
    return;
  }
  const SkinImage& image = art.Get(SkinPiece::kEquipRiderOff);
  const achievement::HitResult hit = PaintedButton(paint, "##monture", image, image,
                                                   CenterOf(layout) - kRiderFromCenter, kRiderTop);
  constexpr const char* kRiderMenu = "##montures";
  if (hit.hovered) {
    SkinTooltip(riders.size() == 1 ? std::string(riders.front().label).c_str()
                                   : core::Tr("Monter / descendre"));
  }
  if (hit.pressed) {
    if (riders.size() == 1) {
      action.sheet.toggled_companion = riders.front().rank;
    } else {
      ImGui::OpenPopup(kRiderMenu);
    }
  }
  if (BeginSkinPopup(kRiderMenu)) {
    for (const EquipWindowRider& rider : riders) {
      if (ImGui::Selectable(std::string(rider.label).c_str())) {
        action.sheet.toggled_companion = rider.rank;
      }
    }
    EndSkinPopup();
  }
}

void DrawCartButton(const Painter& paint, const EquipWindowLayout& layout, const SkinArt& art,
                    const EquipWindowView& view, EquipWindowAction& action) {
  if (!view.cart) {
    return;
  }
  const achievement::HitResult hit = PaintedButton(
      paint, "##chariot", art.Get(SkinPiece::kEquipCartOut), art.Get(SkinPiece::kEquipCartOver),
      CenterOf(layout) - kCartFromCenter, kCartTop);
  if (hit.hovered && !view.cart_tooltip.empty()) {
    SkinTooltip(view.cart_tooltip.c_str());
  }
  action.toggle_cart = hit.pressed;
}

}  // namespace ro::ui::equip_parts
