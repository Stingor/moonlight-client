#include "ro/ui/map_editor_panel.h"

#include <algorithm>
#include <cstring>
#include <format>
#include <string>

#include "ro/core/i18n.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"
#include "ro/ui/window_bounds.h"
#include "ro/ui/window_name.h"

#include "imgui.h"

namespace ro::ui {
namespace {

// La taille au PREMIER affichage seulement : ensuite, c'est celle que le staff
// lui a donnée qui compte.
constexpr float kFirstWidth = 380.0F;
constexpr float kFirstHeight = 420.0F;
// La largeur des champs, fixe : une colonne de champs de même largeur aligne
// leurs libellés d'elle-même.
constexpr float kControlWidthPixels = 220.0F;

// Le pas des glissières, par pixel de souris : en unités du RSW, en degrés, et
// en facteur d'échelle.
constexpr float kPositionStep = 0.05F;
constexpr float kRotationStep = 0.5F;
constexpr float kScaleStep = 0.005F;

constexpr const char* kMapEditorTitle = core::TrText("Éditeur de carte");
constexpr const char* kMapEditorId = "map_editor";

constexpr std::array<const char*, 4> kMapEditorHelp = {
    core::TrText("Un clic dans le monde désigne le modèle du décor sous la souris : la plus "
                 "petite boîte traversée gagne. Ses retouches se redésignent aussi depuis la "
                 "liste."),
    core::TrText("Chaque retouche vérifie le rang, le fichier et la position d'origine du modèle : "
                 "une carte changée par un patch n'est pas retouchée de travers."),
    core::TrText("« Pourquoi » est obligatoire : le document refuse une retouche sans raison."),
    core::TrText("« Enregistrer » écrit data/map_edits/<carte>.yml dans le dossier du client. "
                 "Le visuel seulement : la marche reste celle du serveur."),
};

// LE PLACEMENT, réglé par glissières. Un geste qui glisse se voit à chaque
// image ; il ne devient une entrée de l'historique qu'au relâcher.
void DrawPlacement(const MapEditorView& view, std::vector<MapEditorIntent>& intents,
                   const SkinArt& art) {
  map_editor::Place place{.position = view.position,
                          .rotation = view.rotation,
                          .scale = view.scale,
                          .hidden = view.hidden};
  bool changed = false;
  bool released = false;
  changed |= ImGui::DragFloat3(core::Tr("Position"), place.position.data(), kPositionStep, 0.0F,
                               0.0F, "%.3f");
  released |= ImGui::IsItemDeactivatedAfterEdit();
  changed |= ImGui::DragFloat3(core::Tr("Rotation (°)"), place.rotation.data(), kRotationStep, 0.0F,
                               0.0F, "%.2f");
  released |= ImGui::IsItemDeactivatedAfterEdit();
  changed |=
      ImGui::DragFloat3(core::Tr("Échelle"), place.scale.data(), kScaleStep, 0.0F, 0.0F, "%.3f");
  released |= ImGui::IsItemDeactivatedAfterEdit();
  const bool toggled = SkinCheckBox(core::Tr("Masquer"), &place.hidden, art);
  if (changed || released || toggled) {
    place.released = released || toggled || !ImGui::IsAnyItemActive();
    intents.emplace_back(place);
  }
}

// LA RAISON : saisie dans le panneau, elle devient une intention quand le
// champ se quitte.
void DrawWhy(const MapEditorView& view, MapEditorPanelState& panel,
             std::vector<MapEditorIntent>& intents) {
  if (panel.why_of != view.selected) {
    panel.why.fill('\0');
    const std::size_t kept = std::min(view.why.size(), panel.why.size() - 1);
    std::copy_n(view.why.begin(), kept, panel.why.begin());
    panel.why_of = view.selected;
  }
  static_cast<void>(ImGui::InputText(core::Tr("Pourquoi"), panel.why.data(), panel.why.size()));
  if (ImGui::IsItemDeactivatedAfterEdit()) {
    intents.emplace_back(map_editor::SetWhy{
        std::string(panel.why.data(), strnlen(panel.why.data(), panel.why.size()))});
  }
}

void DrawSelection(const MapEditorView& view, MapEditorPanelState& panel,
                   std::vector<MapEditorIntent>& intents, const SkinArt& art) {
  if (!view.selected.has_value()) {
    ImGui::TextDisabled("%s", core::Tr("Cliquez sur un modèle du décor pour le désigner."));
    return;
  }
  ImGui::TextUnformatted(
      std::format("{} {} — {}", core::Tr("Rang"), *view.selected, view.selected_file).c_str());
  ImGui::TextDisabled("%s", std::format("{} {:.3f} {:.3f} {:.3f}", core::Tr("Origine"),
                                        view.origin[0], view.origin[1], view.origin[2])
                                .c_str());
  DrawPlacement(view, intents, art);
  DrawWhy(view, panel, intents);
  if (SkinButton(core::Tr("Rétablir le modèle"), art)) {
    intents.emplace_back(map_editor::Revert{});
  }
}

}  // namespace

void DrawMapEditor(const MapEditorView& view, MapEditorPanelState& panel,
                   std::vector<MapEditorIntent>& intents, const SkinArt& art) {
  if (!view.active) {
    return;
  }
  // Recomposé au changement de langue seulement : voir `WindowName`.
  static WindowName name(kMapEditorTitle, kMapEditorId);
  ImGui::SetNextWindowSize(ImVec2(kFirstWidth, kFirstHeight), ImGuiCond_FirstUseEver);
  ConstrainNextWindowToScreen(name.Get());
  bool open = true;
  const bool body_shown = BeginSkinnedWindow(name.Get(), &open, art);
  if (!open) {
    intents.emplace_back(map_editor::Toggle{});
  }
  if (!body_shown) {
    EndSkinnedWindow();
    return;
  }
  ImGui::PushItemWidth(kControlWidthPixels * ImGui::GetStyle().FontScaleMain);

  ImGui::TextUnformatted(std::format("{} {}", core::Tr("Carte"), view.map_name).c_str());
  ImGui::SameLine();
  SkinHelpMarker(core::Tr("Éditeur de carte"), kMapEditorHelp);

  ImGui::BeginDisabled(view.undo_label.empty());
  if (SkinButton(core::Tr("Annuler"), art)) {
    intents.emplace_back(map_editor::Undo{});
  }
  ImGui::EndDisabled();
  ImGui::SameLine();
  ImGui::BeginDisabled(view.redo_label.empty());
  if (SkinButton(core::Tr("Rétablir"), art)) {
    intents.emplace_back(map_editor::Redo{});
  }
  ImGui::EndDisabled();
  if (!view.undo_label.empty()) {
    ImGui::SameLine();
    ImGui::TextDisabled("%s", view.undo_label.c_str());
  }

  ImGui::SeparatorText(core::Tr("Modèle désigné"));
  DrawSelection(view, panel, intents, art);

  ImGui::SeparatorText(core::Tr("Retouches de cette carte"));
  if (view.edited.empty()) {
    ImGui::TextDisabled("%s", core::Tr("Aucune."));
  }
  for (std::size_t slot = 0; slot < view.edited.size(); ++slot) {
    if (slot > 0) {
      ImGui::SameLine();
    }
    const std::string label = std::to_string(view.edited[slot]);
    if (ImGui::SmallButton(label.c_str())) {
      intents.emplace_back(map_editor::Select{view.edited[slot]});
    }
  }

  ImGui::Separator();
  ImGui::BeginDisabled(!view.unsaved);
  if (SkinButton(core::Tr("Enregistrer"), art)) {
    intents.emplace_back(map_editor::Save{});
  }
  ImGui::EndDisabled();
  ImGui::SameLine();
  if (SkinButton(core::Tr("Quitter l'éditeur"), art)) {
    intents.emplace_back(map_editor::Toggle{});
  }
  if (!view.status.empty()) {
    ImGui::TextDisabled("%s", view.status.c_str());
  }
  ImGui::PopItemWidth();
  EndSkinnedWindow();
}

}  // namespace ro::ui
