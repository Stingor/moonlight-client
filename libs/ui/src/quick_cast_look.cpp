#include "ro/ui/quick_cast_look.h"

#include "ro/ui/skin_widgets.h"

#include "imgui.h"

namespace ro::ui {

QuickCastTexts QuickCastTextsFor(bool staff) {
  return QuickCastTexts{.title = core::Tr(kQuickCastTitle),
                        .ground_label = core::Tr(kQuickCastGroundLabel),
                        .ground_help = core::Tr(kQuickCastGroundHelp),
                        .target_label = core::Tr(kQuickCastTargetLabel),
                        .target_help = core::Tr(kQuickCastTargetHelp),
                        .repeat_label = core::Tr(kQuickCastRepeatLabel),
                        .repeat_help = core::Tr(kQuickCastRepeatHelp),
                        .item_label = core::Tr(kQuickCastItemLabel),
                        .item_help = core::Tr(kQuickCastItemHelp),
                        .item_repeat_label = core::Tr(kQuickCastItemRepeatLabel),
                        .item_repeat_help = core::Tr(staff ? kQuickCastItemRepeatStaffHelp
                                                           : kQuickCastItemRepeatPlayerHelp)};
}

bool DrawQuickCastSettings(QuickCastLook& look, const SkinArt& art, bool staff) {
  const QuickCastTexts texts = QuickCastTextsFor(staff);
  // Le titre de section vient d'ImGui, comme les autres sections de l'établi.
  ImGui::SeparatorText(texts.title);
  bool changed = false;
  changed |= SkinCheckBox(texts.ground_label, &look.ground, art);
  ImGui::SameLine();
  SkinHelpMarker(texts.ground_help);
  changed |= SkinCheckBox(texts.target_label, &look.target, art);
  ImGui::SameLine();
  SkinHelpMarker(texts.target_help);

  if (look.ground || look.target) {
    changed |= SkinSliderInt(texts.repeat_label, &look.repeat_ms,
                             (staff ? kQuickCastRepeatStaffLowestMs : kQuickCastRepeatLowestMs),
                             kQuickCastRepeatHighestMs, art);
    ImGui::SameLine();
    SkinHelpMarker(texts.repeat_help);
  }

  ImGui::Spacing();
  changed |= SkinCheckBox(texts.item_label, &look.item, art);
  ImGui::SameLine();
  SkinHelpMarker(texts.item_help);
  if (look.item) {
    // ⚠ CE CLAMP NE REMET QUE L'AFFICHAGE DANS LA PLAGE, un fichier édité à la
    // main compris. Le plancher qui compte s'applique à ce qui part : voir
    // `QuickCastItemPeriodMs`.
    const int floor_ms = QuickCastItemFloorMs(staff);
    if (look.item_repeat_ms < floor_ms) {
      look.item_repeat_ms = floor_ms;
      changed = true;
    }
    changed |= SkinSliderInt(texts.item_repeat_label, &look.item_repeat_ms, floor_ms,
                             kQuickCastItemHighestMs, art);
    ImGui::SameLine();
    SkinHelpMarker(texts.item_repeat_help);
  }
  return changed;
}

}  // namespace ro::ui
