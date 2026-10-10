// LE CHAMP À PAS DU CLIENT : un « - » habillé, le champ, un « + » habillé, et
// le libellé à droite. Voir `SkinInputInt` dans `ro/ui/skin_widgets.h`.

#include <algorithm>

#include <imgui_internal.h>

#include "ro/ui/skin_widgets.h"

namespace ro::ui {
namespace {

// Le décompte d'un pas : le rapide quand Ctrl est tenu et qu'il existe.
template <typename Number>
[[nodiscard]] Number StepNow(Number step, Number fast_step) {
  return ImGui::GetIO().KeyCtrl && fast_step > Number{} ? fast_step : step;
}

// LA DISPOSITION COMMUNE, quel que soit le nombre. `field` dessine le champ sans
// pas dans la largeur posée ; `step_by` avance la valeur d'un pas signé.
//
// ⚠ LA LARGEUR TOTALE EST CELLE DE L'ITEM COURANT, boutons compris — comme le
// champ à pas d'ImGui et comme le curseur habillé : une liste de réglages qui
// mêle les trois garde ses libellés alignés.
//
// ⚠ LES BOUTONS RÉPÈTENT quand on les tient : c'est le geste du champ d'ImGui
// qu'ils remplacent.
template <typename Field, typename StepBy>
[[nodiscard]] bool SteppedRow(const char* label, const SkinArt& art, Field field, StepBy step_by) {
  const ImGuiStyle& style = ImGui::GetStyle();
  const float whole = ImGui::CalcItemWidth();
  const float gap = style.ItemInnerSpacing.x;
  const float button = std::max(SkinSmallButtonWidth("-", art), SkinSmallButtonWidth("+", art));
  const float field_width = std::max(1.0F, whole - (button + gap) * 2.0F);

  ImGui::PushID(label);
  ImGui::BeginGroup();
  const float start_x = ImGui::GetCursorScreenPos().x;
  bool changed = false;
  ImGui::PushItemFlag(ImGuiItemFlags_ButtonRepeat, true);
  if (SkinSmallButton("-", art, ImVec2(button, 0.0F))) {
    changed |= step_by(-1);
  }
  ImGui::SameLine(0.0F, gap);
  ImGui::SetNextItemWidth(field_width);
  changed |= field();
  ImGui::SameLine(0.0F, gap);
  if (SkinSmallButton("+", art, ImVec2(button, 0.0F))) {
    changed |= step_by(1);
  }
  ImGui::PopItemFlag();
  // ⚠ LE LIBELLÉ SE POSE À LA LARGEUR TOTALE, et non à la suite du « + » : le
  // petit bouton se resserre contre son voisin, et un libellé qui le suivrait
  // tomberait quelques pixels plus à gauche que celui d'un curseur voisin.
  const char* label_end = ImGui::FindRenderedTextEnd(label);
  if (label_end != label) {
    ImGui::SameLine(0.0F, 0.0F);
    ImGui::SetCursorScreenPos(ImVec2(start_x + whole + gap, ImGui::GetCursorScreenPos().y));
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label, label_end);
  }
  ImGui::EndGroup();
  ImGui::PopID();
  return changed;
}

}  // namespace

bool SkinInputInt(const char* label, int* value, int lowest, int highest, int step, int fast_step,
                  const SkinArt& art) {
  const int before = *value;
  int edited = before;
  const auto field = [&edited] { return ImGui::InputInt("##value", &edited, 0, 0); };
  const auto step_by = [&edited, step, fast_step](int direction) {
    edited += direction * StepNow(step, fast_step);
    return true;
  };
  if (!SteppedRow(label, art, field, step_by)) {
    return false;
  }
  *value = std::clamp(edited, lowest, highest);
  return *value != before;
}

bool SkinInputFloat(const char* label, float* value, float lowest, float highest, float step,
                    float fast_step, const SkinArt& art, const char* format) {
  const float before = *value;
  float edited = before;
  const auto field = [&edited, format] {
    return ImGui::InputFloat("##value", &edited, 0.0F, 0.0F, format);
  };
  const auto step_by = [&edited, step, fast_step](int direction) {
    edited += static_cast<float>(direction) * StepNow(step, fast_step);
    return true;
  };
  if (!SteppedRow(label, art, field, step_by)) {
    return false;
  }
  *value = std::clamp(edited, lowest, highest);
  return *value != before;
}

}  // namespace ro::ui
