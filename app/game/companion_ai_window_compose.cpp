// Voir l'en-tête.

#include "game/companion_ai_window_compose.h"

#include <algorithm>
#include <limits>
#include <utility>

#include "ro/core/i18n.h"

#include "game/companion_ai_names.h"
#include "game/companion_ai_profiles.h"

namespace moonclient {
namespace {

using ro::sim::CompanionAiContext;
using ro::sim::CompanionPlayerConduct;
using ro::sim::PlayerConductKey;
using ro::sim::PlayerConductKeyKind;
using ro::ui::CompanionAiVerb;

// LE MOTIF TRADUIT, son « %s » remplacé par `value`. Chaque motif de ce fichier
// n'en porte qu'un, dans toutes les langues.
[[nodiscard]] std::string Formatted(std::string_view pattern, std::string_view value) {
  std::string text(pattern);
  if (const std::size_t at = text.find("%s"); at != std::string::npos) {
    text.replace(at, 2, value);
  }
  return text;
}

[[nodiscard]] PlayerConductKey PlayerKey(const std::string& name) {
  return PlayerConductKey{.kind = PlayerConductKeyKind::kPlayer, .name = name};
}

}  // namespace

void ComposeProfilesView(CompanionAiSession& session, std::uint16_t job, std::string_view edited,
                         std::string_view proposal, ProfilesViewStore& store,
                         ro::ui::CompanionProfilesView& into) {
  store.choices.clear();
  store.keys.clear();
  const auto add = [&store](const std::string& key) {
    store.choices.push_back(ro::ui::ProfileChoiceView{.key = key, .label = ProfileLabelOf(key)});
    store.keys.push_back(key);
  };
  add(std::string(kBasicProfileName));
  std::size_t named = 0;
  if (const auto type = session.profiles().by_job.find(job);
      type != session.profiles().by_job.end()) {
    for (const NamedProfile& profile : type->second) {
      add(profile.name);
    }
    named = type->second.size();
  }
  const auto rank_of = [&store](std::string_view key) {
    const auto found = std::ranges::find(store.keys, key);
    return found != store.keys.end() ? static_cast<int>(found - store.keys.begin()) : -1;
  };
  into.profiles = store.choices;
  into.edited = std::max(rank_of(edited), 0);
  into.full = named >= kCompanionProfileMost;
  into.current_context = static_cast<int>(session.context());
  // LE CHOIX DE CHAQUE CONTEXTE, ce que son repli prend, et si le profil
  // édité agit quelque part.
  ContextProfiles chosen;
  if (const CompanionCharacter* character = session.profiles().FindCharacter(session.character());
      character != nullptr) {
    if (const auto found = character->active.find(job); found != character->active.end()) {
      chosen = found->second;
    }
  }
  CompanionProfileBook book(session.profiles(), session.edits());
  const std::string pve = book.Active(session.character(), job, CompanionAiContext::kPvE);
  into.unused = true;
  for (std::size_t context = 0; context < ro::ui::kCompanionAiContexts; ++context) {
    into.chosen[context] = chosen[context].has_value() ? rank_of(*chosen[context]) : -1;
    if (context == static_cast<std::size_t>(CompanionAiContext::kPvE)) {
      const std::string fallback = session.profiles().Find(job, kOwnProfileName) != nullptr
                                       ? std::string(kOwnProfileName)
                                       : std::string(kBasicProfileName);
      into.fallback[context] = Formatted(ro::core::Tr("Par défaut (%s)"), ProfileLabelOf(fallback));
    } else {
      into.fallback[context] = Formatted(ro::core::Tr("Comme en PvE (%s)"), ProfileLabelOf(pve));
    }
    if (book.Active(session.character(), job, static_cast<CompanionAiContext>(context)) == edited) {
      into.unused = false;
    }
  }
  // CE QUE « IMPORTER… » PROPOSE, refait quand le type change ou que la liste
  // est périmée.
  if (store.imports_stale || store.imports_job != job) {
    store.imports = ImportSources(session.profiles(), session.character(), job);
    store.import_labels.clear();
    for (const ProfileSource& source : store.imports) {
      store.import_labels.push_back(source.label);
    }
    store.imports_job = job;
    store.imports_stale = false;
  }
  into.imports = store.import_labels;
  into.proposal = std::string(proposal);
}

std::vector<ro::ui::RuleChoice> PlayerJobChoices(
    const std::unordered_map<std::uint32_t, std::string>& job_names) {
  std::vector<ro::ui::RuleChoice> choices;
  for (const auto& [job_id, name] : job_names) {
    if (name.empty() || job_id > std::numeric_limits<std::uint16_t>::max() ||
        !ro::sim::IsConductJob(static_cast<std::uint16_t>(job_id))) {
      continue;
    }
    choices.push_back(ro::ui::RuleChoice{.value = static_cast<int>(job_id), .label = name});
  }
  // UN SEUL RANG PAR LIBELLÉ, le plus petit : la table nomme parfois deux
  // classes pareil.
  std::ranges::sort(choices, [](const ro::ui::RuleChoice& left, const ro::ui::RuleChoice& right) {
    return left.label != right.label ? left.label < right.label : left.value < right.value;
  });
  const auto [first, last] = std::ranges::unique(
      choices, [](const ro::ui::RuleChoice& left, const ro::ui::RuleChoice& right) {
        return left.label == right.label;
      });
  choices.erase(first, last);
  return choices;
}

std::string ProposalLine(const EvolutionProposal& proposal) {
  return Formatted(ro::core::Tr("Recopier le profil de %s ?"),
                   CompanionClassName(proposal.from_job));
}

std::string ProposalChatLine(const EvolutionProposal& proposal) {
  return Formatted(ro::core::Tr("Votre compagnon a changé de forme : la fenêtre « IA du "
                                "compagnon » propose de recopier le profil de %s."),
                   CompanionClassName(proposal.from_job));
}

AiEditResult SetPlayerConduct(CompanionAiSession& session, const SeatedCompanion& seated,
                              const std::string& name, std::optional<ro::sim::TacticRank> rank,
                              std::optional<ro::sim::TacticTrigger> trigger) {
  if (name.empty() || (!rank.has_value() && !trigger.has_value())) {
    return AiEditResult{};
  }
  const AiEditTarget target = session.EditTarget(seated);
  if (target.profile == nullptr) {
    return AiEditResult{};
  }
  const PlayerConductKey key = PlayerKey(name);
  const CompanionPlayerConduct* current = target.profile->conducts.Find(key);
  CompanionPlayerConduct line = current != nullptr ? *current : CompanionPlayerConduct{.key = key};
  if (rank.has_value()) {
    line.rank = *rank;
  }
  if (trigger.has_value()) {
    line.trigger = *trigger;
  }
  if (!target.profile->conducts.Set(line)) {
    return AiEditResult{.refusal = ro::core::Tr("La table des conduites est pleine.")};
  }
  target.edits->SetConducts(target.job, target.profile_name, target.profile->conducts);
  session.Refresh(seated);
  return AiEditResult{.accepted = true, .save = AiEditSave::kNow};
}

AiEditResult RemovePlayerConduct(CompanionAiSession& session, const SeatedCompanion& seated,
                                 const std::string& name) {
  if (!seated.seat->applied.has_value()) {
    return AiEditResult{};
  }
  const auto& [job, profile_name] = *seated.seat->applied;
  CompanionProfile* profile = session.profiles().Find(job, profile_name);
  if (profile == nullptr || !profile->conducts.Remove(PlayerKey(name))) {
    return AiEditResult{};
  }
  session.edits().SetConducts(job, profile_name, profile->conducts);
  session.Refresh(seated);
  return AiEditResult{.accepted = true, .save = AiEditSave::kNow};
}

AiWindowOutcome ApplyAiWindowGesture(const ro::ui::CompanionAiWindowAction& action,
                                     CompanionAiSession& session, const SeatedCompanion& seated,
                                     std::span<const std::string> profile_keys,
                                     std::span<const ProfileSource> imports,
                                     std::span<const ro::sim::TacticKey> tactic_keys,
                                     std::span<const ro::sim::PlayerConductKey> conduct_keys) {
  AiWindowOutcome outcome;
  outcome.gesture = ApplyProfileGesture(action, session, seated, profile_keys, imports);
  if (outcome.gesture.handled) {
    return outcome;
  }
  // LES AUTRES GESTES VISENT LE PROFIL QUE LA VUE ÉDITAIT.
  const AiEditTarget target = session.NamedTarget(seated, action.profile);
  outcome.edit = ApplyAiEdit(action, target, tactic_keys, conduct_keys);
  if (target.profile != nullptr) {
    outcome.rule_count = target.profile->rules.size();
  }
  return outcome;
}

}  // namespace moonclient
