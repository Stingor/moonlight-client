// LA SECTION « SONS » DU PANNEAU DE L'ÉDITEUR DE CARTE : les sources de son de
// la carte, retouchées marquées ; les sons ajoutés ; le choix d'un `.wav` de
// l'installation, qu'on écoute avant de l'ajouter ; et le son désigné — sa
// place, son volume, sa portée, son cycle, la nuit, sa raison.

#include "ro/ui/map_editor_sounds_panel.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "ro/core/i18n.h"
#include "ro/ui/map_editor_effects_panel.h"
#include "ro/ui/map_editor_row_menu.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/triple_clipboard.h"
#include "ro/ui/wheel_drag.h"

#include "imgui.h"

namespace ro::ui {
namespace {

// La hauteur de la liste des fichiers, en lignes : au-delà, elle défile.
constexpr float kLibraryRows = 10.0F;
// Le volume, de 0 à 1 : un centième par pixel, cinq par cran de molette.
constexpr WheelDrag kVolumeDrag{
    .drag_step = 0.01F, .wheel_step = 0.05F, .lowest = 0.0F, .highest = 1.0F, .format = "%.2f"};
// La portée, en unités du RSW, strictement positive.
constexpr float kSmallestRange = 1.0F;
constexpr float kLargestRange = 2000.0F;
constexpr WheelDrag kRangeDrag{.drag_step = 0.5F,
                               .wheel_step = 5.0F,
                               .lowest = kSmallestRange,
                               .highest = kLargestRange,
                               .format = "%.1f"};
// Le cycle, en secondes, strictement positif.
constexpr float kShortestCycle = 0.1F;
constexpr float kLongestCycle = 600.0F;
constexpr WheelDrag kCycleDrag{.drag_step = 0.05F,
                               .wheel_step = 0.5F,
                               .lowest = kShortestCycle,
                               .highest = kLongestCycle,
                               .format = "%.2f s"};
// L'emprise, en unités entières du RSW.
constexpr int kLargestFootprint = 2000;

// Les noms de fichiers dans leur police, le temps d'un bloc : voir
// `MapEditorPanelState::names_font`.
class NamesFontScope {
 public:
  explicit NamesFontScope(ImFont* font) : pushed_(font != nullptr) {
    if (pushed_) {
      ImGui::PushFont(font, 0.0F);
    }
  }
  NamesFontScope(const NamesFontScope&) = delete;
  NamesFontScope& operator=(const NamesFontScope&) = delete;
  ~NamesFontScope() {
    if (pushed_) {
      ImGui::PopFont();
    }
  }

 private:
  bool pushed_;
};

// Le fichier passe-t-il le filtre ? Un morceau de son nom, sans casse ASCII.
bool PassesFilter(std::string_view file, std::string_view filter) {
  const auto lower = [](char letter) {
    return letter >= 'A' && letter <= 'Z' ? static_cast<char>(letter - 'A' + 'a') : letter;
  };
  return filter.empty() || !std::ranges::search(file, filter, {}, lower, lower).empty();
}

// LA LIGNE D'UN SON dans sa liste, et le menu de son clic droit.
void SoundRow(const MapEditorView& view, const MapEditorSound& sound,
              std::vector<MapEditorIntent>& intents) {
  const std::array<float, 3> cells = CellsOfNative(sound.position, view.map_cubes);
  const std::string label =
      std::format("{}{} — {} — {:.1f} {:.1f} {:.1f}{}{}##{}{}", sound.edited ? "✎ " : "",
                  sound.key.index, sound.file, cells[0], cells[1], cells[2],
                  sound.hidden ? std::format(" ({})", core::Tr("masqué")) : std::string(),
                  WhyTag(sound.why), sound.key.added ? "son_ajoute" : "son_carte", sound.key.index);
  const bool chosen = view.selected_sound == sound.key;
  if (ImGui::Selectable(label.c_str(), chosen)) {
    intents.emplace_back(map_editor::SelectSound{chosen ? std::nullopt : std::optional(sound.key)});
  }
  const char* remove_label = core::Tr("Supprimer");
  if (!sound.key.added) {
    remove_label = sound.hidden ? core::Tr("Afficher") : core::Tr("Masquer");
  }
  DrawMapEditorRowMenu(
      MapEditorRowMenu{
          .select = map_editor::SelectSound{sound.key},
          .remove_label = remove_label,
          .revert = !sound.key.added && sound.edited
                        ? std::optional<MapEditorIntent>(map_editor::RevertSound{sound.key})
                        : std::nullopt,
          .cells = cells,
          .move_to = [&sound, cubes = view.map_cubes](
                         const std::array<float, 3>& pasted) -> MapEditorIntent {
            return map_editor::MoveSound{
                .key = sound.key, .native = NativeOfCells(pasted, cubes), .released = true};
          }},
      intents);
}

// LE CHOIX D'UN FICHIER parmi les `.wav` de l'installation, filtrable.
void DrawSoundLibrary(const MapEditorView& view, MapEditorPanelState& panel, const SkinArt& art) {
  MapEditorSoundPanelState& state = panel.sounds;
  if (view.sound_library == nullptr || view.sound_library->empty()) {
    ImGui::TextDisabled("%s", core::Tr("Aucun son dans l'installation."));
    return;
  }
  static_cast<void>(ImGui::InputTextWithHint("##filtre_sons", core::Tr("Filtrer…"),
                                             state.filter.data(), state.filter.size()));
  const std::string_view filter(state.filter.data(),
                                strnlen(state.filter.data(), state.filter.size()));
  std::vector<std::size_t> shown;
  for (std::size_t slot = 0; slot < view.sound_library->size(); ++slot) {
    if (PassesFilter((*view.sound_library)[slot], filter)) {
      shown.push_back(slot);
    }
  }
  const float height = ImGui::GetTextLineHeightWithSpacing() * kLibraryRows;
  if (ImGui::BeginChild("##fichiers_sons", ImVec2(0.0F, height), ImGuiChildFlags_Borders)) {
    const NamesFontScope names(panel.names_font);
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(shown.size()));
    while (clipper.Step()) {
      for (int line = clipper.DisplayStart; line < clipper.DisplayEnd; ++line) {
        const std::string& file = (*view.sound_library)[shown[static_cast<std::size_t>(line)]];
        if (ImGui::Selectable(file.c_str(), state.choice == file)) {
          state.choice = file;
        }
      }
    }
  }
  PaintWindowScrollbar(art);
  ImGui::EndChild();
}

// La raison du son désigné : rechargée quand un autre son est désigné, ou que
// le document la change hors saisie. Vrai quand une saisie s'achève.
bool SoundWhyField(const MapEditorSound& shown, MapEditorSoundPanelState& state, std::string& why) {
  const std::string_view typed(state.why.data(), strnlen(state.why.data(), state.why.size()));
  if (state.why_of != shown.key || (!state.why_typing && typed != shown.why)) {
    state.why.fill('\0');
    const std::size_t kept = std::min(shown.why.size(), state.why.size() - 1);
    std::copy_n(shown.why.begin(), kept, state.why.begin());
    state.why_of = shown.key;
  }
  static_cast<void>(ImGui::InputText(core::Tr("Pourquoi"), state.why.data(), state.why.size()));
  state.why_typing = ImGui::IsItemActive();
  if (!ImGui::IsItemDeactivatedAfterEdit()) {
    return false;
  }
  why = std::string(state.why.data(), strnlen(state.why.data(), state.why.size()));
  return true;
}

// LE SON DÉSIGNÉ : ses champs, et ce qu'on en fait.
void DrawSelectedSound(const MapEditorView& view, const MapEditorSound& shown,
                       MapEditorPanelState& panel, std::vector<MapEditorIntent>& intents,
                       const SkinArt& art) {
  ImGui::PushID("son");
  ImGui::SeparatorText(shown.key.added ? core::Tr("Son ajouté désigné")
                                       : core::Tr("Son de la carte désigné"));
  {
    const NamesFontScope names(panel.names_font);
    ImGui::TextUnformatted(shown.file.c_str());
  }
  map_editor::SetSound set{.sound = shown, .released = false};
  bool changed = false;
  bool released = false;
  bool toggled = false;
  if (shown.key.added) {
    ImGui::BeginDisabled(panel.sounds.choice.empty() || panel.sounds.choice == shown.file);
    if (SkinButton(core::Tr("Prendre le fichier choisi"), art)) {
      set.sound.file = panel.sounds.choice;
      toggled = true;
    }
    ImGui::EndDisabled();
  }
  const WheelDragResult moved =
      PositionInCells(core::Tr("Position (cases, hauteur)"), set.sound.position, view.map_cubes);
  toggled |= PositionClipboardMenu("position", set.sound.position, view.map_cubes);
  for (const WheelDragResult field :
       {moved, WheelDragFloat(core::Tr("Volume"), set.sound.volume, kVolumeDrag),
        WheelDragFloat(core::Tr("Portée"), set.sound.range, kRangeDrag),
        WheelDragFloat(core::Tr("Cycle"), set.sound.cycle_seconds, kCycleDrag)}) {
    changed |= field.changed;
    released |= field.released;
  }
  if (shown.key.added) {
    std::array<int, 2> footprint{set.sound.width, set.sound.height};
    changed |= ImGui::DragInt2(core::Tr("Emprise"), footprint.data(), 1.0F, 0, kLargestFootprint);
    released |= ImGui::IsItemDeactivatedAfterEdit();
    set.sound.width = footprint[0];
    set.sound.height = footprint[1];
  } else {
    toggled |= SkinCheckBox(core::Tr("Masquer"), &set.sound.hidden, art);
  }
  toggled |= SkinCheckBox(core::Tr("La nuit seulement"), &set.sound.night_only, art);
  if (set.sound.night_only) {
    WheelDragResult hours_moved;
    toggled |= LitHoursFields(set.sound.hours, art, hours_moved);
    changed |= hours_moved.changed;
    released |= hours_moved.released;
  }
  if (SoundWhyField(shown, panel.sounds, set.sound.why)) {
    changed = true;
    released = true;
  }
  if (changed || released || toggled) {
    set.released = released || toggled || !ImGui::IsAnyItemActive();
    intents.emplace_back(std::move(set));
  }
  if (SkinButton(core::Tr("Écouter"), art)) {
    intents.emplace_back(map_editor::ListenSound{.file = shown.file, .volume = shown.volume});
  }
  ImGui::SameLine();
  if (shown.key.added) {
    if (SkinButton(core::Tr("Retirer le son"), art)) {
      intents.emplace_back(map_editor::RevertSound{shown.key});
    }
  } else {
    ImGui::BeginDisabled(!shown.edited);
    if (SkinButton(core::Tr("Rétablir le son"), art)) {
      intents.emplace_back(map_editor::RevertSound{shown.key});
    }
    ImGui::EndDisabled();
  }
  if (SkinButton(core::Tr("Centrer"), art)) {
    intents.emplace_back(map_editor::Focus{});
  }
  ImGui::SameLine();
  if (SkinButton(core::Tr("Dupliquer"), art)) {
    intents.emplace_back(map_editor::Duplicate{});
  }
  ImGui::PopID();
}

}  // namespace

void DrawSounds(const MapEditorView& view, MapEditorPanelState& panel,
                std::vector<MapEditorIntent>& intents, const SkinArt& art) {
  ImGui::SeparatorText(core::Tr("Sons de la carte"));
  const MapEditorSound* selected = nullptr;
  bool any_map_sound = false;
  for (const MapEditorSound& sound : view.sounds) {
    if (view.selected_sound == sound.key) {
      selected = &sound;
    }
    if (!sound.key.added) {
      any_map_sound = true;
      SoundRow(view, sound, intents);
    }
  }
  if (!any_map_sound) {
    ImGui::TextDisabled("%s", core::Tr("Cette carte n'a aucun son."));
  }
  ImGui::SeparatorText(core::Tr("Sons ajoutés"));
  DrawSoundLibrary(view, panel, art);
  ImGui::BeginDisabled(panel.sounds.choice.empty());
  if (SkinButton(core::Tr("Écouter le fichier choisi"), art)) {
    intents.emplace_back(map_editor::ListenSound{.file = panel.sounds.choice});
  }
  ImGui::SameLine();
  if (SkinButton(core::Tr("Ajouter à la carte"), art)) {
    intents.emplace_back(map_editor::AddSound{panel.sounds.choice});
  }
  ImGui::EndDisabled();
  for (const MapEditorSound& sound : view.sounds) {
    if (sound.key.added) {
      SoundRow(view, sound, intents);
    }
  }
  if (selected != nullptr) {
    DrawSelectedSound(view, *selected, panel, intents, art);
  }
}

}  // namespace ro::ui
