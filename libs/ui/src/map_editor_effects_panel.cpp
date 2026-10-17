// LA SECTION « EFFETS » DU PANNEAU DE L'ÉDITEUR DE CARTE : la liste, l'ajout
// par numéro, et l'effet désigné — sa place, sa cadence, ses paramètres, la
// nuit, sa lueur, sa raison.

#include "ro/ui/map_editor_effects_panel.h"

#include <algorithm>
#include <cstring>
#include <format>
#include <string>
#include <string_view>

#include "ro/core/i18n.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/wheel_drag.h"

#include "imgui.h"

namespace ro::ui {
namespace {

// Ce que vaut un pixel glissé et un cran de molette, sur la position d'un
// effet, sur sa cadence et sur la portée de sa lueur.
constexpr WheelDrag kEffectPositionDrag{.drag_step = 0.05F, .wheel_step = 1.0F, .format = "%.3f"};
constexpr float kLargestEmitSpeed = 10000.0F;
constexpr WheelDrag kEmitSpeedDrag{.drag_step = 1.0F,
                                   .wheel_step = 5.0F,
                                   .lowest = 0.0F,
                                   .highest = kLargestEmitSpeed,
                                   .format = "%.0f"};
constexpr float kSmallestGlowRange = 0.5F;
constexpr float kLargestGlowRange = 2000.0F;
constexpr WheelDrag kGlowRangeDrag{.drag_step = 0.5F,
                                   .wheel_step = 1.0F,
                                   .lowest = kSmallestGlowRange,
                                   .highest = kLargestGlowRange,
                                   .format = "%.1f"};
constexpr float kParamStep = 0.01F;

}  // namespace

void DrawEffects(const MapEditorView& view, MapEditorPanelState& panel,
                 std::vector<MapEditorIntent>& intents, const SkinArt& art) {
  static_cast<void>(ImGui::InputInt(core::Tr("Numéro de l'effet"), &panel.effect_number));
  if (SkinButton(core::Tr("Ajouter un effet"), art)) {
    intents.emplace_back(map_editor::AddEffect{panel.effect_number});
  }
  for (std::size_t slot = 0; slot < view.effects.size(); ++slot) {
    const MapEditorEffect& effect = view.effects[slot];
    const std::string label = std::format(
        "{} {} — #{} — {:.1f} {:.1f} {:.1f}##effet{}", core::Tr("Effet"), slot, effect.number,
        effect.position[0], effect.position[1], effect.position[2], slot);
    const bool chosen = view.selected_effect == slot;
    if (ImGui::Selectable(label.c_str(), chosen)) {
      intents.emplace_back(map_editor::SelectEffect{chosen ? std::nullopt : std::optional(slot)});
    }
  }
  if (!view.selected_effect.has_value() || *view.selected_effect >= view.effects.size()) {
    return;
  }
  const std::size_t slot = *view.selected_effect;
  // Ses champs portent les noms de ceux du modèle : un autre espace d'ID.
  ImGui::PushID("effet");
  map_editor::SetEffect set{.slot = slot, .effect = view.effects[slot], .released = false};
  bool changed = false;
  bool released = false;
  changed |= ImGui::InputInt(core::Tr("Numéro de l'effet"), &set.effect.number);
  released |= ImGui::IsItemDeactivatedAfterEdit();
  for (const WheelDragResult field :
       {WheelDragFloat3(core::Tr("Position"), set.effect.position, kEffectPositionDrag),
        WheelDragFloat(core::Tr("Cadence"), set.effect.emit_speed, kEmitSpeedDrag)}) {
    changed |= field.changed;
    released |= field.released;
  }
  changed |= ImGui::DragFloat4(core::Tr("Paramètres"), set.effect.param.data(), kParamStep);
  released |= ImGui::IsItemDeactivatedAfterEdit();
  bool toggled = SkinCheckBox(core::Tr("La nuit seulement"), &set.effect.night_only, art);
  toggled |= SkinCheckBox(core::Tr("Éclaire"), &set.effect.glows, art);
  if (set.effect.glows) {
    changed |= ImGui::ColorEdit3(core::Tr("Couleur"), set.effect.glow_color.data(),
                                 ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_Float);
    released |= ImGui::IsItemDeactivatedAfterEdit();
    const WheelDragResult reach =
        WheelDragFloat(core::Tr("Portée"), set.effect.glow_range, kGlowRangeDrag);
    changed |= reach.changed;
    released |= reach.released;
    toggled |= SkinCheckBox(core::Tr("Vacille"), &set.effect.glow_flicker, art);
  }
  const std::string_view typed(panel.effect_why.data(),
                               strnlen(panel.effect_why.data(), panel.effect_why.size()));
  if (panel.effect_why_of != view.selected_effect ||
      (!panel.effect_why_typing && typed != set.effect.why)) {
    panel.effect_why.fill('\0');
    const std::size_t kept = std::min(set.effect.why.size(), panel.effect_why.size() - 1);
    std::copy_n(set.effect.why.begin(), kept, panel.effect_why.begin());
    panel.effect_why_of = view.selected_effect;
  }
  static_cast<void>(
      ImGui::InputText(core::Tr("Pourquoi"), panel.effect_why.data(), panel.effect_why.size()));
  panel.effect_why_typing = ImGui::IsItemActive();
  if (ImGui::IsItemDeactivatedAfterEdit()) {
    set.effect.why = std::string(panel.effect_why.data(),
                                 strnlen(panel.effect_why.data(), panel.effect_why.size()));
    changed = true;
    released = true;
  }
  if (changed || released || toggled) {
    set.released = released || toggled || !ImGui::IsAnyItemActive();
    intents.emplace_back(std::move(set));
  }
  if (SkinButton(core::Tr("Retirer l'effet"), art)) {
    intents.emplace_back(map_editor::RemoveEffect{slot});
  }
  ImGui::SameLine();
  if (SkinButton(core::Tr("Centrer"), art)) {
    intents.emplace_back(map_editor::Focus{});
  }
  ImGui::PopID();
}

}  // namespace ro::ui
