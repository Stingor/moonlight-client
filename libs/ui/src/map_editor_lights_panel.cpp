#include "ro/ui/map_editor_lights_panel.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "ro/core/i18n.h"
#include "ro/ui/map_cells.h"
#include "ro/ui/map_editor_effects_panel.h"
#include "ro/ui/map_editor_row_menu.h"
#include "ro/ui/skin_tooltip.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/triple_clipboard.h"
#include "ro/ui/wheel_drag.h"

#include "imgui.h"

namespace ro::ui {
namespace {

// Le pas de la portée d'une lumière, par pixel puis par cran de molette (dix avec Majuscule).
constexpr float kLightRangeStep = 0.5F;
constexpr float kLightRangeWheelStep = 1.0F;
// La plus grande portée : une portée reste positive — le document refuse une
// portée nulle —, et la glissière la borne de ce côté aussi.
constexpr float kLightRangeMax = 2000.0F;

using WhyBuffer = std::array<char, MapEditorPanelState::kWhyCapacity>;

// LA SAISIE D'UNE RAISON, et la lumière dont elle est la saisie.
struct WhyField {
  WhyBuffer& text;
  std::optional<std::size_t>& of;
  bool& typing;
};

// Ce que les champs d'une lumière ont fait à l'image.
struct LightFieldsResult {
  // Une valeur a changé, un glissé s'est relâché, une case a basculé.
  bool changed = false;
  bool released = false;
  bool toggled = false;
};

// LES CHAMPS D'UNE LUMIÈRE : sa position en cases, sa couleur, sa portée, ses
// heures, son vacillement, son ombre et sa raison. `chosen` : la lumière dont
// la raison se saisit ; une autre la recharge.
LightFieldsResult DrawLightFields(MapEditorLight& light, std::size_t chosen, MapCubes cubes,
                                  WhyField why, const SkinArt& art) {
  LightFieldsResult result;
  const WheelDragResult moved =
      PositionInCells(core::Tr("Position (cases, hauteur)"), light.position, cubes);
  result.changed |= moved.changed;
  result.released |= moved.released;
  result.toggled |= PositionClipboardMenu("position", light.position, cubes);
  const WheelDragResult colored = LightColorFields(light.color);
  result.changed |= colored.changed;
  result.released |= colored.released;
  const WheelDragResult reach = WheelDragFloat(core::Tr("Portée"), light.range,
                                               WheelDrag{.drag_step = kLightRangeStep,
                                                         .wheel_step = kLightRangeWheelStep,
                                                         .lowest = kLightRangeStep,
                                                         .highest = kLightRangeMax,
                                                         .format = "%.1f"});
  result.changed |= reach.changed;
  result.released |= reach.released;
  result.toggled |= SkinCheckBox(core::Tr("La nuit seulement"), &light.night_only, art);
  if (light.night_only) {
    WheelDragResult hours_moved;
    result.toggled |= LitHoursFields(light.hours, art, hours_moved);
    result.changed |= hours_moved.changed;
    result.released |= hours_moved.released;
  }
  result.toggled |= SkinCheckBox(core::Tr("Vacille"), &light.flicker, art);
  result.toggled |= SkinCheckBox(core::Tr("Projette des ombres"), &light.shadow, art);
  SkinTooltip(ShadowHelp());
  const std::string_view typed(why.text.data(), strnlen(why.text.data(), why.text.size()));
  if (why.of != chosen || (!why.typing && typed != light.why)) {
    why.text.fill('\0');
    const std::size_t kept = std::min(light.why.size(), why.text.size() - 1);
    std::copy_n(light.why.begin(), kept, why.text.begin());
    why.of = chosen;
  }
  static_cast<void>(ImGui::InputText(core::Tr("Pourquoi"), why.text.data(), why.text.size()));
  why.typing = ImGui::IsItemActive();
  if (ImGui::IsItemDeactivatedAfterEdit()) {
    light.why = std::string(why.text.data(), strnlen(why.text.data(), why.text.size()));
    result.changed = true;
    result.released = true;
  }
  return result;
}

// Le libellé d'une lumière dans une liste : elle se reconnaît à sa position.
std::string LightLabel(const char* kind, std::size_t slot, const MapEditorLight& light,
                       MapCubes cubes, const char* id) {
  const std::array<float, 3> cells = CellsOfNative(light.position, cubes);
  return std::format("{} {} — {:.1f} {:.1f} {:.1f}{}##{}{}", kind, slot, cells[0], cells[1],
                     cells[2], WhyTag(light.why), id, slot);
}

}  // namespace

void DrawLights(const MapEditorView& view, MapEditorPanelState& panel,
                std::vector<MapEditorIntent>& intents, const SkinArt& art) {
  if (SkinButton(core::Tr("Ajouter une lumière"), art)) {
    intents.emplace_back(map_editor::AddLight{});
  }
  for (std::size_t slot = 0; slot < view.lights.size(); ++slot) {
    const MapEditorLight& light = view.lights[slot];
    const bool chosen = view.selected_light == slot;
    if (ImGui::Selectable(
            LightLabel(core::Tr("Lumière"), slot, light, view.map_cubes, "lumiere").c_str(),
            chosen)) {
      intents.emplace_back(map_editor::SelectLight{chosen ? std::nullopt : std::optional(slot)});
    }
    DrawMapEditorRowMenu(
        MapEditorRowMenu{.select = map_editor::SelectLight{slot},
                         .remove_label = core::Tr("Supprimer"),
                         .cells = CellsOfNative(light.position, view.map_cubes),
                         .move_to = [&light, slot, cubes = view.map_cubes](
                                        const std::array<float, 3>& pasted) -> MapEditorIntent {
                           map_editor::SetLight set{.slot = slot, .light = light, .released = true};
                           set.light.position = NativeOfCells(pasted, cubes);
                           return set;
                         }},
        intents);
  }
  if (!view.selected_light.has_value() || *view.selected_light >= view.lights.size()) {
    return;
  }
  const std::size_t slot = *view.selected_light;
  // Ses glissières portent les noms de celles du modèle : un autre espace d'ID.
  ImGui::PushID("lumiere");
  map_editor::SetLight set{.slot = slot, .light = view.lights[slot], .released = false};
  const LightFieldsResult fields =
      DrawLightFields(set.light, slot, view.map_cubes,
                      WhyField{panel.light_why, panel.light_why_of, panel.light_why_typing}, art);
  if (fields.changed || fields.released || fields.toggled) {
    set.released = fields.released || fields.toggled || !ImGui::IsAnyItemActive();
    intents.emplace_back(std::move(set));
  }
  if (SkinButton(core::Tr("Retirer la lumière"), art)) {
    intents.emplace_back(map_editor::RemoveLight{slot});
  }
  ImGui::SameLine();
  if (SkinButton(core::Tr("Centrer"), art)) {
    intents.emplace_back(map_editor::Focus{});
  }
  ImGui::PopID();
}

void DrawModelLights(const MapEditorView& view, MapEditorPanelState& panel,
                     std::vector<MapEditorIntent>& intents, const SkinArt& art) {
  ImGui::SeparatorText(core::Tr("Lumières de ce type de modèle"));
  // AU CENTRE DE LA BOÎTE, comme « Ajouter une lumière ici » : au cœur d'un
  // lampion plutôt qu'à son pied.
  if (SkinButton(core::Tr("Assigner une lumière à ce type de modèle"), art)) {
    intents.emplace_back(
        map_editor::AddModelLight{.position = view.selected_center.value_or(view.position)});
  }
  SkinTooltip(
      core::Tr("Toutes les poses de ce fichier, sur toutes les cartes, la portent. Elle "
               "s'enregistre dans le document commun @model_lights, publié avec la "
               "carte."));
  // L'EXTINCTION VAUT AUSSI POUR LES EFFETS DU TYPE : un brasero éteint perd
  // sa flamme avec sa lumière.
  if (view.model_lights.empty() && view.model_effects.empty()) {
    return;
  }
  bool unlit = view.selected_unlit;
  if (SkinCheckBox(core::Tr("Éteindre ses lumières sur cette pose"), &unlit, art)) {
    intents.emplace_back(map_editor::SetModelUnlit{unlit});
  }
  SkinTooltip(core::Tr("Ses effets de modèle ne s'y lèvent pas non plus."));
  for (const MapEditorModelLight& shown : view.model_lights) {
    const bool chosen = view.selected_model_light == shown.slot;
    if (ImGui::Selectable(LightLabel(core::Tr("Lumière de modèle"), shown.slot, shown.light,
                                     view.map_cubes, "lumiere_modele")
                              .c_str(),
                          chosen)) {
      intents.emplace_back(
          map_editor::SelectModelLight{chosen ? std::nullopt : std::optional(shown.slot)});
    }
  }
  const auto chosen = std::ranges::find(
      view.model_lights, view.selected_model_light,
      [](const MapEditorModelLight& shown) { return std::optional<std::size_t>(shown.slot); });
  if (chosen == view.model_lights.end()) {
    return;
  }
  ImGui::PushID("lumiere_modele");
  map_editor::SetModelLight set{.slot = chosen->slot, .light = chosen->light, .released = false};
  const LightFieldsResult fields = DrawLightFields(
      set.light, chosen->slot, view.map_cubes,
      WhyField{panel.model_light_why, panel.model_light_why_of, panel.model_light_why_typing}, art);
  if (fields.changed || fields.released || fields.toggled) {
    set.released = fields.released || fields.toggled || !ImGui::IsAnyItemActive();
    intents.emplace_back(std::move(set));
  }
  if (SkinButton(core::Tr("Retirer la lumière de modèle"), art)) {
    intents.emplace_back(map_editor::RemoveModelLight{chosen->slot});
  }
  ImGui::PopID();
}

}  // namespace ro::ui
