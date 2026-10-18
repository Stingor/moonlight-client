// LES TACTIQUES PAR MONSTRE DE L'IA DES COMPAGNONS : ce que le sous-menu et la
// fenêtre « Avancé… » changent dans le profil appliqué, et l'enregistrement des
// profils. Voir `game/companion_ai_stage.h`.

#include <string>
#include <utility>

#include "ro/core/i18n.h"
#include "ro/core/log.h"
#include "ro/core/printed.h"
#include "ro/sim/companion_skills.h"
#include "ro/sim/skill_book.h"

#include "game/companion_ai_stage.h"
#include "game/companion_tactic_fields.h"
#include "game/homunculus_stage.h"
#include "game/mercenary_stage.h"
#include "world/entity_menu.h"

namespace moonclient {
namespace {

using ro::sim::AiCompanion;
using ro::sim::CompanionTactic;
using ro::sim::TacticKey;
using ro::sim::TacticKeyKind;

[[nodiscard]] TacticKey MonsterKey(std::uint16_t monster_job) noexcept {
  return TacticKey{.kind = TacticKeyKind::kMonster, .job = monster_job};
}

// L'OUVERTURE NE PROPOSE QUE CE QUE L'IA ACCEPTE DE LANCER : un skill appris,
// offensif, et que le cerveau ne s'interdit pas.
[[nodiscard]] bool OpeningCandidate(std::uint16_t skill_id, int level,
                                    std::uint16_t targeting) noexcept {
  return level > 0 && (targeting & ro::sim::kSkillOnEnemy) != 0 &&
         !ro::sim::NeverCastAutomatically(skill_id);
}

// Le nom d'un skill : celui du jeu, sinon le nom interne du serveur.
[[nodiscard]] std::string SkillLabel(const ro::sim::SkillTree* skill_tree, std::uint16_t skill_id,
                                     const std::string& server_name) {
  if (skill_tree != nullptr) {
    if (const ro::sim::SkillNode* node = skill_tree->Find(skill_id);
        node != nullptr && !node->display_name.empty()) {
      return node->display_name;
    }
  }
  return server_name.empty() ? std::to_string(skill_id) : server_name;
}

}  // namespace

std::optional<CompanionTactic> CompanionAiStage::TacticFor(AiCompanion companion,
                                                           std::uint16_t monster_job) const {
  const Slot& slot = SlotOf(companion);
  if (slot.actor_id == 0) {
    return std::nullopt;
  }
  const CompanionTactic* line = slot.settings.tactics.Find(MonsterKey(monster_job));
  return line != nullptr ? std::optional(*line) : std::nullopt;
}

void CompanionAiStage::ChooseTactic(AiCompanion companion, std::uint16_t monster_job, int choice) {
  if (choice == static_cast<int>(TacticMenuChoice::kRemove)) {
    static_cast<void>(DropTactic(companion, MonsterKey(monster_job)));
    return;
  }
  CompanionTactic line =
      TacticFor(companion, monster_job).value_or(CompanionTactic{.key = MonsterKey(monster_job)});
  const int rank = choice - static_cast<int>(TacticMenuChoice::kRankFirst);
  const int trigger = choice - static_cast<int>(TacticMenuChoice::kTriggerFirst);
  if (rank >= 0 && rank < static_cast<int>(ro::sim::kTacticRankCount)) {
    line.rank = static_cast<ro::sim::TacticRank>(rank);
  } else if (trigger >= 0 && trigger < static_cast<int>(ro::sim::kTacticTriggerCount)) {
    line.trigger = static_cast<ro::sim::TacticTrigger>(trigger);
  } else {
    return;
  }
  if (StoreTactic(companion, line)) {
    SaveProfiles();
  }
}

bool CompanionAiStage::StoreTactic(AiCompanion companion, const CompanionTactic& tactic) {
  const AiEditTarget target = EditTargetOf(companion);
  if (target.profile == nullptr) {
    return false;
  }
  if (!target.profile->tactics.Set(tactic)) {
    ro::core::LogWarning("IA des compagnons : tactique refusée (table pleine ou hors du cadre)");
    return false;
  }
  session_.edits().Set(target.job, target.profile_name, tactic);
  session_.Refresh(Seated(companion));
  return true;
}

bool CompanionAiStage::DropTactic(AiCompanion companion, TacticKey key) {
  Slot& slot = SlotOf(companion);
  if (slot.actor_id == 0 || !slot.seat.applied.has_value()) {
    return false;
  }
  // L'IA basique n'a aucune ligne à retirer : rien n'est créé pour cela.
  const auto& [job, name] = *slot.seat.applied;
  CompanionProfile* profile = session_.profiles().Find(job, name);
  if (profile == nullptr || !profile->tactics.Remove(key)) {
    return false;
  }
  session_.edits().Remove(job, name, key);
  session_.Refresh(Seated(companion));
  SaveProfiles();
  return true;
}

void CompanionAiStage::SaveProfiles() {
  edited_since_ms_.reset();
  // LE FICHIER ÉCRIT FAIT FOI : il porte aussi les lignes qu'un autre client
  // vient d'y enregistrer.
  if (session_.SaveProfiles()) {
    profiles_view_.imports_stale = true;
    RefreshSettings();
  }
}

void CompanionAiStage::SaveQuietEdits(std::uint32_t now_ms) {
  if (edited_since_ms_.has_value() && now_ms - *edited_since_ms_ >= kTacticSaveQuietMs) {
    SaveProfiles();
  }
}

void CompanionAiStage::OpenTacticWindow(AiCompanion companion, std::uint16_t monster_job,
                                        std::string monster_name,
                                        ro::ui::CompanionTacticWindowState& window) {
  if (!present(companion)) {
    return;
  }
  edited_ = EditedTactic{
      .companion = companion, .monster_job = monster_job, .monster_name = std::move(monster_name)};
  window.open = true;
  window.was_open = false;
}

void CompanionAiStage::StageTacticWindow(const HomunculusStage& homunculus,
                                         const MercenaryStage& mercenary,
                                         const ro::sim::SkillTree* skill_tree,
                                         ro::ui::CompanionTacticWindowState& window,
                                         ro::ui::CompanionTacticView& view) {
  // Le compagnon parti, ou la fenêtre fermée : il n'y a plus rien à montrer, et
  // ce qu'elle a réglé s'enregistre sans attendre.
  if (!edited_.has_value() || !present(edited_->companion)) {
    if (edited_.has_value()) {
      SaveProfiles();
    }
    edited_.reset();
    window.open = false;
    return;
  }
  const bool homunculus_line = edited_->companion == AiCompanion::kHomunculus;
  CollectOpenings(edited_->companion, homunculus, mercenary, skill_tree, openings_);
  window_title_ = ro::core::Printed(ro::core::Tr("Tactique : %s"), edited_->monster_name.c_str());
  window_companion_ = homunculus_line ? ro::core::Tr("Homoncule") : ro::core::Tr("Mercenaire");
  const CompanionTactic line =
      TacticFor(edited_->companion, edited_->monster_job)
          .value_or(CompanionTactic{.key = MonsterKey(edited_->monster_job)});
  view = ro::ui::CompanionTacticView{.title = window_title_,
                                     .companion = window_companion_,
                                     .fields = FieldsOf(line),
                                     .openings = openings_};
}

void CompanionAiStage::CollectOpenings(AiCompanion companion, const HomunculusStage& homunculus,
                                       const MercenaryStage& mercenary,
                                       const ro::sim::SkillTree* skill_tree,
                                       std::vector<ro::ui::TacticSkillChoice>& into) const {
  into.clear();
  if (companion == AiCompanion::kHomunculus) {
    for (const ro::sim::HomunculusSkill& skill : homunculus.homunculus().skills()) {
      if (OpeningCandidate(skill.skill_id, skill.level, skill.targeting)) {
        into.push_back(ro::ui::TacticSkillChoice{
            .skill_id = skill.skill_id,
            .label = SkillLabel(skill_tree, skill.skill_id, skill.server_name)});
      }
    }
    return;
  }
  for (const ro::sim::MercenarySkill& skill : mercenary.book().skills()) {
    if (OpeningCandidate(skill.skill_id, skill.level, skill.targeting)) {
      into.push_back(ro::ui::TacticSkillChoice{
          .skill_id = skill.skill_id, .label = SkillLabel(skill_tree, skill.skill_id, skill.name)});
    }
  }
}

void CompanionAiStage::ApplyTacticWindow(const ro::ui::CompanionTacticWindowAction& action,
                                         std::uint32_t now_ms) {
  // 🔴 LA VALEUR VA À L'IA TOUT DE SUITE, AU FICHIER APRÈS UN SILENCE : un
  // curseur qu'on fait glisser change à chaque image, et réécrire le fichier à
  // chacune ne servirait à rien.
  if (action.changed.has_value() && edited_.has_value()) {
    // UNE LIGNE QUI N'EXISTAIT PAS naît au rang « moyen » et au déclencheur
    // « toujours », avec les champs qu'on vient de régler.
    const CompanionTactic line =
        TacticFor(edited_->companion, edited_->monster_job)
            .value_or(CompanionTactic{.key = MonsterKey(edited_->monster_job)});
    if (StoreTactic(edited_->companion, WithFields(line, *action.changed))) {
      edited_since_ms_ = now_ms;
    }
  }
  if (action.closed) {
    edited_.reset();
    SaveProfiles();
  }
}

}  // namespace moonclient
