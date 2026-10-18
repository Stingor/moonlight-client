#include "ro/ui/companion_tactic_window.h"

#include <algorithm>
#include <array>
#include <string>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/ui/bug_report_window.h"
#include "ro/ui/list_keys.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"

#include "ui_scale.h"

namespace ro::ui {
namespace {

// 🔴 LA LARGEUR DES CURSEURS ET DES LISTES, en pixels de police : fixe, comme
// dans la fenêtre « IA du compagnon ». La fenêtre suit son contenu, et sa
// largeur est donc celle de sa ligne la plus large.
constexpr float kControlWidthPixels = 190.0F;

// Le pas de la réserve de SP : un clic sur « - » ou « + », puis avec Ctrl.
constexpr int kReserveStep = 1;
constexpr int kReserveFastStep = 100;

constexpr std::array<const char*, kTacticAvoidChoices> kAvoidLabels = {
    core::TrText("Jamais"), core::TrText("S'il attaque"), core::TrText("Toujours")};
constexpr std::array<const char*, kTacticSkillUseChoices> kSkillUseLabels = {
    core::TrText("Jamais"), core::TrText("N fois"), core::TrText("Une fois au niveau L"),
    core::TrText("Toujours")};
constexpr std::array<const char*, kTacticShareChoices> kShareLabels = {
    core::TrText("Jamais"), core::TrText("S'il ne bouge pas"), core::TrText("Toujours")};
constexpr std::array<const char*, kTacticChaseChoices> kChaseLabels = {
    core::TrText("Comme le réglage général"), core::TrText("Toujours"), core::TrText("Jamais")};

// Le libellé de l'ouverture choisie, ou « Aucun ».
[[nodiscard]] const char* OpeningLabel(std::uint16_t skill_id,
                                       std::span<const TacticSkillChoice> openings) {
  for (const TacticSkillChoice& choice : openings) {
    if (choice.skill_id == skill_id) {
      return choice.label.c_str();
    }
  }
  return core::Tr("Aucun");
}

}  // namespace

bool DrawTacticAdvancedFields(TacticAdvancedFields& fields,
                              std::span<const TacticSkillChoice> openings, const SkinArt& art,
                              int view_cells, bool for_player) {
  bool changed = false;
  ImGui::PushID("tactic_fields");

  if (!for_player) {
    ImGui::SeparatorText(core::Tr("Éviter"));
    changed |= SkinCombo(core::Tr("Quand"), &fields.avoid, kAvoidLabels, art);
  }
  if (!for_player && fields.avoid != 0) {
    const int most_cells = CompanionAiCellsShown(view_cells, kTacticAvoidMostCells);
    changed |= SkinSliderInt(core::Tr("À moins de (cases)"), &fields.avoid_threshold_cells, 1,
                             most_cells, art);
    changed |= SkinSliderInt(core::Tr("S'écarter de (cases)"), &fields.avoid_step_cells, 1,
                             most_cells, art);
  }

  ImGui::SeparatorText(core::Tr("Skills"));
  if (SkinCombo(core::Tr("Usage"), &fields.skill_use, kSkillUseLabels, art)) {
    changed = true;
    // « N fois » et « une fois au niveau L » veulent au moins 1 : la ligne qui
    // n'en porte pas encore le reçoit.
    if ((fields.skill_use == kTacticSkillUseTimes ||
         fields.skill_use == kTacticSkillUseOnceAtLevel) &&
        fields.skill_use_count < 1) {
      fields.skill_use_count = 1;
    }
  }
  if (fields.skill_use == kTacticSkillUseTimes) {
    changed |= SkinSliderInt(core::Tr("Nombre de fois"), &fields.skill_use_count, 1,
                             kTacticSkillUseMost, art);
  } else if (fields.skill_use == kTacticSkillUseOnceAtLevel) {
    changed |=
        SkinSliderInt(core::Tr("Niveau"), &fields.skill_use_count, 1, kTacticSkillUseMost, art);
  }
  if (BeginSkinCombo(core::Tr("Skill d'ouverture"), OpeningLabel(fields.opening_skill_id, openings),
                     art)) {
    // Le rang zéro est « Aucun », les skills suivent.
    const auto at =
        std::ranges::find(openings, fields.opening_skill_id, &TacticSkillChoice::skill_id);
    int chosen = 0;
    if (fields.opening_skill_id != 0) {
      chosen = at == openings.end() ? kListNoEntry : static_cast<int>(at - openings.begin()) + 1;
    }
    ComboKeys keys(chosen, static_cast<int>(openings.size()) + 1);
    if (keys.Took(0, ImGui::Selectable(core::Tr("Aucun"), fields.opening_skill_id == 0) &&
                         fields.opening_skill_id != 0)) {
      fields.opening_skill_id = 0;
      changed = true;
    }
    for (std::size_t index = 0; index < openings.size(); ++index) {
      const TacticSkillChoice& choice = openings[index];
      ImGui::PushID(choice.skill_id);
      if (keys.Took(
              static_cast<int>(index) + 1,
              ImGui::Selectable(choice.label.c_str(), fields.opening_skill_id == choice.skill_id) &&
                  fields.opening_skill_id != choice.skill_id)) {
        fields.opening_skill_id = choice.skill_id;
        changed = true;
      }
      ImGui::PopID();
    }
    EndSkinCombo();
  }
  if (!for_player) {
    changed |=
        SkinCheckBox(core::Tr("Peut être harcelé pendant un autre combat"), &fields.harass, art);
  }
  bool own_reserve = fields.sp_reserve_points >= 0;
  if (SkinCheckBox(for_player ? core::Tr("SP à garder propre à ce joueur")
                              : core::Tr("SP à garder propre à ce monstre"),
                   &own_reserve, art)) {
    fields.sp_reserve_points = own_reserve ? 0 : -1;
    changed = true;
  }
  if (own_reserve) {
    changed |= SkinInputInt(core::Tr("SP à garder"), &fields.sp_reserve_points, 0,
                            kTacticSpReserveMost, kReserveStep, kReserveFastStep, art);
  }

  if (for_player) {
    ImGui::SeparatorText(core::Tr("Poursuite"));
    changed |= SkinCombo(core::Tr("Poursuite"), &fields.chase, kChaseLabels, art);
  } else {
    ImGui::SeparatorText(core::Tr("Autres joueurs et poursuite"));
    changed |= SkinCombo(core::Tr("Partage"), &fields.share, kShareLabels, art);
    changed |= SkinCombo(core::Tr("Poursuite"), &fields.chase, kChaseLabels, art);
    changed |= SkinSliderFloat(core::Tr("Poids dans les comptages"), &fields.weight, 0.0F,
                               kTacticWeightMost, art, "%.1f");
  }

  ImGui::PopID();
  return changed;
}

CompanionTacticWindowAction DrawCompanionTacticWindow(CompanionTacticWindowState& state,
                                                      const CompanionTacticView& view,
                                                      const SkinArt& art) {
  CompanionTacticWindowAction action;
  if (!state.open) {
    state.was_open = false;
    return action;
  }
  if (!state.was_open) {
    state.was_open = true;
    ImGui::SetNextWindowPos(ImGui::GetIO().MousePos, ImGuiCond_Always);
    ImGui::SetNextWindowFocus();
  }
  // Le titre change avec le monstre et la langue, l'identité jamais.
  const std::string name = view.title + kCompanionTacticWindowId;
  bool open = true;
  PoseBugReportButton(state.bug_report);
  SetNextWindowPinnable(&state.pinned);
  const bool begun =
      BeginSkinnedWindow(name.c_str(), &open, art, SkinnedTitle::kBar,
                         ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse);
  action.report_bug = state.bug_report && TitleActionClicked(0);
  if (!open) {
    state.open = false;
    action.closed = true;
  }
  if (begun) {
    ImGui::PushItemWidth(Px(kControlWidthPixels));
    ImGui::TextUnformatted(view.companion.c_str());
    TacticAdvancedFields fields = view.fields;
    if (DrawTacticAdvancedFields(fields, view.openings, art, view.view_cells)) {
      action.changed = fields;
    }
    ImGui::PopItemWidth();
  }
  EndSkinnedWindow();
  return action;
}

}  // namespace ro::ui
