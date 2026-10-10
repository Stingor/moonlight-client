// LA SECTION « RÈGLES » DE LA FENÊTRE « IA DU COMPAGNON » : une ligne par
// règle, lisible comme une phrase, qui se déplie pour l'édition et se déplace
// en glissant. Voir `ro/ui/companion_ai_window.h`.

#include <algorithm>
#include <cstring>
#include <string>
#include <utility>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/ui/skin_tooltip.h"
#include "ro/ui/skin_widgets.h"

#include "companion_ai_window_parts.h"

namespace ro::ui::companion_ai_parts {

[[nodiscard]] int* FieldOf(RuleConditionFields& condition, RuleNumberField field);
[[nodiscard]] bool Offers(std::span<const RuleChoice> choices, int value);
[[nodiscard]] const RuleSkillChoice* SkillOf(int skill_id, const RuleVocabulary& vocabulary);

namespace {

// LE NOM DE LA CHARGE D'UN GLISSER DE RÈGLE : son rang dans la liste.
constexpr const char* kRulePayload = "companion_rule";
// Le surlignage de la règle qui vient de décider.
constexpr ImU32 kDecidingColor = IM_COL32(255, 205, 60, 110);
// La raison d'un geste refusé.
constexpr ImU32 kRefusalColor = IM_COL32(153, 31, 31, 255);
// Au-delà de cette borne, un nombre se tape plutôt qu'il ne se glisse.
constexpr int kSliderMost = 1000;
constexpr int kTypedStep = 1;
constexpr int kTypedFastStep = 10;
// La largeur des listes de l'éditeur, en pixels de police.
constexpr float kComboPixels = 220.0F;
constexpr float kNumberPixels = 140.0F;

[[nodiscard]] float Px(float pixels) { return pixels * ImGui::GetStyle().FontScaleMain; }

}  // namespace

int* FieldOf(RuleConditionFields& condition, RuleNumberField field) {
  switch (field) {
    case RuleNumberField::kNumber:
      return &condition.number;
    case RuleNumberField::kSeconds:
      return &condition.seconds;
    case RuleNumberField::kCells:
      return &condition.cells;
    case RuleNumberField::kNone:
      break;
  }
  return nullptr;
}

bool Offers(std::span<const RuleChoice> choices, int value) {
  return std::ranges::any_of(choices,
                             [value](const RuleChoice& one) { return one.value == value; });
}

namespace {

[[nodiscard]] const char* LabelOf(std::span<const RuleChoice> choices, int value) {
  for (const RuleChoice& one : choices) {
    if (one.value == value) {
      return one.label.c_str();
    }
  }
  return "?";
}

[[nodiscard]] const RuleConditionKind* KindOf(const RuleConditionFields& condition,
                                              const RuleVocabulary& vocabulary) {
  for (const RuleConditionKind& kind : vocabulary.conditions) {
    if (kind.subject == condition.subject && kind.predicate == condition.predicate) {
      return &kind;
    }
  }
  return nullptr;
}

[[nodiscard]] const RuleActionKind* ActionOf(const RuleFields& rule,
                                             const RuleVocabulary& vocabulary) {
  for (const RuleActionKind& kind : vocabulary.actions) {
    if (kind.action == rule.action) {
      return &kind;
    }
  }
  return nullptr;
}

}  // namespace

const RuleSkillChoice* SkillOf(int skill_id, const RuleVocabulary& vocabulary) {
  for (const RuleSkillChoice& skill : vocabulary.skills) {
    if (skill.skill_id == skill_id) {
      return &skill;
    }
  }
  return nullptr;
}

}  // namespace ro::ui::companion_ai_parts

namespace ro::ui {

using companion_ai_parts::FieldOf;
using companion_ai_parts::Offers;
using companion_ai_parts::SkillOf;

void FitRuleCondition(RuleConditionFields& condition, const RuleConditionKind& kind) {
  condition.subject = kind.subject;
  condition.predicate = kind.predicate;
  if (!kind.comparisons.empty() && !Offers(kind.comparisons, condition.comparison)) {
    condition.comparison = kind.comparisons.front().value;
  }
  for (const RuleNumberSlot& slot : kind.numbers) {
    if (int* value = FieldOf(condition, slot.field); value != nullptr) {
      *value = std::clamp(*value, slot.least, slot.most);
    }
  }
  int* listed = kind.list == RuleListField::kId       ? &condition.id
                : kind.list == RuleListField::kChoice ? &condition.choice
                                                      : nullptr;
  if (listed != nullptr && !kind.choices.empty() && !Offers(kind.choices, *listed)) {
    *listed = kind.choices.front().value;
  }
}

void FitRuleAction(RuleFields& rule, const RuleActionKind& kind, const RuleVocabulary& vocabulary) {
  std::span<const RuleChoice> aims = kind.aims;
  if (kind.needs_skill) {
    const RuleSkillChoice* skill = SkillOf(rule.skill_id, vocabulary);
    if (skill == nullptr && !vocabulary.skills.empty()) {
      skill = &vocabulary.skills.front();
      rule.skill_id = skill->skill_id;
    }
    if (skill != nullptr) {
      rule.skill_level = std::clamp(rule.skill_level, 0, skill->learned_level);
      aims = skill->aims;
    }
  } else {
    rule.skill_id = 0;
    rule.skill_level = 0;
  }
  // Sans cible d'action, la première : le fichier n'écrit pas une cible
  // héritée d'une autre action.
  if (aims.empty()) {
    rule.aim = 0;
  } else if (!Offers(aims, rule.aim)) {
    rule.aim = aims.front().value;
  }
  rule.step_cells = kind.needs_step ? std::clamp(rule.step_cells, 1, vocabulary.step_most) : 0;
}

}  // namespace ro::ui

namespace ro::ui::companion_ai_parts {
namespace {

// UNE LISTE DE CHOIX. Rend vrai quand la valeur a changé.
[[nodiscard]] bool ChoiceCombo(const char* id, int* value, std::span<const RuleChoice> choices,
                               const SkinArt& art) {
  bool changed = false;
  ImGui::SetNextItemWidth(Px(kComboPixels));
  if (BeginSkinCombo(id, LabelOf(choices, *value), art)) {
    for (std::size_t index = 0; index < choices.size(); ++index) {
      const RuleChoice& one = choices[index];
      ImGui::PushID(static_cast<int>(index));
      if (ImGui::Selectable(one.label.c_str(), one.value == *value) && one.value != *value) {
        *value = one.value;
        changed = true;
      }
      ImGui::PopID();
    }
    EndSkinCombo();
  }
  return changed;
}

// ⚠ UNE DISTANCE S'ARRÊTE AU RAYON DE VUE DU SERVEUR, sous la borne du
// vocabulaire : au-delà, rien ne se mesure.
[[nodiscard]] bool NumberField(const RuleNumberSlot& slot, int* value, int view_cells,
                               const SkinArt& art) {
  ImGui::SetNextItemWidth(Px(kNumberPixels));
  const int most = slot.field == RuleNumberField::kCells
                       ? std::max(slot.least, CompanionAiCellsShown(view_cells, slot.most))
                       : slot.most;
  if (most <= kSliderMost) {
    return SkinSliderInt(slot.label.c_str(), value, slot.least, most, art);
  }
  return SkinInputInt(slot.label.c_str(), value, slot.least, slot.most, kTypedStep, kTypedFastStep,
                      art);
}

[[nodiscard]] bool DrawCondition(RuleConditionFields& condition, const RuleVocabulary& vocabulary,
                                 const SkinArt& art) {
  bool changed = false;
  const RuleConditionKind* kind = KindOf(condition, vocabulary);
  ImGui::SetNextItemWidth(Px(kComboPixels));
  if (BeginSkinCombo("##kind", kind != nullptr ? kind->label.c_str() : "?", art)) {
    for (std::size_t index = 0; index < vocabulary.conditions.size(); ++index) {
      const RuleConditionKind& each = vocabulary.conditions[index];
      ImGui::PushID(static_cast<int>(index));
      if (ImGui::Selectable(each.label.c_str(), &each == kind) && &each != kind) {
        FitRuleCondition(condition, each);
        kind = &each;
        changed = true;
      }
      ImGui::PopID();
    }
    EndSkinCombo();
  }
  if (kind == nullptr) {
    return changed;
  }
  SkinTooltip(kind->tip.c_str());
  if (kind->comparisons.size() > 1) {
    ImGui::SameLine();
    changed |= ChoiceCombo("##comparison", &condition.comparison, kind->comparisons, art);
  } else if (!kind->comparisons.empty()) {
    ImGui::SameLine();
    ImGui::TextUnformatted(kind->comparisons.front().label.c_str());
  }
  if (kind->list != RuleListField::kNone) {
    int* listed = kind->list == RuleListField::kId ? &condition.id : &condition.choice;
    changed |= ChoiceCombo("##list", listed, kind->choices, art);
  }
  for (std::size_t index = 0; index < kind->numbers.size(); ++index) {
    const RuleNumberSlot& slot = kind->numbers[index];
    if (int* value = FieldOf(condition, slot.field); value != nullptr) {
      ImGui::PushID(static_cast<int>(index));
      changed |= NumberField(slot, value, vocabulary.view_cells, art);
      ImGui::PopID();
    }
  }
  return changed;
}

[[nodiscard]] bool DrawAction(RuleFields& rule, const RuleVocabulary& vocabulary,
                              const SkinArt& art) {
  bool changed = false;
  const RuleActionKind* kind = ActionOf(rule, vocabulary);
  ImGui::SetNextItemWidth(Px(kComboPixels));
  if (BeginSkinCombo("##action", kind != nullptr ? kind->label.c_str() : "?", art)) {
    for (std::size_t index = 0; index < vocabulary.actions.size(); ++index) {
      const RuleActionKind& each = vocabulary.actions[index];
      ImGui::PushID(static_cast<int>(index));
      if (ImGui::Selectable(each.label.c_str(), &each == kind) && &each != kind) {
        rule.action = each.action;
        FitRuleAction(rule, each, vocabulary);
        kind = &each;
        changed = true;
      }
      ImGui::PopID();
    }
    EndSkinCombo();
  }
  if (kind == nullptr) {
    return changed;
  }
  SkinTooltip(kind->tip.c_str());
  std::span<const RuleChoice> aims = kind->aims;
  if (kind->needs_skill) {
    const RuleSkillChoice* skill = SkillOf(rule.skill_id, vocabulary);
    ImGui::SetNextItemWidth(Px(kComboPixels));
    if (BeginSkinCombo("##skill", skill != nullptr ? skill->label.c_str() : "?", art)) {
      for (const RuleSkillChoice& each : vocabulary.skills) {
        ImGui::PushID(each.skill_id);
        if (ImGui::Selectable(each.label.c_str(), &each == skill) && &each != skill) {
          rule.skill_id = each.skill_id;
          FitRuleAction(rule, *kind, vocabulary);
          skill = &each;
          changed = true;
        }
        ImGui::PopID();
      }
      EndSkinCombo();
    }
    if (skill != nullptr) {
      ImGui::SameLine();
      ImGui::SetNextItemWidth(Px(kNumberPixels));
      changed |= LevelCombo(core::Tr("Niveau"), &rule.skill_level, skill->learned_level, art);
      aims = skill->aims;
    }
  }
  if (!aims.empty()) {
    ImGui::TextUnformatted(core::Tr("Sur"));
    ImGui::SameLine();
    changed |= ChoiceCombo("##aim", &rule.aim, aims, art);
  }
  if (kind->needs_step) {
    ImGui::SetNextItemWidth(Px(kNumberPixels));
    changed |=
        SkinSliderInt(core::Tr("S'écarter de (cases)"), &rule.step_cells, 1,
                      CompanionAiCellsShown(vocabulary.view_cells, vocabulary.step_most), art);
  }
  return changed;
}

[[nodiscard]] bool DrawAttributes(RuleFields& rule, const SkinArt& art) {
  bool changed = false;
  ImGui::SetNextItemWidth(Px(kNumberPixels));
  changed |= TypedSeconds(core::Tr("Pas plus d'une fois toutes les (s)"), &rule.own_delay_ms,
                          kCompanionAiRuleDelayMostMs, art);
  SkinTooltip(core::Tr("Un délai propre à la règle. 0 : aucun."));
  ImGui::SetNextItemWidth(Px(kNumberPixels));
  changed |= SkinSliderInt(core::Tr("Au plus par cible"), &rule.per_target_most, 0,
                           kCompanionAiPerTargetMost, art);
  SkinTooltip(core::Tr("Combien de fois elle agit sur une même cible de combat. 0 : sans limite."));
  changed |= SkinCheckBox(core::Tr("Ignore la SP à garder"), &rule.ignores_sp_reserve, art);
  SkinTooltip(core::Tr("Son skill part même s'il entame la SP à garder."));
  return changed;
}

// L'ÉDITEUR D'UNE RÈGLE DÉPLIÉE. Rend vrai quand elle a changé.
[[nodiscard]] bool DrawRuleEditor(RuleFields& rule, const RuleVocabulary& vocabulary,
                                  const SkinArt& art) {
  bool changed = false;
  ImGui::SeparatorText(core::Tr("Si"));
  std::size_t removed = rule.conditions.size();
  for (std::size_t index = 0; index < rule.conditions.size(); ++index) {
    ImGui::PushID(static_cast<int>(index));
    if (index > 0) {
      ImGui::TextDisabled("%s", core::Tr("et"));
    }
    changed |= DrawCondition(rule.conditions[index], vocabulary, art);
    if (rule.conditions.size() > 1 && SkinSmallButton(core::Tr("Retirer la condition"), art)) {
      removed = index;
    }
    ImGui::PopID();
  }
  if (removed < rule.conditions.size()) {
    rule.conditions.erase(rule.conditions.begin() + static_cast<std::ptrdiff_t>(removed));
    changed = true;
  }
  if (rule.conditions.size() < kCompanionAiConditionMost && !vocabulary.conditions.empty() &&
      SkinSmallButton(core::Tr("Ajouter une condition"), art)) {
    RuleConditionFields added;
    FitRuleCondition(added, vocabulary.conditions.front());
    rule.conditions.push_back(added);
    changed = true;
  }
  ImGui::SeparatorText(core::Tr("Alors"));
  changed |= DrawAction(rule, vocabulary, art);
  ImGui::SeparatorText(core::Tr("Attributs"));
  changed |= DrawAttributes(rule, art);
  return changed;
}

// LA LIGNE D'UNE RÈGLE : un clic la déplie, un glisser la déplace.
void DrawRuleRow(CompanionAiWindowState& state, const RuleRowView& row, std::size_t index,
                 CompanionAiWindowAction& action) {
  const bool expanded = state.expanded_rule == static_cast<int>(index);
  const std::string label = std::to_string(index + 1) + ". " + row.sentence;
  // UNE RÈGLE QUI NE PEUT PAS AGIR se grise, comme une règle écartée, et dit
  // pourquoi ; elle reste éditable.
  const bool unusable = !row.unusable.empty();
  if (unusable) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
  }
  if (ImGui::Selectable(label.c_str(), expanded)) {
    state.expanded_rule = expanded ? -1 : static_cast<int>(index);
  }
  if (unusable) {
    ImGui::PopStyleColor();
  }
  if (row.deciding) {
    ImGui::GetWindowDrawList()->AddRectFilled(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(),
                                              kDecidingColor);
  }
  SkinTooltip(core::Tr("Clic : l'éditer. Glisser : la déplacer."));
  if (ImGui::BeginDragDropSource()) {
    const int from = static_cast<int>(index);
    ImGui::SetDragDropPayload(kRulePayload, &from, sizeof(from));
    ImGui::TextUnformatted(label.c_str());
    ImGui::EndDragDropSource();
  }
  if (ImGui::BeginDragDropTarget()) {
    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kRulePayload);
        payload != nullptr && payload->DataSize == static_cast<int>(sizeof(int))) {
      int from = 0;
      std::memcpy(&from, payload->Data, sizeof(from));
      if (from >= 0 && from != static_cast<int>(index)) {
        action.verb = CompanionAiVerb::kMoveRule;
        action.index = static_cast<std::size_t>(from);
        action.to = index;
      }
    }
    ImGui::EndDragDropTarget();
  }
  if (unusable) {
    ImGui::Indent();
    ImGui::TextDisabled("%s", row.unusable.c_str());
    ImGui::Unindent();
  }
}

}  // namespace

void DrawRulesSection(CompanionAiWindowState& state, const CompanionAiView& view,
                      const SkinArt& art, CompanionAiWindowAction& action) {
  companion_ai_parts::WrappedNote(
      core::Tr("Les règles se lisent de haut en bas : la première vraie et "
               "faisable donne l'ordre. Elles se taisent en attente et "
               "pendant un ordre direct."));
  const bool full = view.rules.size() >= kCompanionAiRuleMost;
  if (view.rules.empty()) {
    ImGui::TextDisabled("%s", core::Tr("Aucune règle : les réglages et les tactiques décident."));
  }
  for (std::size_t index = 0; index < view.rules.size(); ++index) {
    const RuleRowView& row = view.rules[index];
    ImGui::PushID(static_cast<int>(index));
    DrawRuleRow(state, row, index, action);
    if (state.expanded_rule == static_cast<int>(index)) {
      ImGui::Indent();
      RuleFields fields = row.fields;
      if (DrawRuleEditor(fields, view.vocabulary, art)) {
        action.verb = CompanionAiVerb::kEditRule;
        action.index = index;
        action.rule = std::move(fields);
      }
      if (!view.refusal.empty() && view.refused_rule == static_cast<int>(index)) {
        ImGui::PushStyleColor(ImGuiCol_Text, kRefusalColor);
        companion_ai_parts::WrappedNote(view.refusal.c_str());
        ImGui::PopStyleColor();
      }
      ImGui::BeginDisabled(full);
      if (SkinSmallButton(core::Tr("Dupliquer"), art)) {
        action.verb = CompanionAiVerb::kDuplicateRule;
        action.index = index;
      }
      ImGui::EndDisabled();
      if (full && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        SkinTooltipFormat(core::Tr("%d règles au plus par profil."),
                          static_cast<int>(kCompanionAiRuleMost));
      }
      ImGui::SameLine();
      if (SkinSmallButton(core::Tr("Supprimer"), art)) {
        action.verb = CompanionAiVerb::kRemoveRule;
        action.index = index;
      }
      ImGui::Unindent();
      ImGui::Separator();
    }
    ImGui::PopID();
  }
  ImGui::BeginDisabled(full);
  if (SkinSmallButton(core::Tr("Ajouter une règle"), art)) {
    action.verb = CompanionAiVerb::kAddRule;
  }
  ImGui::EndDisabled();
  if (full && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    SkinTooltipFormat(core::Tr("%d règles au plus par profil."),
                      static_cast<int>(kCompanionAiRuleMost));
  }
  if (!view.rejected_rules.empty()) {
    ImGui::SeparatorText(core::Tr("Règles écartées à la lecture du fichier"));
    for (const std::string& rejected : view.rejected_rules) {
      ImGui::TextDisabled("%s", rejected.c_str());
    }
  }
}

}  // namespace ro::ui::companion_ai_parts
