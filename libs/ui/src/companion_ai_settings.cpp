// LES SECTIONS « RÉGLAGES » ET « TACTIQUES » DE LA FENÊTRE « IA DU
// COMPAGNON ». Voir `ro/ui/companion_ai_window.h`.

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <utility>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/ui/list_keys.h"
#include "ro/ui/skin_tooltip.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/staff_skill_id.h"

#include "companion_ai_window_parts.h"
#include "ui_scale.h"

namespace ro::ui::companion_ai_parts {
namespace {

// Le pas des délais tapés, en secondes : un clic sur « - » ou « + », puis avec
// Ctrl.
constexpr float kDelayStepSeconds = 0.1F;
constexpr float kDelayFastStepSeconds = 1.0F;
constexpr float kMsPerSecond = 1000.0F;
constexpr int kReserveStep = 10;
constexpr int kReserveFastStep = 100;
// La largeur des listes d'une ligne de tactique, en pixels de police.
constexpr float kTacticComboPixels = 130.0F;
constexpr int kTacticTableColumns = 4;

constexpr std::array<const char*, kOffensiveSkillChoices> kOffensiveLabels = {
    core::TrText("Jamais"), core::TrText("En combat"), core::TrText("En poursuite et en combat"),
    core::TrText("Skills seuls")};
constexpr std::array<const char*, kOwnerSeatedChoices> kSeatedLabels = {
    core::TrText("Ne rien changer"), core::TrText("Se placer à son côté, passif")};
constexpr std::array<const char*, kTacticRankChoices> kRankLabels = {
    core::TrText("Priorité absolue"),
    core::TrText("Haut"),
    core::TrText("Moyen"),
    core::TrText("Bas"),
    core::TrText("En dernier"),
    core::TrText("Tenir"),
    core::TrText("Ignorer")};
constexpr std::array<const char*, kTacticTriggerChoices> kTriggerLabels = {
    core::TrText("Toujours"), core::TrText("S'il attaque"),
    core::TrText("S'il attaque le compagnon")};

// L'INFOBULLE D'UN RÉGLAGE QUI DÉPEND D'UN AUTRE : grisé quand l'autre le rend
// sans effet, il dit alors pourquoi. ⚠ LE SURVOL SE TESTE MÊME GRISÉ, sans
// quoi l'explication ne paraîtrait jamais.
void DependentTooltip(const char* tip, bool live, const char* why) {
  if (!ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    return;
  }
  if (live) {
    SkinTooltipFormat("%s", tip);
  } else {
    SkinTooltipFormat("%s\n\n%s", tip, why);
  }
}

// LE CURSEUR QUI VIENT D'ÊTRE DESSINÉ, survolé ou tenu : sa zone seule se
// montre au sol. ⚠ TENU AUSSI, parce qu'un glisser sort vite du cadre ; et
// grisé aussi, comme son infobulle.
void NoteZoneHover(CompanionAiZoneSlider zone, CompanionAiWindowAction& action) {
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled) || ImGui::IsItemActive()) {
    action.hovered_zone = zone;
  }
}

void DrawSkillPermissions(CompanionBaseFields& fields, const CompanionAiView& view,
                          const SkinArt& art, bool& changed) {
  if (view.offensive_skills.empty()) {
    ImGui::TextDisabled("%s", core::Tr("Aucun skill offensif appris."));
    return;
  }
  for (std::size_t index = 0;
       index < view.offensive_skills.size() && index < fields.permissions.size(); ++index) {
    const OffensiveSkillRow& skill = view.offensive_skills[index];
    SkillPermissionField& permission = fields.permissions[index];
    ImGui::PushID(skill.skill_id);
    changed |= SkinCheckBox(skill.label.c_str(), &permission.allowed, art);
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
      SkinTooltipWithStaffSkillId(core::Tr("Coché, ce skill peut partir de lui-même."),
                                  view.staff_allowed,
                                  StaffSkillId{.skill_id = skill.skill_id, .idname = skill.idname});
    }
    if (permission.allowed) {
      ImGui::SameLine();
      ImGui::SetNextItemWidth(Px(kTacticComboPixels));
      changed |= LevelCombo("##level", &permission.level, skill.learned_level, art);
      SkinTooltip(core::Tr("Le niveau auquel il le lance de lui-même."));
    }
    ImGui::PopID();
  }
}

}  // namespace

bool LevelCombo(const char* id, int* level, int learned, const SkinArt& art) {
  const std::string shown = *level == 0 ? core::Tr("Niveau appris") : std::to_string(*level);
  bool changed = false;
  if (BeginSkinCombo(id, shown.c_str(), art)) {
    // Le rang d'une entrée est son niveau : zéro, « appris », d'abord.
    ComboKeys keys(*level, learned + 1);
    if (keys.Took(0, ImGui::Selectable(core::Tr("Niveau appris"), *level == 0) && *level != 0)) {
      *level = 0;
      changed = true;
    }
    for (int each = 1; each <= learned; ++each) {
      ImGui::PushID(each);
      if (keys.Took(each, ImGui::Selectable(std::to_string(each).c_str(), *level == each) &&
                              *level != each)) {
        *level = each;
        changed = true;
      }
      ImGui::PopID();
    }
    EndSkinCombo();
  }
  return changed;
}

bool TypedSeconds(const char* label, int* delay_ms, int most_ms, const SkinArt& art) {
  float seconds = static_cast<float>(*delay_ms) / kMsPerSecond;
  if (!SkinInputFloat(label, &seconds, 0.0F, static_cast<float>(most_ms) / kMsPerSecond,
                      kDelayStepSeconds, kDelayFastStepSeconds, art, "%.1f")) {
    return false;
  }
  const int typed = std::clamp(static_cast<int>(std::lround(seconds * kMsPerSecond)), 0, most_ms);
  if (typed == *delay_ms) {
    return false;
  }
  *delay_ms = typed;
  return true;
}

std::span<const char* const> TacticRankLabels() { return kRankLabels; }

std::span<const char* const> TacticTriggerLabels() { return kTriggerLabels; }

void DrawGroundZonesToggle(const CompanionAiView& view, const SkinArt& art,
                           CompanionAiWindowAction& action) {
  bool shown = view.ground_zones_shown;
  action.ground_zones_toggled = SkinCheckBox(core::Tr("Montrer les zones au sol"), &shown, art);
  SkinTooltip(
      core::Tr("Fenêtre ouverte, les zones de suivi (bleu), d'éloignement (orange) et "
               "d'engagement (rouge) se dessinent au sol autour du maître, avec la case "
               "qu'il garde. Cochée, elles y restent fenêtre fermée."));
}

void DrawSettingsSection(const CompanionAiView& view, const SkinArt& art,
                         CompanionAiWindowAction& action) {
  CompanionBaseFields fields = view.base;
  bool changed = false;
  // ⚠ LES DISTANCES S'ARRÊTENT AU RAYON DE VUE DU SERVEUR, pas au plafond du
  // fichier : au-delà, l'IA les borne de toute façon.
  const int most_cells = CompanionAiCellsShown(view.view_cells, kCompanionAiCellsMost);
  const auto cells = [&changed, &art, &action, most_cells](const char* label, int* value,
                                                           const char* tip,
                                                           CompanionAiZoneSlider zone) {
    changed |= SkinSliderInt(label, value, 0, most_cells, art);
    NoteZoneHover(zone, action);
    SkinTooltip(tip);
  };
  const auto percent = [&changed, &art](const char* label, int* value, const char* tip) {
    changed |= SkinSliderInt(label, value, 0, kCompanionAiPercentMost, art, "%d %%");
    SkinTooltip(tip);
  };
  const auto check = [&changed, &art](const char* label, bool* value, const char* tip) {
    changed |= SkinCheckBox(label, value, art);
    SkinTooltip(tip);
  };
  const auto delay = [&changed, &art](const char* label, int* value, const char* tip) {
    changed |= TypedSeconds(label, value, kCompanionAiDelayMostMs, art);
    SkinTooltip(tip);
  };

  ImGui::PushID("follow");
  ImGui::SeparatorText(core::Tr("Suivi"));
  cells(core::Tr("Distance de suivi (cases)"), &fields.follow_cells,
        core::Tr("Au-delà de cette distance au maître, il le rejoint."),
        CompanionAiZoneSlider::kFollow);
  cells(core::Tr("Éloignement maximum, maître immobile"), &fields.leash_still_cells,
        core::Tr("Au-delà de ces cases, il revient au maître quoi qu'il fasse. 0 : aucun."),
        CompanionAiZoneSlider::kLeashStill);
  cells(core::Tr("Éloignement maximum, maître en marche"), &fields.leash_walking_cells,
        core::Tr("Au-delà de ces cases, il revient au maître quoi qu'il fasse. 0 : aucun."),
        CompanionAiZoneSlider::kLeashWalking);
  ImGui::PopID();

  // 🔴 LES RÉGLAGES QUI DÉPENDENT D'UN AUTRE SE GRISENT quand l'autre les rend
  // sans effet — et seulement alors : une ligne de tactique peut passer outre
  // le réglage général, et les rendre de nouveau utiles.
  //
  // SANS POURSUITE, il ne frappe qu'à portée : le rayon d'engagement ne sert
  // plus, sauf ligne qui poursuit « Toujours ».
  const bool chase_matters =
      fields.chase || std::ranges::any_of(view.tactics, [](const TacticRowView& row) {
        return row.advanced.chase == kTacticChaseAlways;
      });
  // SANS INITIATIVE, ni de lui-même ni par une ligne « Toujours », ses seuils
  // ne bornent rien.
  const bool initiative_matters =
      fields.attack_on_own || std::ranges::any_of(view.tactics, [](const TacticRowView& row) {
        return row.trigger == kTacticTriggerAlways;
      });
  const char* engage_tip = core::Tr(
      "Une cible qu'il choisit seul doit être à ces cases du maître. 0 : seule sa vue "
      "compte.");
  const char* without_chase = core::Tr(
      "Grisé : « Poursuivre » (plus bas) est décoché, et aucune ligne de tactique ne "
      "poursuit « Toujours ». Il ne frappe donc qu'à portée, et ce rayon ne sert pas.");
  const char* without_initiative = core::Tr(
      "Grisé : « Attaquer de lui-même » est décoché, et aucune ligne de tactique ne se "
      "déclenche « Toujours ». Il ne prend jamais l'initiative, et ce seuil ne sert pas.");

  ImGui::PushID("engage");
  ImGui::SeparatorText(core::Tr("Engagement"));
  ImGui::BeginDisabled(!chase_matters);
  changed |= SkinSliderInt(core::Tr("Rayon d'engagement, maître immobile"),
                           &fields.engage_still_cells, 0, most_cells, art);
  NoteZoneHover(CompanionAiZoneSlider::kEngageStill, action);
  DependentTooltip(engage_tip, chase_matters, without_chase);
  changed |= SkinSliderInt(core::Tr("Rayon d'engagement, maître en marche"),
                           &fields.engage_walking_cells, 0, most_cells, art);
  NoteZoneHover(CompanionAiZoneSlider::kEngageWalking, action);
  DependentTooltip(engage_tip, chase_matters, without_chase);
  ImGui::EndDisabled();
  check(core::Tr("Attaquer de lui-même"), &fields.attack_on_own,
        core::Tr("Il attaque le monstre visible le plus proche sans attendre."));
  ImGui::BeginDisabled(!initiative_matters);
  changed |=
      SkinSliderInt(core::Tr("PV minimum pour attaquer de lui-même"),
                    &fields.own_attack_min_hp_percent, 0, kCompanionAiPercentMost, art, "%d %%");
  DependentTooltip(core::Tr("Sous ces PV, il ne prend plus l'initiative."), initiative_matters,
                   without_initiative);
  changed |=
      SkinSliderInt(core::Tr("SP minimum pour attaquer de lui-même"),
                    &fields.own_attack_min_sp_percent, 0, kCompanionAiPercentMost, art, "%d %%");
  DependentTooltip(core::Tr("Sous ces SP, il ne prend plus l'initiative."), initiative_matters,
                   without_initiative);
  ImGui::EndDisabled();
  check(core::Tr("Défendre le maître"), &fields.defend_owner,
        core::Tr("Il attaque ce qui attaque le maître."));
  check(core::Tr("Aider le maître"), &fields.assist_owner,
        core::Tr("Il attaque ce que le maître attaque."));
  check(core::Tr("Défendre l'autre compagnon"), &fields.defend_partner,
        core::Tr("Il attaque ce qui attaque l'autre compagnon du maître."));
  check(core::Tr("Se défendre"), &fields.defend_self, core::Tr("Il riposte à ce qui l'attaque."));
  check(core::Tr("Ne pas prendre le monstre d'un autre joueur"), &fields.spare_others_monsters,
        core::Tr("Il laisse les monstres qu'un autre joueur combat. Un ordre direct passe outre."));
  ImGui::PopID();

  ImGui::PushID("chase");
  ImGui::SeparatorText(core::Tr("Poursuite"));
  check(core::Tr("Poursuivre"), &fields.chase,
        core::Tr("Il poursuit une cible hors de portée ; sinon il ne frappe qu'à portée, et "
                 "le rayon d'engagement ne sert plus."));
  check(core::Tr("Changer de cible en route"), &fields.retarget_while_chasing,
        core::Tr("En pleine poursuite, il passe à une cible plus prioritaire."));
  delay(core::Tr("Lâcher une cible intacte après (s)"), &fields.drop_untouched_after_ms,
        core::Tr("Il lâche une cible qu'il n'a pas touchée depuis ce délai. 0 : jamais."));
  delay(core::Tr("Attendre après une arrivée (s)"), &fields.arrival_attack_hold_ms,
        core::Tr("Après une apparition ou un changement de carte, ni attaque ni skill avant ce "
                 "délai. Le serveur en impose un lui-même."));
  ImGui::PopID();

  ImGui::PushID("skills");
  ImGui::SeparatorText(core::Tr("Skills"));
  changed |= SkinCombo(core::Tr("Skills offensifs automatiques"), &fields.offensive_skills,
                       kOffensiveLabels, art);
  SkinTooltip(
      core::Tr("Quand il lance de lui-même ses skills offensifs sur sa cible. « Skills "
               "seuls » supprime l'attaque simple : il ne frappe plus que par ses skills, "
               "et attend à portée quand aucun ne peut partir."));
  changed |= SkinInputInt(core::Tr("SP à garder"), &fields.skill_sp_reserve, 0,
                          kCompanionAiSpReserveMost, kReserveStep, kReserveFastStep, art);
  SkinTooltip(core::Tr("Un skill ne part pas s'il laisserait moins que ces SP."));
  delay(core::Tr("Délai entre deux skills (s)"), &fields.skill_delay_ms,
        core::Tr("Le temps minimum entre deux skills lancés de lui-même."));
  DrawSkillPermissions(fields, view, art, changed);
  ImGui::PopID();

  ImGui::PushID("standby");
  ImGui::SeparatorText(core::Tr("Attente"));
  check(core::Tr("En attente : défendre quand même"), &fields.standby_defends,
        core::Tr("En attente, il défend encore le maître et lui-même."));
  check(core::Tr("Revenir en attente après un combat"), &fields.standby_after_fight,
        core::Tr("Quand un combat s'achève, il se remet en attente."));
  check(core::Tr("Garder l'attente au changement de carte"), &fields.standby_kept_across_maps,
        core::Tr("L'attente survit à un portail et à un changement de carte."));
  ImGui::PopID();

  ImGui::PushID("move");
  ImGui::SeparatorText(core::Tr("Déplacement et repos"));
  check(core::Tr("Après « aller ici » : rester sur la case"), &fields.move_order_holds_cell,
        core::Tr("Il ne quitte plus la case où on l'a envoyé."));
  ImGui::BeginDisabled(!fields.move_order_holds_cell);
  check(core::Tr("Et s'y battre quand même"), &fields.move_order_hold_fights,
        core::Tr("Sur cette case, il frappe ce qui est à portée sans la quitter."));
  ImGui::EndDisabled();
  changed |=
      SkinCombo(core::Tr("Quand le maître s'assoit"), &fields.owner_seated, kSeatedLabels, art);
  SkinTooltip(core::Tr("Le maître assis peut garder son compagnon à son côté, sans attaque."));
  ImGui::PopID();

  ImGui::PushID("retreat");
  ImGui::SeparatorText(core::Tr("Repli"));
  percent(core::Tr("Revenir au maître sous ces PV"), &fields.return_below_hp_percent,
          core::Tr("Sous ces PV, il revient au maître. 0 : jamais."));
  percent(core::Tr("Fuir sous ces PV"), &fields.flee_below_hp_percent,
          core::Tr("Sous ces PV, il fuit ce qui l'attaque. 0 : jamais."));
  ImGui::PopID();

  if (changed) {
    action.verb = CompanionAiVerb::kSetBase;
    action.base = std::move(fields);
  }
}

void DrawTacticsSection(CompanionAiWindowState& state, const CompanionAiView& view,
                        const SkinArt& art, CompanionAiWindowAction& action) {
  companion_ai_parts::WrappedNote(
      core::Tr("Une ligne de monstre s'ajoute depuis le menu d'entité d'un "
               "monstre (clic droit, « Tactique du compagnon »)."));
  if (view.tactics.empty()) {
    ImGui::TextDisabled("%s", core::Tr("Aucune tactique : le comportement de base décide."));
  } else if (ImGui::BeginTable("tactics", kTacticTableColumns,
                               ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit)) {
    ImGui::TableSetupColumn(core::Tr("Monstre"), ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn(core::Tr("Rang"));
    ImGui::TableSetupColumn(core::Tr("Déclencheur"));
    ImGui::TableSetupColumn("##actions");
    ImGui::TableHeadersRow();
    for (std::size_t index = 0; index < view.tactics.size(); ++index) {
      const TacticRowView& row = view.tactics[index];
      const bool expanded = state.expanded_tactic == static_cast<int>(index);
      ImGui::PushID(static_cast<int>(index));
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      if (ImGui::Selectable(row.label.c_str(), expanded)) {
        state.expanded_tactic = expanded ? -1 : static_cast<int>(index);
      }
      SkinTooltip(core::Tr("Clic : les réglages avancés de cette ligne."));
      int rank = row.rank;
      int trigger = row.trigger;
      ImGui::TableNextColumn();
      ImGui::SetNextItemWidth(Px(kTacticComboPixels));
      const bool rank_changed = SkinCombo("##rank", &rank, kRankLabels, art);
      ImGui::TableNextColumn();
      ImGui::SetNextItemWidth(Px(kTacticComboPixels));
      const bool trigger_changed = SkinCombo("##trigger", &trigger, kTriggerLabels, art);
      if (rank_changed || trigger_changed) {
        action.verb = CompanionAiVerb::kSetTactic;
        action.index = index;
        action.rank = rank;
        action.trigger = trigger;
        action.advanced = row.advanced;
      }
      ImGui::TableNextColumn();
      if (SkinSmallButton(core::Tr("Retirer"), art)) {
        action.verb = CompanionAiVerb::kRemoveTactic;
        action.index = index;
      }
      if (expanded) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        TacticAdvancedFields advanced = row.advanced;
        if (DrawTacticAdvancedFields(advanced, view.openings, art, view.view_cells)) {
          action.verb = CompanionAiVerb::kSetTactic;
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

  ImGui::SeparatorText(core::Tr("Lignes génériques"));
  for (int generic = 0; generic < kTacticGenericChoices; ++generic) {
    if (view.generic_present[static_cast<std::size_t>(generic)]) {
      continue;
    }
    ImGui::PushID(generic);
    ImGui::BeginDisabled(view.tactics_full);
    if (SkinSmallButton(CompanionTacticGenericLabel(generic), art)) {
      action.verb = CompanionAiVerb::kAddGenericTactic;
      action.generic = generic;
    }
    ImGui::EndDisabled();
    SkinTooltip(view.tactics_full ? core::Tr("La table des tactiques est pleine.")
                                  : core::Tr("Ajoute cette ligne au rang « moyen »."));
    ImGui::PopID();
    ImGui::SameLine();
  }
  ImGui::NewLine();
}

}  // namespace ro::ui::companion_ai_parts
