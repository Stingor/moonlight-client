// L'ONGLET « JOUEURS » DE LA FENÊTRE « IA DU COMPAGNON » : la table des
// conduites envers les joueurs. Voir `ro/ui/companion_ai_window.h`.

#include <algorithm>
#include <array>
#include <cstddef>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/ui/list_keys.h"
#include "ro/ui/skin_tooltip.h"
#include "ro/ui/skin_widgets.h"

#include "companion_ai_window_parts.h"
#include "ui_scale.h"

namespace ro::ui::companion_ai_parts {
namespace {

// La largeur des listes d'une ligne, en pixels de police.
constexpr float kConductComboPixels = 130.0F;
// La liste des classes, dont les noms sont plus longs qu'un rang.
constexpr float kJobComboPixels = 2.0F * kConductComboPixels;
constexpr int kConductTableColumns = 4;

[[nodiscard]] const char* ConductGenericLabel(int generic) {
  return generic == kConductGenericOwnerGuild ? core::Tr("Membres de la guilde du maître")
                                              : core::Tr("Tout autre joueur");
}

void DrawRows(CompanionAiWindowState& state, const CompanionAiView& view, const SkinArt& art,
              CompanionAiWindowAction& action) {
  if (view.conducts.empty()) {
    ImGui::TextDisabled("%s", core::Tr("Aucune conduite : seule la défense vise un joueur."));
    return;
  }
  if (!ImGui::BeginTable("conducts", kConductTableColumns,
                         ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit)) {
    return;
  }
  ImGui::TableSetupColumn(core::Tr("Joueur"), ImGuiTableColumnFlags_WidthStretch);
  ImGui::TableSetupColumn(core::Tr("Rang"));
  ImGui::TableSetupColumn(core::Tr("Déclencheur"));
  ImGui::TableSetupColumn("##actions");
  ImGui::TableHeadersRow();
  for (std::size_t index = 0; index < view.conducts.size(); ++index) {
    const ConductRowView& row = view.conducts[index];
    const bool expanded = state.expanded_conduct == static_cast<int>(index);
    ImGui::PushID(static_cast<int>(index));
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    if (ImGui::Selectable(row.label.c_str(), expanded)) {
      state.expanded_conduct = expanded ? -1 : static_cast<int>(index);
    }
    SkinTooltip(core::Tr("Clic : les réglages avancés de cette ligne."));
    int rank = row.rank;
    int trigger = row.trigger;
    ImGui::TableNextColumn();
    ImGui::SetNextItemWidth(Px(kConductComboPixels));
    const bool rank_changed = SkinCombo("##rank", &rank, TacticRankLabels(), art);
    ImGui::TableNextColumn();
    ImGui::SetNextItemWidth(Px(kConductComboPixels));
    const bool trigger_changed = SkinCombo("##trigger", &trigger, TacticTriggerLabels(), art);
    if (rank_changed || trigger_changed) {
      action.verb = CompanionAiVerb::kSetConduct;
      action.index = index;
      action.rank = rank;
      action.trigger = trigger;
      action.advanced = row.advanced;
    }
    ImGui::TableNextColumn();
    if (SkinSmallButton(core::Tr("Retirer"), art)) {
      action.verb = CompanionAiVerb::kRemoveConduct;
      action.index = index;
    }
    if (expanded) {
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      TacticAdvancedFields advanced = row.advanced;
      if (DrawTacticAdvancedFields(advanced, view.openings, art, view.view_cells,
                                   /*for_player=*/true)) {
        action.verb = CompanionAiVerb::kSetConduct;
        action.index = index;
        action.rank = row.rank;
        action.trigger = row.trigger;
        action.advanced = advanced;
      }
    }
    ImGui::PopID();
  }
  ImGui::EndTable();
}

void DrawAdditions(CompanionAiWindowState& state, const CompanionAiView& view, const SkinArt& art,
                   CompanionAiWindowAction& action) {
  ImGui::SeparatorText(core::Tr("Lignes génériques et classes"));
  const char* full = view.conducts_full ? core::Tr("La table des conduites est pleine.") : nullptr;
  for (int generic = 0; generic < kConductGenericChoices; ++generic) {
    if (view.conduct_generic_present[static_cast<std::size_t>(generic)]) {
      continue;
    }
    ImGui::PushID(generic);
    ImGui::BeginDisabled(full != nullptr);
    if (SkinSmallButton(ConductGenericLabel(generic), art)) {
      action.verb = CompanionAiVerb::kAddGenericConduct;
      action.generic = generic;
    }
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
      SkinTooltipFormat("%s",
                        full != nullptr ? full : core::Tr("Ajoute cette ligne au rang « moyen »."));
    }
    ImGui::PopID();
    ImGui::SameLine();
  }
  ImGui::NewLine();
  if (view.player_jobs.empty()) {
    return;
  }
  const int last = static_cast<int>(view.player_jobs.size()) - 1;
  state.conduct_job_choice = std::clamp(state.conduct_job_choice, 0, last);
  ImGui::SetNextItemWidth(Px(kJobComboPixels));
  const RuleChoice& shown = view.player_jobs[static_cast<std::size_t>(state.conduct_job_choice)];
  if (BeginSkinCombo("##job", shown.label.c_str(), art)) {
    ComboKeys keys(state.conduct_job_choice, static_cast<int>(view.player_jobs.size()));
    for (std::size_t index = 0; index < view.player_jobs.size(); ++index) {
      ImGui::PushID(static_cast<int>(index));
      if (keys.Took(static_cast<int>(index),
                    ImGui::Selectable(view.player_jobs[index].label.c_str(),
                                      state.conduct_job_choice == static_cast<int>(index)))) {
        state.conduct_job_choice = static_cast<int>(index);
      }
      ImGui::PopID();
    }
    EndSkinCombo();
  }
  ImGui::SameLine();
  ImGui::BeginDisabled(full != nullptr);
  if (SkinSmallButton(core::Tr("Ajouter la classe"), art)) {
    action.verb = CompanionAiVerb::kAddJobConduct;
    action.choice = shown.value;
  }
  ImGui::EndDisabled();
}

}  // namespace

void DrawPlayersSection(CompanionAiWindowState& state, const CompanionAiView& view,
                        const SkinArt& art, CompanionAiWindowAction& action) {
  companion_ai_parts::WrappedNote(
      core::Tr("Ces conduites ne servent qu'en PvP et en WoE. Un joueur "
               "nommé s'ajoute depuis son menu d'entité (clic droit, "
               "« Conduite du compagnon »)."));
  ImGui::TextDisabled("%s", core::Tr("Jamais visés d'eux-mêmes : le maître, ses amis, son "
                                     "groupe, et sa guilde là où la carte l'épargne (WoE, "
                                     "donjon GvG)."));
  DrawRows(state, view, art, action);
  DrawAdditions(state, view, art, action);
}

}  // namespace ro::ui::companion_ai_parts
