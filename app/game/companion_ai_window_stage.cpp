// LA FENÊTRE « IA DU COMPAGNON » VUE DE LA PARTIE : sa vue, composée depuis
// le profil du compagnon, et ses intentions, posées dans le profil, données à
// l'IA et enregistrées. Voir `game/companion_ai_stage.h`.

#include <algorithm>
#include <array>
#include <cstdio>
#include <limits>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ro/core/i18n.h"
#include "ro/core/log.h"
#include "ro/sim/companion_skills.h"
#include "ro/sim/monster_sheet.h"
#include "ro/sim/skill_tree.h"
#include "ro/vfs/monster_words.h"

#include "game/companion_ai_editor.h"
#include "game/companion_ai_names.h"
#include "game/companion_ai_stage.h"
#include "game/companion_tactic_fields.h"
#include "game/homunculus_stage.h"
#include "game/mercenary_stage.h"

namespace moonclient {
namespace {

using ro::sim::AiCompanion;
using ro::sim::CompanionRule;
using ro::sim::CompanionTactic;

constexpr std::size_t kNameBytes = 64;

[[nodiscard]] int WindowIndexOf(AiCompanion companion) noexcept {
  return companion == AiCompanion::kHomunculus ? ro::ui::kCompanionAiHomunculus
                                               : ro::ui::kCompanionAiMercenary;
}

[[nodiscard]] AiCompanion CompanionOfWindow(int index) noexcept {
  return index == ro::ui::kCompanionAiMercenary ? AiCompanion::kMercenary
                                                : AiCompanion::kHomunculus;
}

// LES SKILLS DU COMPAGNON, et leur nom interne du serveur.
struct OwnSkill {
  RuleSkillFact fact;
  std::string server_name;
};

[[nodiscard]] std::vector<OwnSkill> SkillsOf(AiCompanion companion,
                                             const HomunculusStage& homunculus,
                                             const MercenaryStage& mercenary) {
  std::vector<OwnSkill> skills;
  if (companion == AiCompanion::kHomunculus) {
    for (const ro::sim::HomunculusSkill& skill : homunculus.homunculus().skills()) {
      skills.push_back(OwnSkill{
          .fact = {.skill_id = skill.skill_id, .level = skill.level, .targeting = skill.targeting},
          .server_name = skill.server_name});
    }
    return skills;
  }
  for (const ro::sim::MercenarySkill& skill : mercenary.book().skills()) {
    skills.push_back(OwnSkill{
        .fact = {.skill_id = skill.skill_id, .level = skill.level, .targeting = skill.targeting},
        .server_name = skill.name});
  }
  return skills;
}

// LE NOM D'UNE CLASSE DE MONSTRE : celui d'un exemplaire en vue, sinon celui de
// sa fiche, sinon son numéro.
[[nodiscard]] std::string MonsterName(const std::map<std::uint16_t, std::string>& visible,
                                      const ro::sim::MonsterSheetBook* sheets, std::uint16_t job) {
  if (const auto seen = visible.find(job); seen != visible.end()) {
    return seen->second;
  }
  if (sheets != nullptr) {
    if (const ro::sim::MonsterSheet* sheet = sheets->Find(job);
        sheet != nullptr && !sheet->name.empty()) {
      return sheet->name;
    }
  }
  std::array<char, kNameBytes> name{};
  std::snprintf(name.data(), name.size(), ro::core::Tr("Monstre %u"), static_cast<unsigned>(job));
  return name.data();
}

}  // namespace

const char* CompanionAiStage::ProfileLabel(AiCompanion companion) const {
  const Slot& slot = SlotOf(companion);
  if (slot.actor_id == 0 || !slot.seat.applied.has_value()) {
    return "";
  }
  std::string& label = profile_labels_[companion == AiCompanion::kHomunculus ? 0 : 1];
  label = ProfileLabelOf(slot.seat.applied->second);
  return label.c_str();
}

void CompanionAiStage::OpenAiWindow(AiCompanion companion, ro::ui::CompanionAiWindowState& window) {
  if (!present(companion)) {
    return;
  }
  window.open = true;
  window.focus_asked = true;
  if (window.companion != WindowIndexOf(companion)) {
    ro::ui::ChangeAiWindowCompanion(window, WindowIndexOf(companion));
  }
}

void CompanionAiStage::StageAiWindow(const HomunculusStage& homunculus,
                                     const MercenaryStage& mercenary,
                                     const CompanionAiWindowBooks& books,
                                     ro::ui::CompanionAiWindowState& window,
                                     ro::ui::CompanionAiView& view) {
  const std::array<bool, 2> present_now = {present(AiCompanion::kHomunculus),
                                           present(AiCompanion::kMercenary)};
  // UN COMPAGNON PARTI, OU LA FENÊTRE FERMÉE : ce qu'on a réglé s'enregistre
  // sans attendre, et l'état propre au compagnon repart de zéro.
  if (FollowAiWindow(window, window_watch_, present_now)) {
    SaveProfiles();
  }
  if (!window.open) {
    view = ro::ui::CompanionAiView();
    return;
  }
  const AiCompanion companion = CompanionOfWindow(window.companion);
  const Slot& slot = SlotOf(companion);
  const std::uint16_t job = slot.followed.job.value_or(0);
  // LE PROFIL ÉDITÉ, qui n'est pas forcément celui qui est appliqué : la vue
  // montre ses réglages.
  const std::string edited = EditedProfile(slot, window);
  const bool edits_applied = edited == AppliedProfile(slot);
  // LE PROFIL APPLIQUÉ a déjà ses réglages ; un autre se compose.
  if (!edits_applied && job != 0) {
    edited_settings_ = session_.SettingsFor(companion, job, edited);
  }
  const ro::sim::CompanionAiSettings& settings =
      edits_applied || job == 0 ? slot.settings : edited_settings_;
  const std::vector<OwnSkill> skills = SkillsOf(companion, homunculus, mercenary);

  // ── CE QUI NOMME ──
  // Les monstres en vue, une fois par image : leur nom, par classe.
  visible_monsters_.clear();
  for (const auto& [actor_id, actor] : books.world.actors()) {
    if (actor.kind == ro::sim::ActorKind::kMonster && actor.look.job != 0 && !actor.name.empty()) {
      visible_monsters_.emplace(actor.look.job, actor.name);
    }
  }
  std::map<std::uint16_t, std::string> server_names;
  for (const OwnSkill& skill : skills) {
    server_names[skill.fact.skill_id] = skill.server_name;
  }
  RuleNames names;
  names.skill = [&books, &server_names](std::uint16_t skill_id) {
    if (books.skill_tree != nullptr) {
      if (const ro::sim::SkillNode* node = books.skill_tree->Find(skill_id);
          node != nullptr && !node->display_name.empty()) {
        return node->display_name;
      }
    }
    const auto found = server_names.find(skill_id);
    return found != server_names.end() && !found->second.empty() ? found->second
                                                                 : std::to_string(skill_id);
  };
  names.status = [&books](std::uint16_t status) {
    const std::string_view named =
        books.statuses != nullptr ? books.statuses->NameOf(status) : std::string_view();
    return named.empty() ? std::to_string(status) : std::string(named);
  };
  names.monster = [this, &books](std::uint16_t monster_job) {
    return MonsterName(visible_monsters_, books.monster_sheets, monster_job);
  };
  if (books.words != nullptr) {
    names.race = [&books](int race) {
      return std::string(ro::vfs::MonsterRaceWord(*books.words, static_cast<std::uint8_t>(race)));
    };
    names.element = [&books](int element) {
      return std::string(
          ro::vfs::MonsterElementWord(*books.words, static_cast<std::uint8_t>(element)));
    };
    names.size = [&books](int size) {
      return std::string(ro::vfs::MonsterSizeWord(*books.words, static_cast<std::uint8_t>(size)));
    };
  }

  // ── LES STATUTS NOMMÉS, triés par nom, refaits quand la table change ──
  if (books.statuses != nullptr && books.statuses->size() != status_table_size_) {
    status_table_size_ = books.statuses->size();
    status_ids_.clear();
    for (const auto& [effect_id, info] : books.statuses->by_id) {
      if (!info.name.empty() && effect_id != 0 &&
          effect_id <= std::numeric_limits<std::uint16_t>::max()) {
        status_ids_.push_back(static_cast<std::uint16_t>(effect_id));
      }
    }
    std::ranges::sort(status_ids_, [&books](std::uint16_t left, std::uint16_t right) {
      return books.statuses->NameOf(left) < books.statuses->NameOf(right);
    });
  }

  // ── RÉGLAGES ──
  offensive_rows_.clear();
  for (const OwnSkill& skill : skills) {
    const ro::sim::CompanionSkillFact fact{
        .skill_id = skill.fact.skill_id,
        .level = static_cast<std::uint16_t>(std::max(skill.fact.level, 0)),
        .targeting = skill.fact.targeting};
    if (ro::sim::IsOffensiveCompanionSkill(fact)) {
      offensive_rows_.push_back(ro::ui::OffensiveSkillRow{.skill_id = fact.skill_id,
                                                          .label = names.skill(fact.skill_id),
                                                          .idname = skill.server_name,
                                                          .learned_level = fact.level});
    }
  }

  // ── TACTIQUES ──
  tactic_rows_.clear();
  tactic_row_keys_.clear();
  std::array<bool, ro::ui::kTacticGenericChoices> generic_present{};
  std::vector<std::uint16_t> monster_jobs;
  for (const CompanionTactic& line : settings.tactics.lines()) {
    const int generic = GenericIndexOf(line.key.kind);
    std::string label;
    if (generic >= 0) {
      generic_present[static_cast<std::size_t>(generic)] = true;
      label = ro::ui::CompanionTacticGenericLabel(generic);
    } else {
      label = names.monster(line.key.job);
      monster_jobs.push_back(line.key.job);
    }
    tactic_rows_.push_back(ro::ui::TacticRowView{.label = std::move(label),
                                                 .rank = static_cast<int>(line.rank),
                                                 .trigger = static_cast<int>(line.trigger),
                                                 .advanced = FieldsOf(line)});
    tactic_row_keys_.push_back(line.key);
  }
  CollectOpenings(companion, homunculus, mercenary, books.skill_tree, ai_openings_);
  // Les monstres en vue s'ajoutent à la liste « monstre de la cible », dans
  // l'ordre de leur classe.
  for (const auto& [monster_job, name] : visible_monsters_) {
    if (std::ranges::find(monster_jobs, monster_job) == monster_jobs.end()) {
      monster_jobs.push_back(monster_job);
    }
  }

  // ── RÈGLES ──
  std::vector<RuleSkillFact> rule_skills;
  for (const OwnSkill& skill : skills) {
    rule_skills.push_back(skill.fact);
  }
  vocabulary_.Build(rule_skills, status_ids_, monster_jobs, settings.rules, names);
  rule_rows_.clear();
  // LA RÈGLE QUI DÉCIDE ne se surligne que dans le profil qui décide.
  const std::optional<ro::sim::DecidingRule>& deciding = slot.brain.deciding_rule();
  // UNE RÈGLE À SKILL NON POSSÉDÉ se grise, avec sa raison.
  std::vector<std::uint16_t> owned;
  for (const OwnSkill& skill : skills) {
    if (skill.fact.level > 0) {
      owned.push_back(skill.fact.skill_id);
    }
  }
  const std::vector<std::size_t> missing = RulesMissingSkill(settings.rules, owned);
  for (std::size_t index = 0; index < settings.rules.size(); ++index) {
    const CompanionRule& rule = settings.rules[index];
    rule_rows_.push_back(ro::ui::RuleRowView{
        .sentence = RuleSentence(rule, names),
        .fields = RuleFieldsOf(rule),
        .deciding = edits_applied && deciding.has_value() && deciding->index == index &&
                    last_now_ms_ - deciding->at_ms < kDecidingRuleShownMs,
        .unusable = std::ranges::find(missing, index) != missing.end()
                        ? std::string(ro::core::Tr("Ce compagnon ne possède pas ce skill : la "
                                                   "règle ne peut pas agir."))
                        : std::string()});
  }
  rejected_lines_.clear();
  friend_rows_ = settings.friends;
  ComposePlayers(settings, books);
  ro::ui::CompanionProfilesView profiles;
  if (job != 0) {
    ComposeProfiles(companion, job, edited, profiles);
  }
  if (const CompanionProfile* profile = session_.profiles().Find(job, edited); profile != nullptr) {
    for (const RejectedRule& rejected : profile->rejected_rules) {
      rejected_lines_.push_back(RejectedRuleLine(rejected));
    }
  }

  window_companion_name_ = companion == AiCompanion::kHomunculus
                               ? std::string(homunculus.view().name)
                               : std::string(mercenary.view().name);
  if (window_companion_name_.empty()) {
    window_companion_name_ = companion == AiCompanion::kHomunculus ? ro::core::Tr("Homoncule")
                                                                   : ro::core::Tr("Mercenaire");
  }
  // LE REFUS NE SE DIT QU'AU COMPAGNON QUI L'A REÇU.
  const bool refusal_here = refusal_.companion == window.companion && refusal_.job == job;
  view = ro::ui::CompanionAiView{
      .homunculus_present = present_now[0],
      .mercenary_present = present_now[1],
      .companion = window.companion,
      .companion_class = job,
      .companion_name = window_companion_name_,
      .profile = ProfileLabel(companion),
      .editable = job != 0,
      .refusal = refusal_here ? refusal_.text : std::string(),
      .refused_rule = refusal_here ? refusal_.rule : -1,
      .profiles = std::move(profiles),
      .basic = edited == kBasicProfileName,
      .base = BaseFieldsOf(settings, offensive_rows_),
      .offensive_skills = offensive_rows_,
      .tactics = tactic_rows_,
      .openings = ai_openings_,
      .generic_present = generic_present,
      .tactics_full = settings.tactics.lines().size() >= ro::sim::kCompanionTacticMost,
      .view_cells = books.world.session().view_cells,
      .rules = rule_rows_,
      .rejected_rules = rejected_lines_,
      .vocabulary = vocabulary_.view(),
      .friends = friend_rows_,
      .conducts = conduct_rows_,
      .conduct_generic_present = conduct_generic_present_,
      .conducts_full = settings.player_conducts.lines().size() >= ro::sim::kCompanionConductMost,
      .player_jobs = player_jobs_};
  // LES DISTANCES DES RÈGLES s'arrêtent au même rayon que celles des réglages.
  view.vocabulary.view_cells = view.view_cells;
}

AiEditTarget CompanionAiStage::EditTargetOf(AiCompanion companion) {
  if (SlotOf(companion).actor_id == 0) {
    return AiEditTarget{.companion = companion};
  }
  return session_.EditTarget(Seated(companion));
}

void CompanionAiStage::Absorb(AiCompanion companion, const AiEditResult& result,
                              std::uint32_t now_ms) {
  Slot& slot = SlotOf(companion);
  if (!result.refusal.empty()) {
    ro::core::LogInfo("IA des compagnons : geste refusé — {}", result.refusal);
    refusal_ = Refusal{.companion = WindowIndexOf(companion),
                       .job = slot.followed.job.value_or(0),
                       .rule = result.refused_rule,
                       .text = result.refusal};
  }
  if (!result.accepted) {
    return;
  }
  refusal_ = Refusal();
  session_.Refresh(Seated(companion));
  if (result.save == AiEditSave::kNow) {
    SaveProfiles();
  } else if (result.save == AiEditSave::kAfterCalm) {
    edited_since_ms_ = now_ms;
  }
}

void CompanionAiStage::ApplyAiWindow(const ro::ui::CompanionAiWindowAction& action,
                                     ro::ui::CompanionAiWindowState& window, std::uint32_t now_ms) {
  if (action.verb == ro::ui::CompanionAiVerb::kNothing) {
    return;
  }
  const AiCompanion companion = CompanionOfWindow(action.companion);
  if (!present(companion)) {
    return;
  }
  const SeatedCompanion seated = Seated(companion);
  const AiWindowOutcome outcome =
      ApplyAiWindowGesture(action, session_, seated, profiles_view_.keys, profiles_view_.imports,
                           tactic_row_keys_, conduct_row_keys_);
  // LES GESTES DE L'EN-TÊTE DES PROFILS s'enregistrent aussitôt.
  if (const ProfileGestureResult& gesture = outcome.gesture; gesture.handled) {
    if (!gesture.refusal.empty()) {
      Absorb(companion, AiEditResult{.refusal = gesture.refusal}, now_ms);
      return;
    }
    if (gesture.accepted) {
      refusal_ = Refusal();
      if (gesture.edited.has_value()) {
        window.edited_profile = *gesture.edited;
        window.expanded_rule = -1;
        window.expanded_tactic = -1;
        window.expanded_conduct = -1;
      }
      SaveProfiles();
      RefreshSettings();
    }
    return;
  }
  // ⚠ `Absorb` PEUT ENREGISTRER, et un enregistrement remplace les profils en
  // mémoire : rien du profil visé ne se relit après lui, d'où `rule_count`,
  // relevé avant.
  Absorb(companion, outcome.edit, now_ms);
  if (outcome.edit.accepted) {
    ro::ui::FollowAcceptedAiGesture(window, action, outcome.rule_count);
  }
}

bool CompanionAiStage::IsCompanionFriend(AiCompanion companion, const std::string& name) const {
  const std::vector<std::string>& friends = SlotOf(companion).settings.friends;
  return std::ranges::find(friends, name) != friends.end();
}

bool CompanionAiStage::CompanionFriendsFull(AiCompanion companion) const {
  return SlotOf(companion).settings.friends.size() >= ro::sim::kCompanionFriendMost;
}

void CompanionAiStage::SetCompanionFriend(AiCompanion companion, const std::string& name,
                                          bool befriend, std::uint32_t now_ms) {
  if (!present(companion)) {
    return;
  }
  Absorb(companion, SetProfileFriend(EditTargetOf(companion), name, befriend), now_ms);
}

}  // namespace moonclient
