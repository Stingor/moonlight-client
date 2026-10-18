#include "map_editor_library_window.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <format>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

#include "ro/core/i18n.h"
#include "ro/ui/model_library_index.h"
#include "ro/ui/skin_popup.h"
#include "ro/ui/skin_tooltip.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"
#include "ro/ui/window_bounds.h"
#include "ro/ui/window_name.h"

#include "imgui.h"
#include "map_editor_names_font.h"

namespace ro::ui {
namespace {

constexpr const char* kLibraryTitle = core::TrText("Bibliothèque de modèles");
constexpr const char* kLibraryId = "model_library";
// La taille au PREMIER affichage seulement : ensuite, celle que le staff lui a
// donnée.
constexpr float kFirstWidth = 760.0F;
constexpr float kFirstHeight = 540.0F;
// La largeur de la colonne de navigation, et celle de la glissière du côté.
constexpr float kNavigationWidthPx = 220.0F;
constexpr float kSliderWidthPx = 150.0F;
// L'écart entre deux cases, et l'épaisseur du cadre de la case choisie.
constexpr float kCellGapPx = 6.0F;
constexpr float kChosenFramePx = 2.0F;
// La croix d'un modèle qui ne se lit pas : sa marge, en part du côté.
constexpr float kFailedMarginShare = 0.3F;
// Le point d'une vignette qui vient : son rayon, en part du côté.
constexpr float kPendingDotShare = 0.04F;
// Les rangées demandées hors de la vue : au-dessous, où l'on défile
// d'ordinaire, puis au-dessus.
constexpr int kAheadRows = 2;
constexpr int kBehindRows = 1;

constexpr std::array<const char*, 3> kLibraryHelp = {
    core::TrText("Un clic choisit un modèle ; un double-clic, ou Entrée, l'ajoute à la carte, là "
                 "où regarde la caméra."),
    core::TrText("Un clic droit sur une vignette l'ajoute aux favoris ou l'en retire. Les modèles "
                 "posés récemment se retrouvent sous « Récents »."),
    core::TrText("Le filtre cherche chaque mot dans le chemin du modèle. Les vignettes se gardent "
                 "dans le dossier du client : elles reviennent aussitôt à l'ouverture suivante."),
};

std::string_view FilterText(const ModelLibraryWindowState& window) {
  return {window.filter.data(), strnlen(window.filter.data(), window.filter.size())};
}

const std::shared_ptr<const std::vector<std::string>>& SourceList(const ModelLibraryView& library,
                                                                  ModelLibrarySource source) {
  switch (source) {
    case ModelLibrarySource::kFolder:
      return library.files;
    case ModelLibrarySource::kFavorites:
      return library.favorites;
    case ModelLibrarySource::kRecents:
      return library.recents;
  }
  return library.files;
}

bool IsFavorite(const ModelLibraryView& library, const std::string& file) {
  return library.favorites != nullptr && std::ranges::binary_search(*library.favorites, file);
}

// LE TRI MONTRÉ, refait quand la source, le dossier, le filtre ou la liste
// changent. Rend vrai quand le STAFF a changé ce qu'il regarde — et non une
// liste qui s'est allongée sous lui : la grille repart alors du haut.
bool RefreshShown(const ModelLibraryView& library, ModelLibraryWindowState& window) {
  const std::shared_ptr<const std::vector<std::string>>& list = SourceList(library, window.source);
  const std::string_view filter = FilterText(window);
  const std::string_view folder =
      window.source == ModelLibrarySource::kFolder ? std::string_view(window.folder) : "";
  const bool looked_elsewhere = window.source != window.shown_source ||
                                folder != window.shown_folder || filter != window.shown_filter;
  if (!looked_elsewhere && list == window.shown_list) {
    return false;
  }
  window.shown_list = list;
  window.shown_source = window.source;
  window.shown_folder = std::string(folder);
  window.shown_filter = std::string(filter);
  window.shown.clear();
  if (list != nullptr) {
    window.shown = FilterModelFiles(*list, folder, filter);
  }
  return looked_elsewhere;
}

// UN DOSSIER DE L'ARBRE et, ouvert, ses sous-dossiers. Un clic le montre.
void DrawFolder(const ModelLibraryTree& tree, std::size_t rank, ModelLibraryWindowState& window) {
  const ModelFolder& folder = tree.folders[rank];
  ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick |
                             ImGuiTreeNodeFlags_SpanAvailWidth;
  if (folder.children.empty()) {
    flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
  }
  if (rank == 0) {
    flags |= ImGuiTreeNodeFlags_DefaultOpen;
  }
  if (window.source == ModelLibrarySource::kFolder && window.folder == folder.prefix) {
    flags |= ImGuiTreeNodeFlags_Selected;
  }
  const std::string label =
      std::format("{} ({})##{}", rank == 0 ? core::Tr("Tous les modèles") : folder.name.c_str(),
                  folder.model_count, folder.prefix);
  const bool opened = ImGui::TreeNodeEx(label.c_str(), flags);
  if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
    window.source = ModelLibrarySource::kFolder;
    window.folder = folder.prefix;
  }
  if (opened && !folder.children.empty()) {
    for (const std::size_t child : folder.children) {
      DrawFolder(tree, child, window);
    }
    ImGui::TreePop();
  }
}

// LA NAVIGATION : les favoris, les récents, puis l'arbre des dossiers.
void DrawNavigation(const ModelLibraryView& library, MapEditorPanelState& panel) {
  ModelLibraryWindowState& window = panel.library_window;
  const auto offer = [&](ModelLibrarySource source, const char* label,
                         const std::shared_ptr<const std::vector<std::string>>& list) {
    const std::string text =
        std::format("{} ({})", label, list != nullptr ? list->size() : std::size_t{0});
    if (ImGui::Selectable(text.c_str(), window.source == source)) {
      window.source = source;
    }
  };
  offer(ModelLibrarySource::kFavorites, core::Tr("Favoris"), library.favorites);
  offer(ModelLibrarySource::kRecents, core::Tr("Récents"), library.recents);
  ImGui::Separator();
  if (library.tree == nullptr || library.tree->folders.empty()) {
    return;
  }
  // Les dossiers portent des noms coréens.
  const NamesFont names(panel.names_font);
  DrawFolder(*library.tree, 0, window);
}

// LE MENU D'UNE CASE : l'ajouter à la carte, basculer son favori.
void DrawCellMenu(const ModelLibraryView& library, ModelLibraryWindowState& window,
                  const std::string& file, std::vector<MapEditorIntent>& intents) {
  if (!BeginSkinPopupContextItem(nullptr)) {
    return;
  }
  if (ImGui::MenuItem(core::Tr("Ajouter à la carte"))) {
    intents.emplace_back(map_editor::AddModel{file});
  }
  if (ImGui::MenuItem(IsFavorite(library, file) ? core::Tr("Retirer des favoris")
                                                : core::Tr("Ajouter aux favoris"))) {
    window.favorite_toggles.push_back(file);
  }
  EndSkinPopup();
}

// LE DESSIN D'UNE CASE, posée par le bouton invisible qui la précède : la
// vignette, ou son attente, ou la croix d'un modèle illisible ; puis son nom.
void PaintCell(const MapEditorPreview* vignette, std::string_view stem, float cell_px, bool chosen,
               ImFont* names_font) {
  ImDrawList* list = ImGui::GetWindowDrawList();
  const ImVec2 corner = ImGui::GetItemRectMin();
  const ImVec2 opposite = ImGui::GetItemRectMax();
  const ImVec2 picture_far(corner.x + cell_px, corner.y + cell_px);
  const ImGuiCol back = chosen                   ? ImGuiCol_Header
                        : ImGui::IsItemHovered() ? ImGuiCol_FrameBgHovered
                                                 : ImGuiCol_FrameBg;
  list->AddRectFilled(corner, picture_far, ImGui::GetColorU32(back));
  if (vignette != nullptr && vignette->image.usable()) {
    list->AddImage(vignette->image.id, corner, picture_far);
  } else if (vignette != nullptr && vignette->failed) {
    const float margin = cell_px * kFailedMarginShare;
    const ImU32 cross = ImGui::GetColorU32(ImGuiCol_TextDisabled);
    list->AddLine(ImVec2(corner.x + margin, corner.y + margin),
                  ImVec2(picture_far.x - margin, picture_far.y - margin), cross, kChosenFramePx);
    list->AddLine(ImVec2(picture_far.x - margin, corner.y + margin),
                  ImVec2(corner.x + margin, picture_far.y - margin), cross, kChosenFramePx);
  } else {
    list->AddCircleFilled(
        ImVec2((corner.x + picture_far.x) * 0.5F, (corner.y + picture_far.y) * 0.5F),
        cell_px * kPendingDotShare, ImGui::GetColorU32(ImGuiCol_TextDisabled));
  }
  if (chosen) {
    list->AddRect(corner, picture_far, ImGui::GetColorU32(ImGuiCol_Text), 0.0F, 0, kChosenFramePx);
  }
  const NamesFont names(names_font);
  const ImVec4 clip(corner.x, picture_far.y, opposite.x, opposite.y);
  list->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(corner.x, picture_far.y),
                ImGui::GetColorU32(ImGuiCol_Text), stem.data(), stem.data() + stem.size(), 0.0F,
                &clip);
}

// LA GRILLE : seules les cases visibles se posent, et la fenêtre note les
// vignettes qu'elle veut — les visibles d'abord, puis leurs voisines.
void DrawGrid(const MapEditorView& view, MapEditorPanelState& panel,
              std::vector<MapEditorIntent>& intents, bool from_top) {
  ModelLibraryWindowState& window = panel.library_window;
  const std::vector<std::string>* list = window.shown_list.get();
  if (list == nullptr || window.shown.empty()) {
    switch (window.source) {
      case ModelLibrarySource::kFavorites:
        ImGui::TextDisabled("%s", core::Tr("Aucun favori : un clic droit sur une vignette l'y "
                                           "ajoute."));
        break;
      case ModelLibrarySource::kRecents:
        ImGui::TextDisabled("%s", core::Tr("Aucun modèle posé récemment."));
        break;
      case ModelLibrarySource::kFolder:
        ImGui::TextDisabled("%s", core::Tr("Aucun modèle ne passe le filtre."));
        break;
    }
    return;
  }
  if (from_top) {
    ImGui::SetScrollY(0.0F);
  }
  std::unordered_map<std::string_view, const MapEditorPreview*> ready;
  ready.reserve(view.library.vignettes.size());
  for (const MapEditorPreview& vignette : view.library.vignettes) {
    ready.emplace(vignette.file, &vignette);
  }
  const float cell = window.cell_px;
  const float label = ImGui::GetTextLineHeight();
  const float row = cell + label + kCellGapPx;
  const ImVec2 start = ImGui::GetCursorPos();
  const int columns = GridColumns(ImGui::GetContentRegionAvail().x, cell, kCellGapPx);
  const std::size_t count = window.shown.size();
  const GridSpan visible = VisibleGridCells(count, columns, row, ImGui::GetScrollY() - start.y,
                                            ImGui::GetWindowHeight());
  const auto width = static_cast<std::size_t>(columns);
  for (std::size_t cell_rank = visible.first; cell_rank < visible.end; ++cell_rank) {
    const std::string& file = (*list)[window.shown[cell_rank]];
    ImGui::PushID(static_cast<int>(cell_rank));
    ImGui::SetCursorPos(
        ImVec2(start.x + static_cast<float>(cell_rank % width) * (cell + kCellGapPx),
               start.y + static_cast<float>(cell_rank / width) * row));
    if (ImGui::InvisibleButton("##case", ImVec2(cell, cell + label))) {
      panel.library_choice = file;
    }
    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
      intents.emplace_back(map_editor::AddModel{file});
    }
    const auto found = ready.find(file);
    PaintCell(found != ready.end() ? found->second : nullptr, ModelStem(file), cell,
              panel.library_choice == file, panel.names_font);
    PreviewOnHover(view, panel, file);
    DrawCellMenu(view.library, window, file, intents);
    ImGui::PopID();
  }
  // La hauteur entière de la grille, pour que l'ascenseur la parcoure.
  const std::size_t rows = (count + width - 1) / width;
  ImGui::SetCursorPos(ImVec2(start.x, start.y + static_cast<float>(rows) * row));
  ImGui::Dummy(ImVec2(0.0F, 0.0F));
  for (const std::size_t cell_rank :
       VignetteRequestOrder(visible, count, columns, kAheadRows, kBehindRows,
                            ModelLibraryWindowState::kMostWanted)) {
    window.wanted.push_back((*list)[window.shown[cell_rank]]);
  }
}

// LE PIED : le modèle choisi, et ce qu'on en fait.
void DrawFooter(const MapEditorView& view, MapEditorPanelState& panel,
                std::vector<MapEditorIntent>& intents, const SkinArt& art) {
  ModelLibraryWindowState& window = panel.library_window;
  const std::string& chosen = panel.library_choice;
  ImGui::BeginDisabled(chosen.empty());
  const bool keyed = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
                     !ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Enter);
  if (SkinButton(core::Tr("Ajouter à la carte"), art) || (keyed && !chosen.empty())) {
    intents.emplace_back(map_editor::AddModel{chosen});
  }
  ImGui::SameLine();
  if (SkinButton(IsFavorite(view.library, chosen) ? core::Tr("Retirer des favoris")
                                                  : core::Tr("Ajouter aux favoris"),
                 art)) {
    window.favorite_toggles.push_back(chosen);
  }
  ImGui::EndDisabled();
  ImGui::SameLine();
  ImGui::TextDisabled(
      "%s", std::format("{} : {}", core::Tr("Modèles montrés"), window.shown.size()).c_str());
  if (!chosen.empty()) {
    ImGui::SameLine();
    const NamesFont names(panel.names_font);
    ImGui::TextUnformatted(chosen.c_str());
  }
}

}  // namespace

void ForgetModelLibraryWishes(ModelLibraryWindowState& window) {
  window.wanted.clear();
  window.favorite_toggles.clear();
  window.cell_px_committed = false;
}

void PreviewOnHover(const MapEditorView& view, MapEditorPanelState& panel,
                    const std::string& file) {
  if (!ImGui::IsItemHovered()) {
    return;
  }
  panel.hovered_model = file;
  const MapEditorPreview& preview = view.preview;
  const bool ready = preview.file == file;
  if (!BeginSkinTooltip(ready && preview.image.usable() ? preview.image.width : 0.0F)) {
    return;
  }
  {
    const NamesFont names(panel.names_font);
    ImGui::TextUnformatted(file.c_str());
  }
  if (ready && preview.image.usable()) {
    ImGui::Image(preview.image.id, ImVec2(preview.image.width, preview.image.height));
  } else if (ready && preview.failed) {
    ImGui::TextDisabled("%s", core::Tr("Ce modèle ne se lit pas."));
  } else {
    ImGui::TextDisabled("%s", core::Tr("Aperçu…"));
  }
  EndSkinTooltip();
}

void DrawModelLibraryWindow(const MapEditorView& view, MapEditorPanelState& panel,
                            std::vector<MapEditorIntent>& intents, const SkinArt& art) {
  ModelLibraryWindowState& window = panel.library_window;
  ForgetModelLibraryWishes(window);
  if (!window.open) {
    return;
  }
  // Recomposé au changement de langue seulement : voir `WindowName`.
  static WindowName name(kLibraryTitle, kLibraryId);
  ImGui::SetNextWindowSize(ImVec2(kFirstWidth, kFirstHeight), ImGuiCond_FirstUseEver);
  ConstrainNextWindowToScreen(name.Get());
  bool open = true;
  const bool body_shown = BeginSkinnedWindow(name.Get(), &open, art);
  window.open = open;
  if (!body_shown) {
    EndSkinnedWindow();
    return;
  }
  if (view.library.files == nullptr || view.library.files->empty()) {
    ImGui::TextDisabled("%s", core::Tr("Aucun modèle dans l'installation."));
    EndSkinnedWindow();
    return;
  }
  const float scale = ImGui::GetStyle().FontScaleMain;
  const float footer = SkinButtonHeight(art) + ImGui::GetStyle().ItemSpacing.y;
  if (ImGui::BeginChild("##navigation", ImVec2(kNavigationWidthPx * scale, -footer),
                        ImGuiChildFlags_Borders)) {
    DrawNavigation(view.library, panel);
  }
  PaintWindowScrollbar(art);
  ImGui::EndChild();
  ImGui::SameLine();
  ImGui::BeginGroup();
  ImGui::SetNextItemWidth(-(kSliderWidthPx * scale + ImGui::GetStyle().ItemSpacing.x * 2.0F +
                            ImGui::CalcTextSize(core::Tr("Taille")).x));
  static_cast<void>(ImGui::InputTextWithHint("##filtre_grille", core::Tr("Filtrer…"),
                                             window.filter.data(), window.filter.size()));
  ImGui::SameLine();
  ImGui::SetNextItemWidth(kSliderWidthPx * scale);
  window.cell_px_moved |=
      SkinSliderFloat(core::Tr("Taille"), &window.cell_px, ModelLibraryWindowState::kSmallestCellPx,
                      ModelLibraryWindowState::kLargestCellPx, art, "%.0f px");
  window.cell_px = std::clamp(window.cell_px, ModelLibraryWindowState::kSmallestCellPx,
                              ModelLibraryWindowState::kLargestCellPx);
  if (window.cell_px_moved && !ImGui::IsAnyItemActive()) {
    window.cell_px_moved = false;
    window.cell_px_committed = true;
  }
  const bool from_top = RefreshShown(view.library, window);
  if (ImGui::BeginChild("##grille", ImVec2(0.0F, -footer), ImGuiChildFlags_Borders)) {
    DrawGrid(view, panel, intents, from_top);
  }
  PaintWindowScrollbar(art);
  ImGui::EndChild();
  ImGui::EndGroup();
  DrawFooter(view, panel, intents, art);
  ImGui::SameLine();
  SkinHelpMarker(core::Tr("Bibliothèque de modèles"), kLibraryHelp);
  EndSkinnedWindow();
}

}  // namespace ro::ui
