#include "ro/ui/map_editor_panel.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <format>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

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

// La hauteur de l'arbre de la scène, en lignes : au-delà, il défile.
constexpr float kTreeRows = 12.0F;
// L'épaisseur des traits tracés par-dessus la vue, en pixels.
constexpr float kLineThicknessPixels = 2.0F;
// Les couleurs des traits, en RGBA : la boîte du désigné, les trois axes —
// rouge, vert, bleu, comme partout ailleurs —, et la poignée saisie.
constexpr ImU32 kSelectionColor = IM_COL32(255, 210, 60, 230);
constexpr ImU32 kAxisXColor = IM_COL32(230, 70, 70, 255);
constexpr ImU32 kAxisYColor = IM_COL32(80, 210, 80, 255);
constexpr ImU32 kAxisZColor = IM_COL32(80, 130, 240, 255);
constexpr ImU32 kActiveColor = IM_COL32(255, 255, 255, 255);

ImU32 ColorOf(MapEditorLineKind kind) {
  switch (kind) {
    case MapEditorLineKind::kSelection:
      return kSelectionColor;
    case MapEditorLineKind::kAxisX:
      return kAxisXColor;
    case MapEditorLineKind::kAxisY:
      return kAxisYColor;
    case MapEditorLineKind::kAxisZ:
      return kAxisZColor;
    case MapEditorLineKind::kActive:
      return kActiveColor;
  }
  return kSelectionColor;
}

// LA BARRE D'OUTILS : l'outil des poignées.
void DrawToolbar(const MapEditorView& view, std::vector<MapEditorIntent>& intents) {
  const auto offer = [&](MapEditorTool tool, const char* label) {
    if (ImGui::RadioButton(label, view.tool == tool)) {
      intents.emplace_back(map_editor::SetTool{tool});
    }
  };
  offer(MapEditorTool::kMove, core::Tr("Déplacer"));
  ImGui::SameLine();
  offer(MapEditorTool::kRotate, core::Tr("Tourner"));
  ImGui::SameLine();
  offer(MapEditorTool::kScale, core::Tr("Échelle"));
}

// Le modèle passe-t-il le filtre ? Un morceau de son fichier, sans égard à la
// casse ASCII, ou son rang écrit en entier.
bool PassesFilter(const MapEditorModelRow& row, std::string_view filter) {
  if (filter.empty()) {
    return true;
  }
  if (std::to_string(row.model_index) == filter) {
    return true;
  }
  const auto lower = [](char letter) {
    return letter >= 'A' && letter <= 'Z' ? static_cast<char>(letter - 'A' + 'a') : letter;
  };
  const auto found = std::ranges::search(row.file, filter, {}, lower, lower);
  return !found.empty();
}

// L'ARBRE DE LA SCÈNE : tous les modèles de la carte, filtrables. Un clic
// désigne ; les retouchés sont marqués, et les masqués le disent.
void DrawSceneTree(const MapEditorView& view, MapEditorPanelState& panel,
                   std::vector<MapEditorIntent>& intents, const SkinArt& art) {
  static_cast<void>(ImGui::InputTextWithHint("##filtre_arbre", core::Tr("Filtrer…"),
                                             panel.filter.data(), panel.filter.size()));
  ImGui::SameLine();
  static_cast<void>(SkinCheckBox(core::Tr("Retouchés"), &panel.edited_only, art));
  const std::string_view filter(panel.filter.data(),
                                strnlen(panel.filter.data(), panel.filter.size()));
  std::vector<std::size_t> shown;
  for (std::size_t slot = 0; slot < view.models.size(); ++slot) {
    const MapEditorModelRow& row = view.models[slot];
    if ((!panel.edited_only || row.edited) && PassesFilter(row, filter)) {
      shown.push_back(slot);
    }
  }
  const float height = ImGui::GetTextLineHeightWithSpacing() * kTreeRows;
  if (!ImGui::BeginChild("##arbre_scene", ImVec2(0.0F, height), ImGuiChildFlags_Borders)) {
    ImGui::EndChild();
    return;
  }
  ImGuiListClipper clipper;
  clipper.Begin(static_cast<int>(shown.size()));
  while (clipper.Step()) {
    for (int line = clipper.DisplayStart; line < clipper.DisplayEnd; ++line) {
      const MapEditorModelRow& row = view.models[shown[static_cast<std::size_t>(line)]];
      const std::string label = std::format(
          "{}{} — {}{}##modele{}", row.added ? "+ " : (row.edited ? "✎ " : ""), row.model_index,
          row.file, row.hidden ? std::format(" ({})", core::Tr("masqué")) : std::string(),
          row.model_index);
      const bool selected = view.selected == row.model_index;
      if (ImGui::Selectable(label.c_str(), selected)) {
        intents.emplace_back(map_editor::Select{row.model_index});
      }
    }
  }
  ImGui::EndChild();
}

// LA BIBLIOTHÈQUE : les modèles de l'installation, filtrables ; le choisi
// s'ajoute à la carte, là où l'appelant le pose.
void DrawLibrary(const MapEditorView& view, MapEditorPanelState& panel,
                 std::vector<MapEditorIntent>& intents, const SkinArt& art) {
  if (view.library == nullptr || view.library->empty()) {
    ImGui::TextDisabled("%s", core::Tr("Aucun modèle dans l'installation."));
    return;
  }
  static_cast<void>(ImGui::InputTextWithHint("##filtre_bibliotheque", core::Tr("Filtrer…"),
                                             panel.library_filter.data(),
                                             panel.library_filter.size()));
  const std::string_view filter(panel.library_filter.data(),
                                strnlen(panel.library_filter.data(), panel.library_filter.size()));
  std::vector<std::size_t> shown;
  for (std::size_t slot = 0; slot < view.library->size(); ++slot) {
    if (PassesFilter(MapEditorModelRow{.file = (*view.library)[slot]}, filter)) {
      shown.push_back(slot);
    }
  }
  const float height = ImGui::GetTextLineHeightWithSpacing() * kTreeRows;
  if (ImGui::BeginChild("##bibliotheque", ImVec2(0.0F, height), ImGuiChildFlags_Borders)) {
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(shown.size()));
    while (clipper.Step()) {
      for (int line = clipper.DisplayStart; line < clipper.DisplayEnd; ++line) {
        const std::string& file = (*view.library)[shown[static_cast<std::size_t>(line)]];
        if (ImGui::Selectable(file.c_str(), panel.library_choice == file)) {
          panel.library_choice = file;
        }
      }
    }
  }
  ImGui::EndChild();
  ImGui::BeginDisabled(panel.library_choice.empty());
  if (SkinButton(core::Tr("Ajouter à la carte"), art)) {
    intents.emplace_back(map_editor::AddModel{panel.library_choice});
  }
  ImGui::EndDisabled();
}

// CE QUE LE MODE TRACE PAR-DESSUS LA VUE, sous les fenêtres.
void DrawLines(const MapEditorView& view) {
  ImDrawList* list = ImGui::GetBackgroundDrawList();
  for (const MapEditorLine& line : view.lines) {
    list->AddLine(ImVec2(line.from.x, line.from.y), ImVec2(line.to.x, line.to.y),
                  ColorOf(line.kind), kLineThicknessPixels);
  }
}

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
  if (SkinButton(view.selected_added ? core::Tr("Retirer le modèle ajouté")
                                     : core::Tr("Rétablir le modèle"),
                 art)) {
    intents.emplace_back(map_editor::Revert{});
  }
}

}  // namespace

void DrawMapEditor(const MapEditorView& view, MapEditorPanelState& panel,
                   std::vector<MapEditorIntent>& intents, const SkinArt& art) {
  if (!view.active) {
    return;
  }
  DrawLines(view);
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

  DrawToolbar(view, intents);

  ImGui::SeparatorText(core::Tr("Modèle désigné"));
  DrawSelection(view, panel, intents, art);

  ImGui::SeparatorText(core::Tr("Arbre de la scène"));
  DrawSceneTree(view, panel, intents, art);

  if (ImGui::CollapsingHeader(core::Tr("Bibliothèque"))) {
    DrawLibrary(view, panel, intents, art);
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
