// LA SECTION « EFFETS » DU PANNEAU DE L'ÉDITEUR DE CARTE : la liste, l'ajout
// par numéro, et l'effet désigné — sa place, sa cadence, ses paramètres, la
// nuit, sa lueur, sa raison.

#include "ro/ui/map_editor_effects_panel.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <format>
#include <optional>
#include <string>
#include <string_view>

#include "ro/core/i18n.h"
#include "ro/ui/hour_range_text.h"
#include "ro/ui/map_editor_row_menu.h"
#include "ro/ui/skin_popup.h"
#include "ro/ui/skin_tooltip.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/triple_clipboard.h"
#include "ro/ui/wheel_drag.h"

#include "imgui.h"

namespace ro::ui {
namespace {

// Ce que vaut un pixel glissé et un cran de molette, sur la cadence d'un effet
// et sur la portée de sa lueur.
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
// Une borne de plage d'heures : un cran de molette vaut un quart d'heure.
constexpr float kHoursPerDay = 24.0F;
constexpr WheelDrag kHourDrag{.drag_step = 0.05F,
                              .wheel_step = 0.25F,
                              .lowest = 0.0F,
                              .highest = kHoursPerDay,
                              .format = "%.2f h"};

// LE MENU DU CLIC DROIT SUR LA PLAGE D'HEURES qu'on vient de poser : la copier
// en texte, « 20:00-06:00 », ou coller celle que porte le presse-papiers, qui
// choisit la plage. Chaque entrée est grisée quand elle n'a rien à faire. Vrai
// quand Coller a changé `hours`.
[[nodiscard]] bool HourRangeMenu(MapEditorHours& hours) {
  if (!BeginSkinPopupContextItem("##plage_heures")) {
    return false;
  }
  if (ImGui::MenuItem(core::Tr("Copier la plage"), nullptr, false, hours.chosen)) {
    ImGui::SetClipboardText(
        HourRangeText(HourRange{.from_hour = hours.from_hour, .to_hour = hours.to_hour}).c_str());
  }
  bool pasted = false;
  const std::optional<HourRange> read = ClipboardHourRange();
  if (ImGui::MenuItem(core::Tr("Coller la plage"), nullptr, false, read.has_value())) {
    hours = MapEditorHours{.chosen = true, .from_hour = read->from_hour, .to_hour = read->to_hour};
    pasted = true;
  }
  EndSkinPopup();
  return pasted;
}

using WhyBuffer = std::array<char, MapEditorPanelState::kWhyCapacity>;

// LA SAISIE D'UNE RAISON, et l'effet dont elle est la saisie.
struct WhyField {
  WhyBuffer& text;
  std::optional<std::size_t>& of;
  bool& typing;
};

// Ce que les champs d'un effet ont fait à l'image.
struct EffectFieldsResult {
  // Une valeur a changé, un glissé s'est relâché, une case a basculé.
  bool changed = false;
  bool released = false;
  bool toggled = false;
};

// LES CHAMPS D'UN EFFET AJOUTÉ : son numéro, sa position en cases, sa cadence,
// ses paramètres, la nuit, sa lueur et sa raison. `chosen` : l'effet dont la
// raison se saisit ; un autre la recharge.
EffectFieldsResult DrawEffectFields(MapEditorEffect& effect, std::size_t chosen, MapCubes cubes,
                                    WhyField why, const SkinArt& art) {
  EffectFieldsResult result;
  result.changed |= ImGui::InputInt(core::Tr("Numéro de l'effet"), &effect.number);
  result.released |= ImGui::IsItemDeactivatedAfterEdit();
  const WheelDragResult moved =
      PositionInCells(core::Tr("Position (cases, hauteur)"), effect.position, cubes);
  result.toggled |= PositionClipboardMenu("position", effect.position, cubes);
  const WheelDragResult paced =
      WheelDragFloat(core::Tr("Cadence"), effect.emit_speed, kEmitSpeedDrag);
  result.changed |= moved.changed || paced.changed;
  result.released |= moved.released || paced.released;
  result.changed |= ImGui::DragFloat4(core::Tr("Paramètres"), effect.param.data(), kParamStep);
  result.released |= ImGui::IsItemDeactivatedAfterEdit();
  result.toggled |= SkinCheckBox(core::Tr("La nuit seulement"), &effect.night_only, art);
  if (effect.night_only) {
    WheelDragResult hours_moved;
    result.toggled |= LitHoursFields(effect.hours, art, hours_moved);
    result.changed |= hours_moved.changed;
    result.released |= hours_moved.released;
  }
  result.toggled |= SkinCheckBox(core::Tr("Éclaire"), &effect.glows, art);
  if (effect.glows) {
    result.changed |= ImGui::ColorEdit3(core::Tr("Couleur"), effect.glow_color.data(),
                                        ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_Float);
    result.released |= ImGui::IsItemDeactivatedAfterEdit();
    const WheelDragResult reach =
        WheelDragFloat(core::Tr("Portée"), effect.glow_range, kGlowRangeDrag);
    result.changed |= reach.changed;
    result.released |= reach.released;
    result.toggled |= SkinCheckBox(core::Tr("Vacille"), &effect.glow_flicker, art);
    result.toggled |= SkinCheckBox(core::Tr("Projette des ombres"), &effect.glow_shadow, art);
    SkinTooltip(ShadowHelp());
  }
  const std::string_view typed(why.text.data(), strnlen(why.text.data(), why.text.size()));
  if (why.of != chosen || (!why.typing && typed != effect.why)) {
    why.text.fill('\0');
    const std::size_t kept = std::min(effect.why.size(), why.text.size() - 1);
    std::copy_n(effect.why.begin(), kept, why.text.begin());
    why.of = chosen;
  }
  static_cast<void>(ImGui::InputText(core::Tr("Pourquoi"), why.text.data(), why.text.size()));
  why.typing = ImGui::IsItemActive();
  if (ImGui::IsItemDeactivatedAfterEdit()) {
    effect.why = std::string(why.text.data(), strnlen(why.text.data(), why.text.size()));
    result.changed = true;
    result.released = true;
  }
  return result;
}

// LES EFFETS DE LA CARTE : la liste des objets d'effet du RSW, et celui qu'on
// règle — sa place, masqué, la nuit, sa lueur, sa raison.
void DrawMapEffects(const MapEditorView& view, MapEditorPanelState& panel,
                    std::vector<MapEditorIntent>& intents, const SkinArt& art) {
  ImGui::SeparatorText(core::Tr("Effets de la carte"));
  if (view.map_effects.empty()) {
    ImGui::TextDisabled("%s", core::Tr("Cette carte n'a aucun effet."));
    return;
  }
  for (const MapEditorMapEffect& effect : view.map_effects) {
    const std::array<float, 3> cells = CellsOfNative(effect.position, view.map_cubes);
    const std::string label = std::format(
        "{}{} — #{} — {:.1f} {:.1f} {:.1f}{}{}##effet_carte{}", effect.edited ? "✎ " : "",
        effect.effect_index, effect.number, cells[0], cells[1], cells[2],
        effect.hidden ? std::format(" ({})", core::Tr("masqué")) : std::string(),
        WhyTag(effect.why), effect.effect_index);
    const bool chosen = view.selected_map_effect == effect.effect_index;
    if (ImGui::Selectable(label.c_str(), chosen)) {
      intents.emplace_back(
          map_editor::SelectMapEffect{chosen ? std::nullopt : std::optional(effect.effect_index)});
    }
    DrawMapEditorRowMenu(
        MapEditorRowMenu{.select = map_editor::SelectMapEffect{effect.effect_index},
                         .remove_label = effect.hidden ? core::Tr("Afficher") : core::Tr("Masquer"),
                         .revert = effect.edited
                                       ? std::optional<MapEditorIntent>(
                                             map_editor::RevertMapEffect{effect.effect_index})
                                       : std::nullopt,
                         .cells = cells,
                         .move_to = [&effect, cubes = view.map_cubes](
                                        const std::array<float, 3>& pasted) -> MapEditorIntent {
                           map_editor::SetMapEffect set{
                               .index = effect.effect_index, .effect = effect, .released = true};
                           set.effect.position = NativeOfCells(pasted, cubes);
                           return set;
                         }},
        intents);
  }
  if (!view.selected_map_effect.has_value() ||
      *view.selected_map_effect >= view.map_effects.size()) {
    return;
  }
  const std::size_t index = *view.selected_map_effect;
  ImGui::PushID("effet_carte");
  map_editor::SetMapEffect set{
      .index = index, .effect = view.map_effects[index], .released = false};
  bool changed = false;
  bool released = false;
  const WheelDragResult moved =
      PositionInCells(core::Tr("Position (cases, hauteur)"), set.effect.position, view.map_cubes);
  changed |= moved.changed;
  released |= moved.released;
  bool toggled = PositionClipboardMenu("position", set.effect.position, view.map_cubes);
  toggled |= SkinCheckBox(core::Tr("Masquer"), &set.effect.hidden, art);
  toggled |= SkinCheckBox(core::Tr("La nuit seulement"), &set.effect.night_only, art);
  if (set.effect.night_only) {
    WheelDragResult hours_moved;
    toggled |= LitHoursFields(set.effect.hours, art, hours_moved);
    changed |= hours_moved.changed;
    released |= hours_moved.released;
  }
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
    toggled |= SkinCheckBox(core::Tr("Projette des ombres"), &set.effect.glow_shadow, art);
    SkinTooltip(ShadowHelp());
  }
  const std::string_view typed(panel.map_effect_why.data(),
                               strnlen(panel.map_effect_why.data(), panel.map_effect_why.size()));
  if (panel.map_effect_why_of != view.selected_map_effect ||
      (!panel.map_effect_why_typing && typed != set.effect.why)) {
    panel.map_effect_why.fill('\0');
    const std::size_t kept = std::min(set.effect.why.size(), panel.map_effect_why.size() - 1);
    std::copy_n(set.effect.why.begin(), kept, panel.map_effect_why.begin());
    panel.map_effect_why_of = view.selected_map_effect;
  }
  static_cast<void>(ImGui::InputText(core::Tr("Pourquoi"), panel.map_effect_why.data(),
                                     panel.map_effect_why.size()));
  panel.map_effect_why_typing = ImGui::IsItemActive();
  if (ImGui::IsItemDeactivatedAfterEdit()) {
    set.effect.why = std::string(panel.map_effect_why.data(),
                                 strnlen(panel.map_effect_why.data(), panel.map_effect_why.size()));
    changed = true;
    released = true;
  }
  if (changed || released || toggled) {
    set.released = released || toggled || !ImGui::IsAnyItemActive();
    intents.emplace_back(std::move(set));
  }
  ImGui::BeginDisabled(!view.map_effects[index].edited);
  if (SkinButton(core::Tr("Rétablir l'effet"), art)) {
    intents.emplace_back(map_editor::RevertMapEffect{index});
  }
  ImGui::EndDisabled();
  ImGui::SameLine();
  if (SkinButton(core::Tr("Centrer"), art)) {
    intents.emplace_back(map_editor::Focus{});
  }
  ImGui::PopID();
}

}  // namespace

const char* ShadowHelp() {
  return core::Tr(
      "Décochez pour une lumière posée DANS un modèle du décor — une lanterne, un lampion : le "
      "modèle la boucherait et noircirait ce qu'il cache. Seules les quatre lumières qui portent "
      "ombre les plus proches de la caméra en projettent une.");
}

std::string WhyTag(std::string_view why) {
  if (why.empty()) {
    return {};
  }
  std::string tag = " — ";
  for (const char letter : why) {
    tag += letter;
    if (letter == '#' && tag.ends_with("##")) {
      tag.insert(tag.size() - 1, " ");
    }
  }
  return tag;
}

bool LitHoursFields(MapEditorHours& hours, const SkinArt& art, WheelDragResult& moved) {
  // La case et ses bornes forment un groupe : le clic droit sur l'un d'eux ouvre
  // le menu de la plage.
  ImGui::BeginGroup();
  bool toggled = SkinCheckBox(core::Tr("Plage d'heures"), &hours.chosen, art);
  if (ImGui::IsItemHovered()) {
    SkinTooltipFormat("%s", core::Tr("Suit l'heure du serveur quand le ciel en a une — le cycle "
                                     "jour et nuit ; sinon, la nuit du serveur décide."));
  }
  if (hours.chosen) {
    for (const WheelDragResult field :
         {WheelDragFloat(core::Tr("De (heure)"), hours.from_hour, kHourDrag),
          WheelDragFloat(core::Tr("À (heure)"), hours.to_hour, kHourDrag)}) {
      moved.changed |= field.changed;
      moved.released |= field.released;
    }
  }
  ImGui::EndGroup();
  toggled |= HourRangeMenu(hours);
  return toggled;
}

void DrawEffects(const MapEditorView& view, MapEditorPanelState& panel,
                 std::vector<MapEditorIntent>& intents, const SkinArt& art) {
  // Les effets de la carte d'abord : ce sont eux qu'on vient d'abord régler.
  DrawMapEffects(view, panel, intents, art);
  ImGui::SeparatorText(core::Tr("Effets ajoutés"));
  static_cast<void>(ImGui::InputInt(core::Tr("Numéro de l'effet"), &panel.effect_number));
  if (SkinButton(core::Tr("Ajouter un effet"), art)) {
    intents.emplace_back(map_editor::AddEffect{panel.effect_number});
  }
  for (std::size_t slot = 0; slot < view.effects.size(); ++slot) {
    const MapEditorEffect& effect = view.effects[slot];
    // Il se reconnaît à sa case, comme sur la minicarte.
    const std::array<float, 3> cells = CellsOfNative(effect.position, view.map_cubes);
    const std::string label =
        std::format("{} {} — #{} — {:.1f} {:.1f} {:.1f}{}##effet{}", core::Tr("Effet"), slot,
                    effect.number, cells[0], cells[1], cells[2], WhyTag(effect.why), slot);
    const bool chosen = view.selected_effect == slot;
    if (ImGui::Selectable(label.c_str(), chosen)) {
      intents.emplace_back(map_editor::SelectEffect{chosen ? std::nullopt : std::optional(slot)});
    }
    DrawMapEditorRowMenu(
        MapEditorRowMenu{.select = map_editor::SelectEffect{slot},
                         .remove_label = core::Tr("Supprimer"),
                         .cells = cells,
                         .move_to = [&effect, slot, cubes = view.map_cubes](
                                        const std::array<float, 3>& pasted) -> MapEditorIntent {
                           map_editor::SetEffect set{
                               .slot = slot, .effect = effect, .released = true};
                           set.effect.position = NativeOfCells(pasted, cubes);
                           return set;
                         }},
        intents);
  }
  if (!view.selected_effect.has_value() || *view.selected_effect >= view.effects.size()) {
    return;
  }
  const std::size_t slot = *view.selected_effect;
  // Ses champs portent les noms de ceux du modèle : un autre espace d'ID.
  ImGui::PushID("effet");
  map_editor::SetEffect set{.slot = slot, .effect = view.effects[slot], .released = false};
  const EffectFieldsResult fields = DrawEffectFields(
      set.effect, slot, view.map_cubes,
      WhyField{panel.effect_why, panel.effect_why_of, panel.effect_why_typing}, art);
  if (fields.changed || fields.released || fields.toggled) {
    set.released = fields.released || fields.toggled || !ImGui::IsAnyItemActive();
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

void DrawModelEffects(const MapEditorView& view, MapEditorPanelState& panel,
                      std::vector<MapEditorIntent>& intents, const SkinArt& art) {
  ImGui::SeparatorText(core::Tr("Effets de ce type de modèle"));
  // LE NUMÉRO EST CELUI DE « Ajouter un effet », les flammes par défaut, et
  // l'effet se pose au centre de la boîte : au cœur d'un brasero plutôt qu'à
  // son pied. Le champ porte le libellé de celui des effets ajoutés, peint dans
  // le même panneau : un autre espace d'ID.
  ImGui::PushID("assigner_effet_modele");
  static_cast<void>(ImGui::InputInt(core::Tr("Numéro de l'effet"), &panel.effect_number));
  ImGui::PopID();
  if (SkinButton(core::Tr("Assigner un effet à ce type de modèle"), art)) {
    intents.emplace_back(map_editor::AddModelEffect{
        .number = panel.effect_number, .position = view.selected_center.value_or(view.position)});
  }
  SkinTooltip(
      core::Tr("Toutes les poses de ce fichier, sur toutes les cartes, le portent. Il "
               "s'enregistre dans le document commun @model_lights, publié avec la "
               "carte."));
  for (const MapEditorModelEffect& shown : view.model_effects) {
    const std::array<float, 3> cells = CellsOfNative(shown.effect.position, view.map_cubes);
    const std::string label =
        std::format("{} {} — #{} — {:.1f} {:.1f} {:.1f}{}##effet_modele{}",
                    core::Tr("Effet de modèle"), shown.slot, shown.effect.number, cells[0],
                    cells[1], cells[2], WhyTag(shown.effect.why), shown.slot);
    const bool chosen = view.selected_model_effect == shown.slot;
    if (ImGui::Selectable(label.c_str(), chosen)) {
      intents.emplace_back(
          map_editor::SelectModelEffect{chosen ? std::nullopt : std::optional(shown.slot)});
    }
  }
  const auto chosen = std::ranges::find(
      view.model_effects, view.selected_model_effect,
      [](const MapEditorModelEffect& shown) { return std::optional<std::size_t>(shown.slot); });
  if (chosen == view.model_effects.end()) {
    return;
  }
  ImGui::PushID("effet_modele");
  map_editor::SetModelEffect set{.slot = chosen->slot, .effect = chosen->effect, .released = false};
  const EffectFieldsResult fields = DrawEffectFields(
      set.effect, chosen->slot, view.map_cubes,
      WhyField{panel.model_effect_why, panel.model_effect_why_of, panel.model_effect_why_typing},
      art);
  if (fields.changed || fields.released || fields.toggled) {
    set.released = fields.released || fields.toggled || !ImGui::IsAnyItemActive();
    intents.emplace_back(std::move(set));
  }
  if (SkinButton(core::Tr("Retirer l'effet de modèle"), art)) {
    intents.emplace_back(map_editor::RemoveModelEffect{chosen->slot});
  }
  ImGui::PopID();
}

}  // namespace ro::ui
