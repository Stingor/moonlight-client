// LES GESTES DE LA FENÊTRE « IA DU COMPAGNON » SUR LE PROFIL. Voir
// `game/companion_ai_editor.h`.

#include "game/companion_ai_editor.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include "ro/core/i18n.h"
#include "ro/sim/companion_rules.h"
#include "ro/sim/companion_skills.h"

#include "game/companion_ai_fields.h"
#include "game/companion_tactic_fields.h"

namespace moonclient {
namespace {

using ro::sim::CompanionRule;
using ro::sim::CompanionTactic;
using ro::ui::CompanionAiVerb;

constexpr std::size_t kLineBytes = 160;

[[nodiscard]] AiEditResult Accepted(AiEditSave save) {
  return AiEditResult{.accepted = true, .save = save};
}

[[nodiscard]] AiEditResult Refused(std::string why, int rule = -1) {
  return AiEditResult{.refusal = std::move(why), .refused_rule = rule};
}

[[nodiscard]] ro::sim::CompanionAiSettings EffectiveSettings(const AiEditTarget& target) {
  return SettingsOfProfile(target.companion, target.job, target.profile);
}

// UNE LIGNE DE TACTIQUE posée dans le profil et notée à enregistrer.
[[nodiscard]] AiEditResult StoreTactic(const AiEditTarget& target, const CompanionTactic& line) {
  if (!target.profile->tactics.Set(line)) {
    return Refused(ro::core::Tr("La table des tactiques est pleine."));
  }
  target.edits->Set(target.job, target.profile_name, line);
  return Accepted(AiEditSave::kAfterCalm);
}

[[nodiscard]] AiEditResult ApplyTactic(const ro::ui::CompanionAiWindowAction& action,
                                       const AiEditTarget& target,
                                       std::span<const ro::sim::TacticKey> tactic_keys) {
  if (action.verb == CompanionAiVerb::kAddGenericTactic) {
    return StoreTactic(target, CompanionTactic{.key = GenericKeyOf(action.generic)});
  }
  if (action.index >= tactic_keys.size()) {
    return AiEditResult{};
  }
  const ro::sim::TacticKey key = tactic_keys[action.index];
  if (action.verb == CompanionAiVerb::kRemoveTactic) {
    if (!target.profile->tactics.Remove(key)) {
      return AiEditResult{};
    }
    target.edits->Remove(target.job, target.profile_name, key);
    return Accepted(AiEditSave::kNow);
  }
  const CompanionTactic* current = target.profile->tactics.Find(key);
  CompanionTactic line =
      WithFields(current != nullptr ? *current : CompanionTactic{.key = key}, action.advanced);
  line.rank =
      static_cast<ro::sim::TacticRank>(std::clamp(action.rank, 0, ro::ui::kTacticRankChoices - 1));
  line.trigger = static_cast<ro::sim::TacticTrigger>(
      std::clamp(action.trigger, 0, ro::ui::kTacticTriggerChoices - 1));
  return StoreTactic(target, line);
}

static_assert(ro::ui::kConductGenericAnyPlayer == 0 && ro::ui::kConductGenericOwnerGuild == 1 &&
              ro::ui::kConductGenericChoices == 2);

// UNE CONDUITE posée dans le profil et notée à enregistrer.
[[nodiscard]] AiEditResult StoreConduct(const AiEditTarget& target,
                                        const ro::sim::CompanionPlayerConduct& line) {
  if (!target.profile->conducts.Set(line)) {
    return Refused(ro::core::Tr("La table des conduites est pleine."));
  }
  target.edits->SetConducts(target.job, target.profile_name, target.profile->conducts);
  return Accepted(AiEditSave::kAfterCalm);
}

[[nodiscard]] AiEditResult ApplyConduct(const ro::ui::CompanionAiWindowAction& action,
                                        const AiEditTarget& target,
                                        std::span<const ro::sim::PlayerConductKey> conduct_keys) {
  using ro::sim::PlayerConductKey;
  using ro::sim::PlayerConductKeyKind;
  if (action.verb == CompanionAiVerb::kAddGenericConduct) {
    return StoreConduct(
        target,
        ro::sim::CompanionPlayerConduct{
            .key = PlayerConductKey{.kind = action.generic == ro::ui::kConductGenericOwnerGuild
                                                ? PlayerConductKeyKind::kOwnerGuild
                                                : PlayerConductKeyKind::kAnyPlayer}});
  }
  if (action.verb == CompanionAiVerb::kAddJobConduct) {
    if (action.choice < 0) {
      return AiEditResult{};
    }
    return StoreConduct(
        target, ro::sim::CompanionPlayerConduct{
                    .key = PlayerConductKey{.kind = PlayerConductKeyKind::kJob,
                                            .job = static_cast<std::uint16_t>(action.choice)}});
  }
  if (action.index >= conduct_keys.size()) {
    return AiEditResult{};
  }
  const PlayerConductKey& key = conduct_keys[action.index];
  if (action.verb == CompanionAiVerb::kRemoveConduct) {
    if (!target.profile->conducts.Remove(key)) {
      return AiEditResult{};
    }
    target.edits->SetConducts(target.job, target.profile_name, target.profile->conducts);
    return Accepted(AiEditSave::kNow);
  }
  const ro::sim::CompanionPlayerConduct* current = target.profile->conducts.Find(key);
  ro::sim::CompanionPlayerConduct line = WithConductFields(
      current != nullptr ? *current : ro::sim::CompanionPlayerConduct{.key = key}, action.advanced);
  line.rank =
      static_cast<ro::sim::TacticRank>(std::clamp(action.rank, 0, ro::ui::kTacticRankChoices - 1));
  line.trigger = static_cast<ro::sim::TacticTrigger>(
      std::clamp(action.trigger, 0, ro::ui::kTacticTriggerChoices - 1));
  return StoreConduct(target, line);
}

// LES RÈGLES ET LEURS CHAMPS INCONNUS, qui voyagent ensemble.
struct RuleList {
  std::vector<CompanionRule> rules;
  std::vector<std::string> extras;

  void Erase(std::size_t index) {
    rules.erase(rules.begin() + static_cast<std::ptrdiff_t>(index));
    extras.erase(extras.begin() + static_cast<std::ptrdiff_t>(index));
  }
  void Insert(std::size_t index, CompanionRule rule, std::string extra) {
    rules.insert(rules.begin() + static_cast<std::ptrdiff_t>(index), std::move(rule));
    extras.insert(extras.begin() + static_cast<std::ptrdiff_t>(index), std::move(extra));
  }
};

[[nodiscard]] AiEditResult ApplyRule(const ro::ui::CompanionAiWindowAction& action,
                                     const AiEditTarget& target) {
  CompanionProfile& profile = *target.profile;
  RuleList list{.rules = profile.rules, .extras = profile.rule_extras};
  list.extras.resize(list.rules.size());
  const bool room = list.rules.size() < ro::sim::kCompanionRuleMost;
  const bool aimed = action.index < list.rules.size();
  std::array<char, kLineBytes> cap{};
  std::snprintf(cap.data(), cap.size(), ro::core::Tr("%d règles au plus par profil."),
                static_cast<int>(ro::sim::kCompanionRuleMost));
  switch (action.verb) {
    case CompanionAiVerb::kEditRule: {
      if (!aimed) {
        return AiEditResult{};
      }
      // 🔴 UNE RÈGLE HORS DU CADRE NE PASSE PAS, et la fenêtre dit pourquoi :
      // elle ne propose que le vocabulaire, mais l'IA juge en dernier.
      CompanionRule edited = RuleOf(action.rule);
      if (std::string why = RuleRefusal(edited); !why.empty()) {
        return Refused(std::move(why), static_cast<int>(action.index));
      }
      list.rules[action.index] = std::move(edited);
      break;
    }
    case CompanionAiVerb::kAddRule:
      if (!room) {
        return Refused(cap.data());
      }
      list.Insert(list.rules.size(), DefaultCompanionRule(), std::string());
      break;
    case CompanionAiVerb::kRemoveRule:
      if (!aimed) {
        return AiEditResult{};
      }
      list.Erase(action.index);
      break;
    case CompanionAiVerb::kDuplicateRule:
      if (!aimed) {
        return AiEditResult{};
      }
      if (!room) {
        return Refused(cap.data(), static_cast<int>(action.index));
      }
      list.Insert(action.index + 1, list.rules[action.index], list.extras[action.index]);
      break;
    case CompanionAiVerb::kMoveRule: {
      if (!aimed || action.to >= list.rules.size() || action.to == action.index) {
        return AiEditResult{};
      }
      CompanionRule moved = list.rules[action.index];
      std::string extra = list.extras[action.index];
      list.Erase(action.index);
      list.Insert(action.to, std::move(moved), std::move(extra));
      break;
    }
    default:
      return AiEditResult{};
  }
  target.edits->SetRules(target.job, target.profile_name, list.rules, list.extras);
  profile.rules = std::move(list.rules);
  profile.rule_extras = std::move(list.extras);
  return Accepted(AiEditSave::kAfterCalm);
}

// « REPARTIR DE L'IA BASIQUE » : réglages, tactiques, conduites, règles et amis. Les
// entrées du fichier écartées à la lecture restent.
[[nodiscard]] AiEditResult ResetToBasic(const ro::ui::CompanionAiWindowAction& action,
                                        const AiEditTarget& target) {
  // 🔴 LA CONFIRMATION VISAIT CE COMPAGNON-LÀ, ou rien ne se fait.
  if (action.companion_class != target.job) {
    return Refused(ro::core::Tr("La confirmation visait un autre compagnon : rien n'a changé."));
  }
  CompanionProfile& profile = *target.profile;
  const ro::sim::CompanionAiSettings basic =
      ro::sim::BasicCompanionAiSettings(target.companion, target.job);
  for (const CompanionTactic& line : profile.tactics.lines()) {
    target.edits->Remove(target.job, target.profile_name, line.key);
  }
  profile.tactics = ro::sim::CompanionTactics();
  profile.conducts = ro::sim::CompanionPlayerConducts();
  profile.settings = basic;
  profile.rules.clear();
  profile.rule_extras.clear();
  profile.friends.clear();
  target.edits->SetConducts(target.job, target.profile_name, profile.conducts);
  target.edits->SetSettings(target.job, target.profile_name, basic);
  target.edits->SetRules(target.job, target.profile_name, {}, {});
  target.edits->SetFriends(target.job, target.profile_name, {});
  return Accepted(AiEditSave::kNow);
}

}  // namespace

AiEditResult ApplyAiEdit(const ro::ui::CompanionAiWindowAction& action, const AiEditTarget& target,
                         std::span<const ro::sim::TacticKey> tactic_keys,
                         std::span<const ro::sim::PlayerConductKey> conduct_keys) {
  if (target.profile == nullptr || target.edits == nullptr || target.job == 0) {
    return AiEditResult{};
  }
  switch (action.verb) {
    case CompanionAiVerb::kNothing:
      return AiEditResult{};
    case CompanionAiVerb::kSetBase: {
      const ro::sim::CompanionAiSettings settings =
          WithBaseFields(EffectiveSettings(target), action.base);
      target.profile->settings = settings;
      target.edits->SetSettings(target.job, target.profile_name, settings);
      return Accepted(AiEditSave::kAfterCalm);
    }
    case CompanionAiVerb::kResetToBasic:
      return ResetToBasic(action, target);
    case CompanionAiVerb::kSetTactic:
    case CompanionAiVerb::kRemoveTactic:
    case CompanionAiVerb::kAddGenericTactic:
      return ApplyTactic(action, target, tactic_keys);
    case CompanionAiVerb::kEditRule:
    case CompanionAiVerb::kAddRule:
    case CompanionAiVerb::kRemoveRule:
    case CompanionAiVerb::kDuplicateRule:
    case CompanionAiVerb::kMoveRule:
      return ApplyRule(action, target);
    case CompanionAiVerb::kAddFriend:
      return SetProfileFriend(target, action.name, true);
    case CompanionAiVerb::kRemoveFriend: {
      std::vector<std::string>& friends = target.profile->friends;
      if (action.index >= friends.size()) {
        return AiEditResult{};
      }
      friends.erase(friends.begin() + static_cast<std::ptrdiff_t>(action.index));
      target.edits->SetFriends(target.job, target.profile_name, friends);
      return Accepted(AiEditSave::kAfterCalm);
    }
    case CompanionAiVerb::kSetConduct:
    case CompanionAiVerb::kRemoveConduct:
    case CompanionAiVerb::kAddGenericConduct:
    case CompanionAiVerb::kAddJobConduct:
      return ApplyConduct(action, target, conduct_keys);
    // LES GESTES DES PROFILS ne touchent pas un profil : voir
    // `ApplyProfileGesture`.
    case CompanionAiVerb::kNewProfile:
    case CompanionAiVerb::kRenameProfile:
    case CompanionAiVerb::kDuplicateProfile:
    case CompanionAiVerb::kDeleteProfile:
    case CompanionAiVerb::kImportProfile:
    case CompanionAiVerb::kSetContextProfile:
    case CompanionAiVerb::kAnswerProposal:
      break;
  }
  return AiEditResult{};
}

AiEditResult SetProfileFriend(const AiEditTarget& target, std::string_view typed, bool befriend) {
  if (target.profile == nullptr || target.edits == nullptr || target.job == 0) {
    return AiEditResult{};
  }
  const std::string name = ro::ui::TrimmedCharacterName(typed);
  if (!ro::sim::ValidFriendName(name)) {
    return Refused(ro::core::Tr("Tapez le nom exact d'un personnage."));
  }
  std::vector<std::string>& friends = target.profile->friends;
  const auto found = std::ranges::find(friends, name);
  if (befriend == (found != friends.end())) {
    return befriend ? Refused(ro::core::Tr("Ce nom est déjà dans la liste.")) : AiEditResult{};
  }
  if (befriend) {
    if (friends.size() >= ro::sim::kCompanionFriendMost) {
      return Refused(ro::core::Tr("La liste d'amis du compagnon est pleine."));
    }
    friends.push_back(name);
  } else {
    friends.erase(found);
  }
  target.edits->SetFriends(target.job, target.profile_name, friends);
  return Accepted(AiEditSave::kAfterCalm);
}

std::string RuleRefusal(const CompanionRule& rule) {
  if (ro::sim::ValidRule(rule)) {
    return std::string();
  }
  if (rule.conditions.empty() || rule.conditions.size() > ro::sim::kRuleConditionMost) {
    return ro::core::Tr("Une règle porte d'une à trois conditions.");
  }
  if (!std::ranges::all_of(rule.conditions, &ro::sim::ValidCondition)) {
    return ro::core::Tr("Une condition sort du cadre de l'IA : la règle n'est pas changée.");
  }
  if (rule.action == ro::sim::RuleAction::kCastSkill &&
      ro::sim::NeverCastAutomatically(rule.skill_id)) {
    return ro::core::Tr(
        "Ce skill ne part jamais d'une règle : il coûte ce qui ne se rattrape pas.");
  }
  return ro::core::Tr("L'action ou sa cible sort du cadre de l'IA : la règle n'est pas changée.");
}

bool ProfileIsBasic(ro::sim::AiCompanion companion, std::uint16_t job,
                    const CompanionProfile& profile) {
  return SettingsOfProfile(companion, job, &profile) ==
         ro::sim::BasicCompanionAiSettings(companion, job);
}

bool FollowAiWindow(ro::ui::CompanionAiWindowState& window, AiWindowWatch& watch,
                    std::array<bool, 2> present_now) {
  bool save = false;
  for (std::size_t index = 0; index < present_now.size(); ++index) {
    save = save || (watch.present[index] && !present_now[index]);
  }
  if (!present_now[0] && !present_now[1]) {
    window.open = false;
  }
  if (watch.open && !window.open) {
    // FERMÉE : par sa croix, par Échap, ou faute de compagnon.
    save = true;
    ro::ui::ChangeAiWindowCompanion(window, window.companion);
  } else if (window.open && !present_now[static_cast<std::size_t>(window.companion)]) {
    ro::ui::ChangeAiWindowCompanion(
        window, present_now[0] ? ro::ui::kCompanionAiHomunculus : ro::ui::kCompanionAiMercenary);
  }
  watch.present = present_now;
  watch.open = window.open;
  return save;
}

}  // namespace moonclient
