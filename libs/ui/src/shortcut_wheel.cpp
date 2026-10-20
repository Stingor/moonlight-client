// LA ROUE D'UTILISATION : les douze cases d'une barre en cercle, sous le
// curseur. Voir `ro/ui/shortcut_bar.h`.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <optional>
#include <span>
#include <string_view>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/ui/shortcut_bar.h"
#include "ro/ui/skin_text.h"

#include "shortcut_slot_paint.h"
#include "ui_scale.h"

namespace ro::ui {
namespace {

constexpr float kFullTurn = 2.0F * std::numbers::pi_v<float>;
constexpr float kSectorTurn = kFullTurn / static_cast<float>(kShortcutSlotsPerBar);
// L'angle d'ImGui part de la droite ; celui de la roue part du HAUT.
constexpr float kImGuiAngleOfTop = -0.5F * std::numbers::pi_v<float>;

// Le disque sombre sous les cases, qui les détache du monde. Il dépasse le
// cercle des centres de trois quarts de côté : une demi-case, plus une marge.
constexpr ImU32 kWheelBackdrop = IM_COL32(8, 8, 14, 150);
constexpr float kBackdropBeyondRingPerSide = 0.75F;
// Le secteur visé, éclairé du centre jusqu'au bord du disque.
constexpr ImU32 kAimedSector = IM_COL32(255, 205, 105, 70);
// La zone morte : un moyeu, cerclé pour qu'on voie où relâcher pour annuler.
constexpr ImU32 kHubFill = IM_COL32(20, 20, 28, 220);
constexpr ImU32 kHubEdge = IM_COL32(200, 200, 210, 160);
constexpr float kHubEdgeThickness = 1.0F;
// UNE CASE VIDE, EN POINTILLÉ : gardée à sa place pour que les autres ne
// tournent pas, mais visiblement sans rien à lancer.
constexpr ImU32 kEmptyDash = IM_COL32(170, 170, 180, 170);
constexpr float kDashLengthPx = 4.0F;
constexpr float kDashGapPx = 3.0F;
constexpr float kDashThickness = 1.0F;
// Le nom de la case visée, au centre.
constexpr ImU32 kNameColor = IM_COL32(255, 255, 255, 240);
constexpr ImU32 kNameHalo = IM_COL32(0, 0, 0, 230);

// Un trait pointillé de `from` à `to`, en tirets de longueur fixe.
void AddDashedLine(ImDrawList* into, ImVec2 from, ImVec2 to, float dash, float gap, ImU32 color,
                   float thickness) {
  const float dx = to.x - from.x;
  const float dy = to.y - from.y;
  const float length = std::sqrt(dx * dx + dy * dy);
  if (length <= 0.0F || dash <= 0.0F) {
    return;
  }
  const float ux = dx / length;
  const float uy = dy / length;
  for (float at = 0.0F; at < length; at += dash + gap) {
    const float until = std::min(at + dash, length);
    into->AddLine(ImVec2(from.x + ux * at, from.y + uy * at),
                  ImVec2(from.x + ux * until, from.y + uy * until), color, thickness);
  }
}

void PaintEmptySlot(ImDrawList* into, const ShortcutBarLayout& look, ImVec2 top_left, float side) {
  const float scale = Scale();
  const ImVec2 bottom_right(top_left.x + side, top_left.y + side);
  into->AddRectFilled(top_left, bottom_right, look.empty_color, kShortcutSlotRounding * scale);
  const float dash = kDashLengthPx * scale;
  const float gap = kDashGapPx * scale;
  const float thickness = kDashThickness * scale;
  const ImVec2 top_right(bottom_right.x, top_left.y);
  const ImVec2 bottom_left(top_left.x, bottom_right.y);
  AddDashedLine(into, top_left, top_right, dash, gap, kEmptyDash, thickness);
  AddDashedLine(into, top_right, bottom_right, dash, gap, kEmptyDash, thickness);
  AddDashedLine(into, bottom_right, bottom_left, dash, gap, kEmptyDash, thickness);
  AddDashedLine(into, bottom_left, top_left, dash, gap, kEmptyDash, thickness);
}

[[nodiscard]] bool CarriesSomething(const ShortcutSlotView& slot) {
  return slot.skill_id != 0 || slot.item_id != 0;
}

}  // namespace

std::optional<std::size_t> WheelBarOf(const ShortcutBarsLayout& layout) {
  if (layout.wheel_bar < 1 || layout.wheel_bar > static_cast<int>(kShortcutBarCount)) {
    return std::nullopt;
  }
  return static_cast<std::size_t>(layout.wheel_bar - 1);
}

float WheelRingRadius(float slot_side_px) {
  return std::max(kWheelSmallestRingRadiusPx, slot_side_px * kWheelRingRadiusPerSlotSide);
}

std::optional<std::size_t> WheelSectorAt(float dx_px, float dy_px, float dead_zone_radius_px) {
  // ⚠ ÉCRIT EN NÉGATION : un décalage qui ne serait pas un nombre ne vise rien,
  // au lieu de tomber dans le premier secteur.
  if (!(dx_px * dx_px + dy_px * dy_px >= dead_zone_radius_px * dead_zone_radius_px)) {
    return std::nullopt;
  }
  // L'angle depuis le HAUT, dans le sens des aiguilles : l'axe vertical de
  // l'écran descend, donc « en haut » est un `dy` négatif.
  float angle = std::atan2(dx_px, -dy_px);
  if (angle < 0.0F) {
    angle += kFullTurn;
  }
  // ⚠ UN DEMI-SECTEUR DE DÉCALAGE : la case 1 est AU MILIEU de son secteur, qui
  // déborde donc de part et d'autre du haut.
  const auto rank = static_cast<std::size_t>((angle + kSectorTurn * 0.5F) / kSectorTurn);
  return rank % kShortcutSlotsPerBar;
}

ImVec2 WheelSlotCenter(ImVec2 center, std::size_t rank, float ring_radius_px) {
  const float angle = static_cast<float>(rank % kShortcutSlotsPerBar) * kSectorTurn;
  return ImVec2(center.x + ring_radius_px * std::sin(angle),
                center.y - ring_radius_px * std::cos(angle));
}

std::optional<std::size_t> DrawShortcutWheel(const ShortcutWheelView& view,
                                             const ShortcutBarsLayout& layout,
                                             std::span<const ShortcutSlotView> slots) {
  if (!view.open || view.bar >= kShortcutBarCount) {
    return std::nullopt;
  }
  const ShortcutBarLayout& look = layout.bars[view.bar];
  const float side =
      std::clamp(look.slot_side_px, kSmallestSlotSidePixels, kLargestSlotSidePixels) * Scale();
  const float ring = WheelRingRadius(side);
  const float outer = ring + side * kBackdropBeyondRingPerSide;
  const std::optional<std::size_t> aimed = WheelSectorAt(
      view.pointer.x - view.center.x, view.pointer.y - view.center.y, kWheelDeadZoneRadiusPx);

  ImDrawList* into = ImGui::GetForegroundDrawList();
  into->AddCircleFilled(view.center, outer, kWheelBackdrop);
  if (aimed.has_value()) {
    // LE SECTEUR VISÉ, en part de tarte : un douzième de tour reste convexe.
    const float middle = kImGuiAngleOfTop + static_cast<float>(*aimed) * kSectorTurn;
    into->PathLineTo(view.center);
    into->PathArcTo(view.center, outer, middle - kSectorTurn * 0.5F, middle + kSectorTurn * 0.5F);
    into->PathFillConvex(kAimedSector);
  }
  into->AddCircleFilled(view.center, kWheelDeadZoneRadiusPx, kHubFill);
  into->AddCircle(view.center, kWheelDeadZoneRadiusPx, kHubEdge, 0, kHubEdgeThickness * Scale());

  const ShortcutSlotView nothing;
  for (std::size_t rank = 0; rank < kShortcutSlotsPerBar; ++rank) {
    const std::size_t index = ShortcutSlotIndex(view.bar, rank);
    const ShortcutSlotView& slot = index < slots.size() ? slots[index] : nothing;
    const ImVec2 middle = WheelSlotCenter(view.center, rank, ring);
    const ImVec2 top_left(middle.x - side * 0.5F, middle.y - side * 0.5F);
    if (CarriesSomething(slot)) {
      PaintShortcutSlot(into, look, slot, top_left, side,
                        SlotPaintMarks{.hot = aimed == rank, .show_key = layout.show_keys});
    } else {
      PaintEmptySlot(into, look, top_left, side);
    }
  }

  // LE NOM DE LA CASE VISÉE, AU CENTRE : c'est là que le regard se pose, et
  // l'icône seule ne départage pas deux potions qui se ressemblent.
  if (aimed.has_value()) {
    const std::size_t index = ShortcutSlotIndex(view.bar, *aimed);
    const ShortcutSlotView& slot = index < slots.size() ? slots[index] : nothing;
    // ⚠ UN REPLI NOMMÉ PLUTÔT QU'UN CENTRE MUET, comme la bulle de la barre :
    // un identifiant que la base ne connaît pas garde au moins son genre.
    std::string_view shown = slot.name;
    if (!CarriesSomething(slot)) {
      shown = core::Tr("Case vide");
    } else if (shown.empty()) {
      shown = slot.item_id != 0 ? core::Tr("Item") : core::Tr("Skill");
    }
    const ImVec2 size = ImGui::CalcTextSize(shown.data(), shown.data() + shown.size());
    AddTextHalo(into, ImVec2(view.center.x - size.x * 0.5F, view.center.y - size.y * 0.5F),
                kNameColor, shown.data(), shown.data() + shown.size(), kNameHalo);
  }
  return aimed;
}

}  // namespace ro::ui
