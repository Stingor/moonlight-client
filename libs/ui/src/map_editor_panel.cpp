#include "ro/ui/map_editor_panel.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <format>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "ro/core/i18n.h"
#include "ro/ui/list_keys.h"
#include "ro/ui/map_editor_effects_panel.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"
#include "ro/ui/wheel_drag.h"
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

// Le pas des glissières de rotation et d'échelle, par pixel de souris : en
// degrés, en facteur d'échelle. La position a les siens : voir `PositionInCells`.
constexpr float kRotationStep = 0.5F;
constexpr float kScaleStep = 0.005F;
// Et ce que vaut un cran de molette, dix avec Majuscule : un degré, un
// centième d'échelle.
constexpr float kRotationWheelStep = 1.0F;
constexpr float kScaleWheelStep = 0.01F;
constexpr WheelDrag kRotationDrag{
    .drag_step = kRotationStep, .wheel_step = kRotationWheelStep, .format = "%.2f"};
constexpr WheelDrag kScaleDrag{
    .drag_step = kScaleStep, .wheel_step = kScaleWheelStep, .format = "%.3f"};

constexpr const char* kMapEditorTitle = core::TrText("Éditeur de carte");
constexpr const char* kMapEditorId = "map_editor";

constexpr std::array<const char*, 5> kMapEditorHelp = {
    core::TrText("Un clic dans le monde désigne le modèle du décor sous la souris : la plus "
                 "petite boîte traversée gagne. Ses retouches se redésignent aussi depuis la "
                 "liste."),
    core::TrText("Chaque retouche vérifie le rang, le fichier et la position d'origine du modèle : "
                 "une carte changée par un patch n'est pas retouchée de travers."),
    core::TrText("« Pourquoi » est obligatoire : le document refuse une retouche sans raison."),
    core::TrText("« Enregistrer » écrit data/map_edits/<carte>.yml dans le dossier du client. "
                 "Le visuel seulement : la marche reste celle du serveur."),
    core::TrText("Ctrl+Z annule, Ctrl+Y ou Ctrl+Maj+Z rétablit, Ctrl+S enregistre, Ctrl+1 à "
                 "Ctrl+4 choisissent l'outil, Ctrl+D duplique, Ctrl+F centre la caméra, "
                 "Ctrl+flèches poussent, Suppr retire ou masque, Maj aimante un glissé. "
                 "Ctrl+clic fait marcher le personnage."),
};

// Le rayon du cercle d'une lumière ponctuelle, en pixels.
constexpr float kLightMarkerPixels = 7.0F;
// Les pas des glissières d'une lumière : sa portée, et sa couleur.
constexpr float kLightRangeStep = 0.5F;
// Un cran de molette sur la portée : une unité, dix avec Majuscule.
constexpr float kLightRangeWheelStep = 1.0F;
// La plus grande portée : une portée reste positive — le document refuse une
// portée nulle —, et la glissière la borne de ce côté aussi.
constexpr float kLightRangeMax = 2000.0F;

// La hauteur de l'arbre de la scène, en lignes : au-delà, il défile.
constexpr float kTreeRows = 12.0F;
// L'épaisseur des traits tracés par-dessus la vue, en pixels.
constexpr float kLineThicknessPixels = 2.0F;
// La demi-largeur du point d'un sommet, et du sommet désigné, en pixels.
constexpr float kVertexHalfPixels = 2.5F;
constexpr float kSelectedVertexHalfPixels = 4.5F;
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
    case MapEditorLineKind::kVertexSelected:
    case MapEditorLineKind::kLightSelected:
    case MapEditorLineKind::kEffectSelected:
      return kActiveColor;
    case MapEditorLineKind::kVertex:
    case MapEditorLineKind::kLight:
      return kSelectionColor;
    case MapEditorLineKind::kEffect:
      return kAxisXColor;
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
  ImGui::SameLine();
  offer(MapEditorTool::kVertices, core::Tr("Sommets"));
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

// LES NOMS DE FICHIERS DANS LEUR POLICE, le temps d'un bloc : voir
// `MapEditorPanelState::names_font`.
class NamesFont {
 public:
  explicit NamesFont(ImFont* font) : pushed_(font != nullptr) {
    if (pushed_) {
      // Taille nulle : celle du texte courant.
      ImGui::PushFont(font, 0.0F);
    }
  }
  NamesFont(const NamesFont&) = delete;
  NamesFont& operator=(const NamesFont&) = delete;
  ~NamesFont() {
    if (pushed_) {
      ImGui::PopFont();
    }
  }

 private:
  bool pushed_;
};

// LE CLAVIER D'UNE LISTE de l'éditeur, dans sa fenêtre fille focalisée — les
// gestes de toute liste du jeu, voir `PressedListKey` : le rang visé parmi
// `count` lignes, depuis `current`, ou `kListNoEntry`.
int KeyedRank(int current, int count) {
  if (!ImGui::IsWindowFocused()) {
    return kListNoEntry;
  }
  const ListKeyMove move = PressedListKey(/*arrows_only=*/ImGui::GetIO().WantTextInput);
  if (move == ListKeyMove::kNone) {
    return kListNoEntry;
  }
  return ListKeyTarget(current, count, VisibleListRows(ImGui::GetTextLineHeightWithSpacing()),
                       move);
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
  if (view.models == nullptr) {
    return;
  }
  const std::vector<MapEditorModelRow>& rows = *view.models;
  std::vector<std::size_t> shown;
  for (std::size_t slot = 0; slot < rows.size(); ++slot) {
    const MapEditorModelRow& row = rows[slot];
    if ((!panel.edited_only || row.edited) && PassesFilter(row, filter)) {
      shown.push_back(slot);
    }
  }
  const float height = ImGui::GetTextLineHeightWithSpacing() * kTreeRows;
  if (!ImGui::BeginChild("##arbre_scene", ImVec2(0.0F, height), ImGuiChildFlags_Borders)) {
    ImGui::EndChild();
    return;
  }
  int current = kListNoEntry;
  for (std::size_t line = 0; line < shown.size(); ++line) {
    if (view.selected == rows[shown[line]].model_index) {
      current = static_cast<int>(line);
    }
  }
  const int keyed = KeyedRank(current, static_cast<int>(shown.size()));
  if (keyed != kListNoEntry && keyed != current) {
    intents.emplace_back(
        map_editor::Select{rows[shown[static_cast<std::size_t>(keyed)]].model_index});
  }
  {
    const NamesFont names(panel.names_font);
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(shown.size()));
    // La ligne qu'une touche vient de choisir se pose même hors de la vue, pour
    // que la liste la rejoigne.
    if (keyed != kListNoEntry) {
      clipper.IncludeItemByIndex(keyed);
    }
    while (clipper.Step()) {
      for (int line = clipper.DisplayStart; line < clipper.DisplayEnd; ++line) {
        const MapEditorModelRow& row = rows[shown[static_cast<std::size_t>(line)]];
        const std::string label = std::format(
            "{}{} — {}{}##modele{}", row.added ? "+ " : (row.edited ? "✎ " : ""), row.model_index,
            row.file, row.hidden ? std::format(" ({})", core::Tr("masqué")) : std::string(),
            row.model_index);
        const bool selected = view.selected == row.model_index || line == keyed;
        if (ImGui::Selectable(label.c_str(), selected)) {
          intents.emplace_back(map_editor::Select{row.model_index});
        }
        if (line == keyed) {
          KeepLastItemInView();
        }
      }
    }
  }
  PaintWindowScrollbar(art);
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
    int current = kListNoEntry;
    for (std::size_t line = 0; line < shown.size(); ++line) {
      if ((*view.library)[shown[line]] == panel.library_choice) {
        current = static_cast<int>(line);
      }
    }
    const int keyed = KeyedRank(current, static_cast<int>(shown.size()));
    if (keyed != kListNoEntry) {
      panel.library_choice = (*view.library)[shown[static_cast<std::size_t>(keyed)]];
    }
    const NamesFont names(panel.names_font);
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(shown.size()));
    if (keyed != kListNoEntry) {
      clipper.IncludeItemByIndex(keyed);
    }
    while (clipper.Step()) {
      for (int line = clipper.DisplayStart; line < clipper.DisplayEnd; ++line) {
        const std::string& file = (*view.library)[shown[static_cast<std::size_t>(line)]];
        if (ImGui::Selectable(file.c_str(), panel.library_choice == file)) {
          panel.library_choice = file;
        }
        if (line == keyed) {
          KeepLastItemInView();
        }
      }
    }
  }
  PaintWindowScrollbar(art);
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
    if (line.kind == MapEditorLineKind::kVertex ||
        line.kind == MapEditorLineKind::kVertexSelected) {
      const float half =
          line.kind == MapEditorLineKind::kVertex ? kVertexHalfPixels : kSelectedVertexHalfPixels;
      list->AddRectFilled(ImVec2(line.from.x - half, line.from.y - half),
                          ImVec2(line.from.x + half, line.from.y + half), ColorOf(line.kind));
      continue;
    }
    if (line.kind == MapEditorLineKind::kEffect ||
        line.kind == MapEditorLineKind::kEffectSelected) {
      const ImVec2 at(line.from.x, line.from.y);
      list->AddQuad(
          ImVec2(at.x, at.y - kLightMarkerPixels), ImVec2(at.x + kLightMarkerPixels, at.y),
          ImVec2(at.x, at.y + kLightMarkerPixels), ImVec2(at.x - kLightMarkerPixels, at.y),
          ColorOf(line.kind), kLineThicknessPixels);
      continue;
    }
    if (line.kind == MapEditorLineKind::kLight || line.kind == MapEditorLineKind::kLightSelected) {
      list->AddCircle(ImVec2(line.from.x, line.from.y), kLightMarkerPixels, ColorOf(line.kind), 0,
                      kLineThicknessPixels);
      continue;
    }
    list->AddLine(ImVec2(line.from.x, line.from.y), ImVec2(line.to.x, line.to.y),
                  ColorOf(line.kind), kLineThicknessPixels);
  }
}

// L'OUTIL SOMMETS : le sommet désigné, et ce qu'on en fait.
void DrawVertexTool(const MapEditorView& view, std::vector<MapEditorIntent>& intents,
                    const SkinArt& art) {
  ImGui::SeparatorText(core::Tr("Sommets"));
  bool all = view.all_poses;
  if (SkinCheckBox(core::Tr("Toutes les poses du fichier"), &all, art)) {
    intents.emplace_back(map_editor::SetAllPoses{all});
  }
  ImGui::TextDisabled(
      "%s", std::format("{} : {}", core::Tr("Sommets déplacés"), view.moved_vertices).c_str());
  if (view.selected_vertex.empty()) {
    ImGui::TextDisabled("%s", core::Tr("Cliquez sur un point du modèle pour désigner un sommet."));
    return;
  }
  ImGui::TextUnformatted(std::format("{} {}", core::Tr("Sommet"), view.selected_vertex).c_str());
  if (SkinButton(core::Tr("Rétablir le sommet"), art)) {
    intents.emplace_back(map_editor::RevertVertex{});
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
  {
    const WheelDragResult field =
        PositionInCells(core::Tr("Position (cases, hauteur)"), place.position, view.map_cubes);
    changed |= field.changed;
    released |= field.released;
  }
  for (const auto& [label, values, drag] :
       {std::tuple(core::Tr("Rotation (°)"), &place.rotation, &kRotationDrag),
        std::tuple(core::Tr("Échelle"), &place.scale, &kScaleDrag)}) {
    const WheelDragResult field = WheelDragFloat3(label, *values, *drag);
    changed |= field.changed;
    released |= field.released;
  }
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
  // LA SAISIE SE RECHARGE quand le modèle change, ou quand le document change la
  // raison sous un champ qu'on ne tape pas : une annulation, l'outil Sommets
  // qui montre celle du maillage.
  const std::string_view typed(panel.why.data(), strnlen(panel.why.data(), panel.why.size()));
  if (panel.why_of != view.selected || (!panel.why_typing && typed != view.why)) {
    panel.why.fill('\0');
    const std::size_t kept = std::min(view.why.size(), panel.why.size() - 1);
    std::copy_n(view.why.begin(), kept, panel.why.begin());
    panel.why_of = view.selected;
  }
  static_cast<void>(ImGui::InputText(core::Tr("Pourquoi"), panel.why.data(), panel.why.size()));
  panel.why_typing = ImGui::IsItemActive();
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
  {
    const NamesFont names(panel.names_font);
    ImGui::TextUnformatted(
        std::format("{} {} — {}", core::Tr("Rang"), *view.selected, view.selected_file).c_str());
  }
  const std::array<float, 3> origin = CellsOfNative(view.origin, view.map_cubes);
  ImGui::TextDisabled("%s", std::format("{} {:.2f} {:.2f} {:.2f}", core::Tr("Origine"), origin[0],
                                        origin[1], origin[2])
                                .c_str());
  DrawPlacement(view, intents, art);
  DrawWhy(view, panel, intents);
  if (view.tool == MapEditorTool::kVertices) {
    DrawVertexTool(view, intents, art);
  }
  if (SkinButton(view.selected_added ? core::Tr("Retirer le modèle ajouté")
                                     : core::Tr("Rétablir le modèle"),
                 art)) {
    intents.emplace_back(map_editor::Revert{});
  }
  ImGui::SameLine();
  if (SkinButton(core::Tr("Dupliquer"), art)) {
    intents.emplace_back(map_editor::Duplicate{});
  }
  ImGui::SameLine();
  if (SkinButton(core::Tr("Centrer"), art)) {
    intents.emplace_back(map_editor::Focus{});
  }
}

// LES LUMIÈRES PONCTUELLES : la liste, l'ajout, et la lumière désignée — ses
// glissières, sa raison, son retrait.
void DrawLights(const MapEditorView& view, MapEditorPanelState& panel,
                std::vector<MapEditorIntent>& intents, const SkinArt& art) {
  if (SkinButton(core::Tr("Ajouter une lumière"), art)) {
    intents.emplace_back(map_editor::AddLight{});
  }
  for (std::size_t slot = 0; slot < view.lights.size(); ++slot) {
    const MapEditorLight& light = view.lights[slot];
    // Elle se reconnaît à sa position, en unités du RSW.
    const std::string label =
        std::format("{} {} — {:.1f} {:.1f} {:.1f}{}##lumiere{}", core::Tr("Lumière"), slot,
                    CellsOfNative(light.position, view.map_cubes)[0],
                    CellsOfNative(light.position, view.map_cubes)[1],
                    CellsOfNative(light.position, view.map_cubes)[2], WhyTag(light.why), slot);
    const bool chosen = view.selected_light == slot;
    if (ImGui::Selectable(label.c_str(), chosen)) {
      intents.emplace_back(map_editor::SelectLight{chosen ? std::nullopt : std::optional(slot)});
    }
  }
  if (!view.selected_light.has_value() || *view.selected_light >= view.lights.size()) {
    return;
  }
  const std::size_t slot = *view.selected_light;
  // Ses glissières portent les noms de celles du modèle : un autre espace d'ID.
  ImGui::PushID("lumiere");
  map_editor::SetLight set{.slot = slot, .light = view.lights[slot], .released = false};
  bool changed = false;
  bool released = false;
  const WheelDragResult moved =
      PositionInCells(core::Tr("Position (cases, hauteur)"), set.light.position, view.map_cubes);
  changed |= moved.changed;
  released |= moved.released;
  // UNE PASTILLE ET SON SÉLECTEUR : la couleur se choisit à l'œil. Au-delà de
  // 1, une composante éclaire plus fort ; la session borne ce que l'on tape.
  changed |= ImGui::ColorEdit3(core::Tr("Couleur"), set.light.color.data(),
                               ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_Float);
  released |= ImGui::IsItemDeactivatedAfterEdit();
  const WheelDragResult reach = WheelDragFloat(core::Tr("Portée"), set.light.range,
                                               WheelDrag{.drag_step = kLightRangeStep,
                                                         .wheel_step = kLightRangeWheelStep,
                                                         .lowest = kLightRangeStep,
                                                         .highest = kLightRangeMax,
                                                         .format = "%.1f"});
  changed |= reach.changed;
  released |= reach.released;
  bool toggled = SkinCheckBox(core::Tr("La nuit seulement"), &set.light.night_only, art);
  if (set.light.night_only) {
    WheelDragResult hours_moved;
    toggled |= LitHoursFields(set.light.hours, art, hours_moved);
    changed |= hours_moved.changed;
    released |= hours_moved.released;
  }
  toggled |= SkinCheckBox(core::Tr("Vacille"), &set.light.flicker, art);
  const std::string_view typed(panel.light_why.data(),
                               strnlen(panel.light_why.data(), panel.light_why.size()));
  if (panel.light_why_of != view.selected_light ||
      (!panel.light_why_typing && typed != set.light.why)) {
    panel.light_why.fill('\0');
    const std::size_t kept = std::min(set.light.why.size(), panel.light_why.size() - 1);
    std::copy_n(set.light.why.begin(), kept, panel.light_why.begin());
    panel.light_why_of = view.selected_light;
  }
  static_cast<void>(
      ImGui::InputText(core::Tr("Pourquoi"), panel.light_why.data(), panel.light_why.size()));
  panel.light_why_typing = ImGui::IsItemActive();
  if (ImGui::IsItemDeactivatedAfterEdit()) {
    set.light.why = std::string(panel.light_why.data(),
                                strnlen(panel.light_why.data(), panel.light_why.size()));
    changed = true;
    released = true;
  }
  if (changed || released || toggled) {
    set.released = released || toggled || !ImGui::IsAnyItemActive();
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

  if (ImGui::CollapsingHeader(core::Tr("Arbre de la scène"), ImGuiTreeNodeFlags_DefaultOpen)) {
    DrawSceneTree(view, panel, intents, art);
  }

  if (ImGui::CollapsingHeader(core::Tr("Bibliothèque"))) {
    DrawLibrary(view, panel, intents, art);
  }
  if (ImGui::CollapsingHeader(core::Tr("Lumières"))) {
    DrawLights(view, panel, intents, art);
  }
  if (ImGui::CollapsingHeader(core::Tr("Effets"))) {
    DrawEffects(view, panel, intents, art);
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
