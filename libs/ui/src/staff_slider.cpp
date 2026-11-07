// Voir l'en-tête.

#include "ro/ui/staff_slider.h"

#include <algorithm>

#include <imgui.h>

#include "ro/ui/skin_widgets.h"

namespace ro::ui {

bool StaffSliderAwaited(const std::optional<StaffSliderPending>& pending, int served,
                        double now_s) noexcept {
  return pending.has_value() && served != pending->value &&
         now_s - pending->sent_at_s < kStaffSliderAwaitSeconds;
}

std::optional<int> StaffSliderSettle(int value, const StaffSliderGesture& gesture,
                                     std::optional<int>& draft) {
  if (gesture.released_after_edit) {
    draft.reset();
    return value;
  }
  if (gesture.active) {
    // ⚠ UN CLIC QUI SAISIT SANS RIEN CHANGER NE FAIT PAS DE BROUILLON : il
    // renverrait la valeur montrée au relâchement.
    if (gesture.changed || draft.has_value()) {
      draft = value;
    }
    return std::nullopt;
  }
  if (gesture.changed) {
    draft = value;
  }
  if (draft.has_value() && !gesture.mouse_held) {
    const int settled = *draft;
    draft.reset();
    return settled;
  }
  return std::nullopt;
}

int StaffSliderShown(std::optional<StaffSliderPending>& pending, int served, double now_s) {
  if (!StaffSliderAwaited(pending, served, now_s)) {
    pending.reset();
    return served;
  }
  return pending->value;
}

std::optional<int> StaffReleasedSlider(const char* label, int shown, int lowest, int highest,
                                       std::optional<int>& draft, const SkinArt& art) {
  int value = std::clamp(draft.value_or(shown), lowest, highest);
  const bool changed =
      SkinSliderInt(label, &value, lowest, highest, art, "%d", 0, SkinSliderWheel::kLeaves);
  return StaffSliderSettle(value,
                           StaffSliderGesture{
                               .changed = changed,
                               .active = ImGui::IsItemActive(),
                               .released_after_edit = ImGui::IsItemDeactivatedAfterEdit(),
                               .mouse_held = ImGui::IsMouseDown(ImGuiMouseButton_Left),
                           },
                           draft);
}

std::optional<int> StaffWheelSlider(const char* label, int shown, int lowest, int highest,
                                    StaffWheelDraft& draft, const SkinArt& art) {
  int value = std::clamp(draft.value.value_or(shown), lowest, highest);
  const bool changed = SkinSliderInt(label, &value, lowest, highest, art);
  const double now_s = ImGui::GetTime();
  if (changed) {
    draft.changed_at_s = now_s;
  }
  const bool resting = now_s - draft.changed_at_s >= kStaffSliderRestSeconds;
  const std::optional<int> settled =
      StaffSliderSettle(value,
                        StaffSliderGesture{
                            .changed = changed,
                            .active = ImGui::IsItemActive(),
                            .released_after_edit = ImGui::IsItemDeactivatedAfterEdit(),
                            .mouse_held = ImGui::IsMouseDown(ImGuiMouseButton_Left) || !resting,
                        },
                        draft.value);
  return settled == shown ? std::nullopt : settled;
}

}  // namespace ro::ui
