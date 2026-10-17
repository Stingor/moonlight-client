#include "ro/ui/stat_preset_window.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <string>

#include <imgui.h>

#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"
#include "ro/ui/window_name.h"

namespace ro::ui {
namespace {

constexpr float kButtonGapPixels = 4.0F;
constexpr float kNameInputPixels = 160.0F;
// La couleur d'une note de l'aperçu : ce qui ne sera pas atteint.
constexpr ImVec4 kShortInk = ImVec4(0.80F, 0.30F, 0.20F, 1.0F);
constexpr ImVec4 kAboveInk = ImVec4(0.55F, 0.55F, 0.55F, 1.0F);
// Les colonnes de l'aperçu : la caractéristique, actuel → après, la cible, la
// note.
constexpr int kPreviewColumns = 4;
// Les colonnes de la liste : le nom, les six valeurs, les gestes.
constexpr int kListColumns = 3;

[[nodiscard]] float Scale() { return ImGui::GetStyle().FontScaleMain; }

// ⚠ UN AIGUILLAGE DE LITTÉRAUX : `core::Tr` veut voir sa chaîne à l'appel.
[[nodiscard]] const char* StatLabel(std::size_t rank) {
  switch (rank) {
    case 0:
      return core::Tr("STR");
    case 1:
      return core::Tr("AGI");
    case 2:
      return core::Tr("VIT");
    case 3:
      return core::Tr("INT");
    case 4:
      return core::Tr("DEX");
    default:
      return core::Tr("LUK");
  }
}

// « 90 / 99 / 1 / 1 / 40 / 1 », dans l'ordre de la fiche.
[[nodiscard]] std::string ValuesText(const StatPresetValues& values) {
  std::string text;
  for (std::size_t rank = 0; rank < values.size(); ++rank) {
    std::array<char, 32> piece{};
    std::snprintf(piece.data(), piece.size(), "%s%s %lld", rank == 0 ? "" : "  ", StatLabel(rank),
                  static_cast<long long>(values[rank]));
    text += piece.data();
  }
  return text;
}

void Gap() { ImGui::SameLine(0.0F, kButtonGapPixels * Scale()); }

void DrawPreviewNote(const StatPresetPreviewRow& row) {
  switch (row.verdict) {
    case StatPresetRowVerdict::kReached:
      ImGui::TextUnformatted(core::Tr("atteinte"));
      return;
    case StatPresetRowVerdict::kAboveTarget:
      ImGui::TextColored(kAboveInk, "%s", core::Tr("au-dessus de la cible, inchangée"));
      return;
    case StatPresetRowVerdict::kShort:
      ImGui::TextColored(kShortInk, core::Tr("il manquera %lld (%lld points)"),
                         static_cast<long long>(row.missing_steps),
                         static_cast<long long>(row.missing_points));
      return;
    case StatPresetRowVerdict::kCeiling:
      ImGui::TextColored(kShortInk, core::Tr("plafond du serveur : il manquera %lld"),
                         static_cast<long long>(row.missing_steps));
      return;
  }
}

void DrawPreview(const StatPresetWindowView& view, const SkinArt& art, StatPresetAction& action) {
  const StatPresetPreviewView& preview = view.preview;
  ImGui::Text(core::Tr("Aperçu : %.*s"), static_cast<int>(preview.name.size()),
              preview.name.data());
  if (ImGui::BeginTable("##apercu", kPreviewColumns,
                        ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg)) {
    for (std::size_t rank = 0; rank < preview.rows.size(); ++rank) {
      const StatPresetPreviewRow& row = preview.rows[rank];
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::TextUnformatted(StatLabel(rank));
      ImGui::TableNextColumn();
      ImGui::Text("%lld -> %lld", static_cast<long long>(row.current),
                  static_cast<long long>(row.after));
      ImGui::TableNextColumn();
      ImGui::Text(core::Tr("cible %lld"), static_cast<long long>(row.target));
      ImGui::TableNextColumn();
      DrawPreviewNote(row);
    }
    ImGui::EndTable();
  }
  ImGui::Text(core::Tr("Points de statut : %lld -> %lld (%lld montées)"),
              static_cast<long long>(preview.points_before),
              static_cast<long long>(preview.points_after), static_cast<long long>(preview.raises));
  ImGui::BeginDisabled(view.running || preview.raises <= 0);
  if (SkinButton(core::Tr("Confirmer"), art)) {
    action.verb = StatPresetVerb::kConfirm;
  }
  ImGui::EndDisabled();
  Gap();
  if (SkinButton(core::Tr("Annuler"), art)) {
    action.verb = StatPresetVerb::kCancelPreview;
  }
}

void DrawSaveRow(StatPresetWindowState& state, const StatPresetWindowView& view, const SkinArt& art,
                 StatPresetAction& action) {
  if (view.stats_known) {
    ImGui::TextDisabled(core::Tr("Actuelles : %s"), ValuesText(view.current).c_str());
  } else {
    ImGui::TextDisabled("%s", core::Tr("Les valeurs ne sont pas encore connues."));
  }
  ImGui::SetNextItemWidth(kNameInputPixels * Scale());
  ImGui::InputTextWithHint("##nom_prereglage", core::Tr("nom du préréglage"), state.name.data(),
                           state.name.size());
  Gap();
  ImGui::BeginDisabled(!view.stats_known || state.name[0] == '\0');
  if (SkinButton(core::Tr("Enregistrer"), art)) {
    action.verb = StatPresetVerb::kSave;
    action.name = state.name.data();
    state.name.fill('\0');
  }
  ImGui::EndDisabled();
  ImGui::TextDisabled(core::Tr("%zu / %zu"), view.presets.size(), view.capacity);
}

void DrawRowGestures(StatPresetWindowState& state, const StatPresetWindowView& view,
                     std::size_t index, const SkinArt& art, StatPresetAction& action) {
  const int rank = static_cast<int>(index);
  if (state.renaming == rank) {
    ImGui::SetNextItemWidth(kNameInputPixels * Scale());
    ImGui::InputText("##renommer", state.rename.data(), state.rename.size());
    Gap();
    if (SkinButton(core::Tr("OK"), art)) {
      action.verb = StatPresetVerb::kRename;
      action.index = index;
      action.name = state.rename.data();
      state.renaming = -1;
    }
    Gap();
    if (SkinButton(core::Tr("Annuler"), art)) {
      state.renaming = -1;
    }
    return;
  }
  if (state.removing == rank) {
    ImGui::TextUnformatted(core::Tr("Supprimer ?"));
    Gap();
    if (SkinButton(core::Tr("Oui"), art)) {
      action.verb = StatPresetVerb::kRemove;
      action.index = index;
      state.removing = -1;
    }
    Gap();
    if (SkinButton(core::Tr("Non"), art)) {
      state.removing = -1;
    }
    return;
  }
  ImGui::BeginDisabled(view.running || !view.stats_known);
  if (SkinButton(core::Tr("Appliquer"), art)) {
    action.verb = StatPresetVerb::kPreview;
    action.index = index;
  }
  ImGui::EndDisabled();
  Gap();
  if (SkinButton(core::Tr("Renommer"), art)) {
    state.renaming = rank;
    state.removing = -1;
    const std::string_view name = view.presets[index].name;
    state.rename.fill('\0');
    std::memcpy(state.rename.data(), name.data(), std::min(name.size(), state.rename.size() - 1));
  }
  Gap();
  if (SkinButton(core::Tr("Supprimer"), art)) {
    state.removing = rank;
    state.renaming = -1;
  }
}

void DrawList(StatPresetWindowState& state, const StatPresetWindowView& view, const SkinArt& art,
              StatPresetAction& action) {
  if (view.presets.empty()) {
    ImGui::TextDisabled("%s", core::Tr("Aucun préréglage enregistré."));
    return;
  }
  if (!ImGui::BeginTable("##prereglages", kListColumns,
                         ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg)) {
    return;
  }
  for (std::size_t index = 0; index < view.presets.size(); ++index) {
    const StatPresetRowView& row = view.presets[index];
    ImGui::PushID(static_cast<int>(index));
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(row.name.data(), row.name.data() + row.name.size());
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(ValuesText(row.stats).c_str());
    ImGui::TableNextColumn();
    DrawRowGestures(state, view, index, art, action);
    ImGui::PopID();
  }
  ImGui::EndTable();
}

}  // namespace

void PoseStatPresetButton() {
  SetNextWindowTitleAction(core::Tr("Préréglages"),
                           core::Tr("Enregistrer et appliquer des préréglages de stats"), true);
}

StatPresetAction DrawStatPresetWindow(StatPresetWindowState& state,
                                      const StatPresetWindowView& view, const SkinArt& art) {
  StatPresetAction action;
  if (!state.open) {
    return action;
  }
  // Une ligne retirée entre deux images ne garde ni sa saisie ni sa question.
  if (state.renaming >= static_cast<int>(view.presets.size())) {
    state.renaming = -1;
  }
  if (state.removing >= static_cast<int>(view.presets.size())) {
    state.removing = -1;
  }
  static WindowName name(kStatPresetWindowTitle, kStatPresetWindowId);
  if (BeginSkinnedWindow(name.Get(), &state.open, art, SkinnedTitle::kBar,
                         ImGuiWindowFlags_AlwaysAutoResize)) {
    if (view.preview.shown) {
      DrawPreview(view, art, action);
      ImGui::Separator();
    }
    if (view.running) {
      ImGui::Text(core::Tr("Application en cours : %zu / %zu"), view.done, view.total);
      Gap();
      if (SkinButton(core::Tr("Arrêter"), art)) {
        action.verb = StatPresetVerb::kStop;
      }
    }
    if (!view.report.empty()) {
      ImGui::TextWrapped("%.*s", static_cast<int>(view.report.size()), view.report.data());
    }
    if (view.running || !view.report.empty()) {
      ImGui::Separator();
    }
    DrawSaveRow(state, view, art, action);
    ImGui::Separator();
    DrawList(state, view, art, action);
  }
  EndSkinnedWindow();
  return action;
}

}  // namespace ro::ui
