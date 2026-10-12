// LA LISTE « @autolootid » ET SES PRESETS, dans l'onglet « Autoloots » de la
// page « Commands Settings ».
//
// ⚠ LES PRESETS VIVENT SUR LE SERVEUR : chaque geste ici est un ORDRE, et c'est
// la liste qu'il renvoie qui dit ce qui a été fait.

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/ui/skin_widgets.h"

#include "commands_page_parts.h"

namespace ro::ui::commands_page {
namespace {

// Les largeurs des champs et de la liste, en pixels d'interface.
constexpr float kIdInputWidth = 100.0F;
constexpr float kPresetFieldWidth = 120.0F;
constexpr float kListHeight = 160.0F;
// L'écart resserré de la liste : quatre pixels entre voisins, un entre lignes.
constexpr float kListSpacingX = 4.0F;
constexpr float kListSpacingY = 1.0F;

// Assez pour un nom de preset, sa marque d'actif et son terminateur.
constexpr std::size_t kPresetLabelBytes = 80;
// Assez pour « (non sauvegardé) » derrière un nom de preset, dans toute langue.
constexpr std::size_t kHeaderBytes = 160;

[[nodiscard]] const CommandsPresetView* FindPreset(std::span<const CommandsPresetView> presets,
                                                   std::uint8_t slot) {
  for (const CommandsPresetView& preset : presets) {
    if (preset.slot == slot) {
      return &preset;
    }
  }
  return nullptr;
}

// Le nom d'un preset, ou « #N » pour un preset sans nom — et l'étoile de celui
// qui est chargé.
void PresetLabel(const CommandsPresetView& preset, bool mark_active,
                 std::array<char, kPresetLabelBytes>& into) {
  const char* star = mark_active ? " *" : "";
  if (preset.name[0] == '\0') {
    std::snprintf(into.data(), into.size(), "#%u%s", static_cast<unsigned>(preset.slot), star);
  } else {
    std::snprintf(into.data(), into.size(), "%s%s", preset.name, star);
  }
}

[[nodiscard]] bool Listed(const CommandsPageView& view, std::uint32_t item_id) {
  for (const CommandsAutolootEntry& entry : view.autoloot) {
    if (entry.item_id == item_id) {
      return true;
    }
  }
  return false;
}

void DrawAddField(const CommandsPageView& view, CommandsPageState& state, const SkinArt& art,
                  CommandsPageAction& asked) {
  ImGui::SetNextItemWidth(Px(kIdInputWidth));
  ImGui::InputInt("##alootid_input", &state.autoloot_id_input, 0, 0);
  state.autoloot_id_input = std::max(state.autoloot_id_input, 0);
  ImGui::SameLine();
  const bool can_add =
      state.autoloot_id_input > 0 && view.autoloot.size() < kCommandsAutolootCapacity;
  ImGui::BeginDisabled(!can_add);
  if (SkinButton(core::Tr("Add##alootid"), art)) {
    const auto item_id = static_cast<std::uint32_t>(state.autoloot_id_input);
    if (!Listed(view, item_id)) {
      asked.autoloot_add = item_id;
    }
  }
  ImGui::EndDisabled();
}

// « Liste courante », ou le preset chargé — et s'il a changé depuis.
void DrawListHeader(const CommandsPageView& view) {
  std::array<char, kHeaderBytes> header{};
  if (view.active_preset == 0) {
    std::snprintf(header.data(), header.size(), "%s", core::Tr("Liste courante"));
  } else {
    std::array<char, kPresetLabelBytes> name{};
    if (const CommandsPresetView* active = FindPreset(view.presets, view.active_preset);
        active != nullptr) {
      PresetLabel(*active, false, name);
    } else {
      std::snprintf(name.data(), name.size(), "?");
    }
    if (view.autoloot_unsaved) {
      std::snprintf(header.data(), header.size(), core::Tr("%s (non sauvegardé)"), name.data());
    } else {
      std::snprintf(header.data(), header.size(), "%s", name.data());
    }
  }
  ImGui::TextUnformatted(header.data());
}

void DrawList(const CommandsPageView& view, const SkinArt& art, CommandsPageAction& asked) {
  ImGui::BeginChild("##alootid_list", ImVec2(0.0F, Px(kListHeight)), ImGuiChildFlags_Borders);
  if (ImGui::BeginTable("##alootid_tbl", 2, ImGuiTableFlags_SizingStretchSame)) {
    int rank = 0;
    for (const CommandsAutolootEntry& entry : view.autoloot) {
      ImGui::TableNextColumn();
      ImGui::PushID(rank++);
      // ⚠ LA CROIX N'EST PAS TRADUITE : elle se lit pareil dans toute langue.
      if (SkinSmallButton("x", art)) {
        asked.autoloot_remove = entry.item_id;
      }
      ImGui::PopID();
      ImGui::SameLine();
      if (entry.name != nullptr) {
        ImGui::Text("%s [%u]", entry.name, static_cast<unsigned>(entry.item_id));
      } else {
        ImGui::Text("[%u]", static_cast<unsigned>(entry.item_id));
      }
    }
    ImGui::EndTable();
  }
  ImGui::EndChild();
}

// L'ITEM DONT ON VIENT DE LIRE LA DESCRIPTION, à ajouter ou retirer d'un clic.
void DrawLastViewed(const CommandsPageView& view, const SkinArt& art, CommandsPageAction& asked) {
  if (view.last_viewed_item == 0) {
    return;
  }
  ImGui::Separator();
  const auto shown = static_cast<unsigned>(view.last_viewed_item);
  if (view.last_viewed_name != nullptr) {
    ImGui::Text(core::Tr("Vu: [%u] %s"), shown, view.last_viewed_name);
  } else {
    ImGui::Text(core::Tr("Vu: [%u]"), shown);
  }
  ImGui::SameLine();
  if (Listed(view, view.last_viewed_item)) {
    if (SkinSmallButton(core::Tr("Remove##alootid_vu"), art)) {
      asked.autoloot_remove = view.last_viewed_item;
    }
    return;
  }
  ImGui::BeginDisabled(view.autoloot.size() >= kCommandsAutolootCapacity);
  if (SkinSmallButton(core::Tr("Add##alootid_vu"), art)) {
    asked.autoloot_add = view.last_viewed_item;
  }
  ImGui::EndDisabled();
}

void AskPreset(CommandsPresetGesture gesture, std::uint8_t slot, const char* name,
               CommandsPageAction& asked) {
  asked.preset = gesture;
  asked.preset_slot = slot;
  asked.preset_name = name != nullptr ? name : "";
}

// « Presets : », la case du chargement automatique, et celui qui l'a.
void DrawAutoloadLine(const CommandsPageView& view, const CommandsPageState& state,
                      const SkinArt& art, CommandsPageAction& asked) {
  ImGui::TextUnformatted(Translated(kPresetsLabel));
  ImGui::SameLine();
  const CommandsPresetView* selected = FindPreset(view.presets, state.selected_preset);
  bool autoload = selected != nullptr && selected->autoload;
  ImGui::BeginDisabled(selected == nullptr);
  if (SkinCheckBox(core::Tr("Autoload##preset"), &autoload, art)) {
    AskPreset(CommandsPresetGesture::kAutoload, autoload ? state.selected_preset : 0, nullptr,
              asked);
  }
  ImGui::EndDisabled();
  ImGui::SameLine();
  const CommandsPresetView* loading = nullptr;
  for (const CommandsPresetView& preset : view.presets) {
    if (preset.autoload) {
      loading = &preset;
      break;
    }
  }
  std::array<char, kPresetLabelBytes> label{};
  if (loading == nullptr) {
    std::snprintf(label.data(), label.size(), "%s", core::Tr("(aucun)"));
  } else if (loading->name[0] == '\0') {
    std::snprintf(label.data(), label.size(), "(#%u)", static_cast<unsigned>(loading->slot));
  } else {
    std::snprintf(label.data(), label.size(), "(%s)", loading->name);
  }
  FaintText(label.data());
}

// Le nom, le renommage, et le bouton qui sauvegarde — ou supprime.
void DrawSaveLine(const CommandsPageView& view, CommandsPageState& state, const SkinArt& art,
                  CommandsPageAction& asked) {
  ImGui::SetNextItemWidth(Px(kPresetFieldWidth));
  ImGui::InputText("##preset_name", state.preset_name.data(), state.preset_name.size());
  ImGui::SameLine();
  static_cast<void>(SkinCheckBox(core::Tr("Renommer##preset_toggle"), &state.rename_open, art));
  if (state.rename_open) {
    ImGui::SameLine();
    ImGui::SetNextItemWidth(Px(kPresetFieldWidth));
    ImGui::InputText("##preset_rename", state.rename.data(), state.rename.size());
  }
  ImGui::SameLine();
  const CommandsPresetView* selected = FindPreset(view.presets, state.selected_preset);
  const CommandsPresetView* named = nullptr;
  for (const CommandsPresetView& preset : view.presets) {
    if (std::strcmp(preset.name, state.preset_name.data()) == 0) {
      named = &preset;
      break;
    }
  }
  const bool list_empty = view.autoloot.empty();
  // ⚠ UNE LISTE VIDE NE SE SAUVEGARDE PAS : le serveur n'écrirait aucune ligne.
  // Le même bouton SUPPRIME alors le preset qui porte ce nom.
  const bool deletes = !state.rename_open && list_empty && named != nullptr;
  // 🔴 ET UNE LISTE PLEINE NE SAUVEGARDE PLUS SOUS UN NOM NEUF : dix emplacements
  // pris, le clic ne mènerait nulle part.
  const std::uint8_t save_slot = PresetSlotForName(view.presets, state.preset_name.data());
  const bool can_act = state.rename_open ? state.rename[0] != '\0' && selected != nullptr
                                         : state.preset_name[0] != '\0' &&
                                               (deletes || (!list_empty && save_slot != 0));
  ImGui::BeginDisabled(!can_act);
  if (SkinSmallButton(
          deletes ? core::Tr("Supprimer##preset_save") : core::Tr("Sauvegarder##preset"), art)) {
    if (state.rename_open) {
      AskPreset(CommandsPresetGesture::kRename, state.selected_preset, state.rename.data(), asked);
      state.rename_open = false;
    } else if (deletes) {
      AskPreset(CommandsPresetGesture::kDelete, named->slot, nullptr, asked);
    } else {
      AskPreset(CommandsPresetGesture::kSave, save_slot, state.preset_name.data(), asked);
    }
  }
  ImGui::EndDisabled();
}

// La liste des presets, et ce qu'on fait de celui qu'on y choisit.
void DrawChoiceLine(const CommandsPageView& view, CommandsPageState& state, const SkinArt& art,
                    CommandsPageAction& asked) {
  const CommandsPresetView* selected = FindPreset(view.presets, state.selected_preset);
  std::array<char, kPresetLabelBytes> preview{};
  if (selected != nullptr) {
    PresetLabel(*selected, false, preview);
  } else {
    std::snprintf(preview.data(), preview.size(), "%s", core::Tr("-- choisir --"));
  }
  ImGui::SetNextItemWidth(Px(kPresetFieldWidth));
  int step = 0;
  const bool open = BeginSkinCombo("##preset_select", preview.data(), art, &step);
  // Choisir n'est pas charger : la molette ne fait que désigner, le bouton d'à
  // côté applique.
  const int rank = selected != nullptr ? static_cast<int>(selected - view.presets.data()) : -1;
  if (const int to = SkinComboStepped(rank, step, static_cast<int>(view.presets.size()));
      to != rank) {
    state.selected_preset = view.presets[static_cast<std::size_t>(to)].slot;
  }
  if (open) {
    for (const CommandsPresetView& preset : view.presets) {
      const bool chosen = preset.slot == state.selected_preset;
      std::array<char, kPresetLabelBytes> label{};
      PresetLabel(preset, preset.slot == view.active_preset, label);
      ImGui::PushID(preset.slot);
      if (ImGui::Selectable(label.data(), chosen)) {
        state.selected_preset = preset.slot;
      }
      ImGui::PopID();
      if (chosen) {
        ImGui::SetItemDefaultFocus();
      }
    }
    EndSkinCombo();
  }
  ImGui::SameLine();
  ImGui::BeginDisabled(selected == nullptr);
  if (SkinSmallButton(core::Tr("Charger##preset"), art)) {
    AskPreset(CommandsPresetGesture::kLoad, state.selected_preset, nullptr, asked);
  }
  ImGui::SameLine();
  if (SkinSmallButton(core::Tr("Supprimer##preset"), art)) {
    AskPreset(CommandsPresetGesture::kDelete, state.selected_preset, nullptr, asked);
  }
  ImGui::EndDisabled();
}

}  // namespace

void DrawAutolootIdNode(const CommandsPageView& view, CommandsPageState& state, const SkinArt& art,
                        CommandsPageAction& asked) {
  if (!ImGui::TreeNode(kAutolootIdCommand)) {
    return;
  }
  ImGui::TextUnformatted("@autolootid :");
  ImGui::SameLine();
  SkinHelpMarker(
      core::Tr("Loot automatiquement les items par ID.\nMax 50 IDs. (@autolootid <id>)"));
  ImGui::SameLine();
  if (SkinSmallButton(core::Tr("Clear##alootid"), art)) {
    asked.autoloot_clear = true;
  }
  DrawAddField(view, state, art, asked);
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(kListSpacingX, kListSpacingY));
  DrawListHeader(view);
  DrawList(view, art, asked);
  ImGui::PopStyleVar();
  DrawLastViewed(view, art, asked);
  ImGui::Separator();
  DrawAutoloadLine(view, state, art, asked);
  DrawSaveLine(view, state, art, asked);
  DrawChoiceLine(view, state, art, asked);
  ImGui::TreePop();
}

}  // namespace ro::ui::commands_page
