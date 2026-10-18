#include "ro/ui/wheel_drag.h"

#include <algorithm>
#include <cstddef>
#include <string_view>

#include <imgui.h>

#include "ro/ui/wheel_gate.h"

#include "ui_text.h"

namespace ro::ui {
namespace {

// Ce qui sépare l'étiquette affichée de la part qui ne sert qu'à l'identifier.
constexpr std::string_view kHiddenLabelMark = "##";

// Le champ posé, puis la molette sur lui.
WheelDragResult DragWithWheel(const char* id, float& value, const WheelDrag& drag) {
  const bool bounded = drag.lowest < drag.highest;
  WheelDragResult result;
  result.changed =
      ImGui::DragFloat(id, &value, drag.drag_step, drag.lowest, drag.highest, drag.format,
                       bounded ? ImGuiSliderFlags_AlwaysClamp : ImGuiSliderFlags_None);
  result.released = ImGui::IsItemDeactivatedAfterEdit();
  if (const float wheel = WheelOverItem(); wheel != 0.0F) {
    const float multiplier = ImGui::GetIO().KeyShift ? kWheelShiftMultiplier : 1.0F;
    float wanted = value + (wheel > 0.0F ? 1.0F : -1.0F) * drag.wheel_step * multiplier;
    if (bounded) {
      wanted = std::clamp(wanted, drag.lowest, drag.highest);
    }
    if (wanted != value) {
      value = wanted;
      result.changed = true;
      result.released = true;
    }
  }
  return result;
}

}  // namespace

WheelDragResult WheelDragFloat(const char* label, float& value, const WheelDrag& drag) {
  return DragWithWheel(label, value, drag);
}

WheelDragResult WheelDragFloat3(const char* label, std::array<float, 3>& values,
                                const WheelDrag& drag) {
  const ImGuiStyle& style = ImGui::GetStyle();
  const float spacing = style.ItemInnerSpacing.x;
  const float each =
      std::max(1.0F, (ImGui::CalcItemWidth() - spacing * static_cast<float>(values.size() - 1)) /
                         static_cast<float>(values.size()));
  WheelDragResult result;
  ImGui::BeginGroup();
  ImGui::PushID(label);
  for (std::size_t axis = 0; axis < values.size(); ++axis) {
    if (axis > 0) {
      ImGui::SameLine(0.0F, spacing);
    }
    ImGui::PushID(static_cast<int>(axis));
    ImGui::SetNextItemWidth(each);
    const WheelDragResult one = DragWithWheel("##composante", values[axis], drag);
    result.changed |= one.changed;
    result.released |= one.released;
    ImGui::PopID();
  }
  const std::string_view whole(label);
  const std::string_view shown = whole.substr(0, whole.find(kHiddenLabelMark));
  if (!shown.empty()) {
    ImGui::SameLine(0.0F, spacing);
    Unformatted(shown);
  }
  ImGui::PopID();
  ImGui::EndGroup();
  return result;
}

}  // namespace ro::ui
