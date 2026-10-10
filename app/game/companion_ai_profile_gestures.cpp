// Voir l'en-tête.

#include "game/companion_ai_profile_gestures.h"

#include <algorithm>
#include <string>
#include <utility>

#include "ro/core/i18n.h"
#include "ro/sim/companion_players.h"

#include "game/companion_ai_names.h"
#include "game/companion_ai_profiles.h"

namespace moonclient {
namespace {

using ro::ui::CompanionAiVerb;

[[nodiscard]] ProfileGestureResult Outcome(ProfileRefusal refusal,
                                           std::optional<std::string> edited = std::nullopt) {
  if (refusal != ProfileRefusal::kNone) {
    return ProfileGestureResult{.handled = true, .refusal = ProfileRefusalText(refusal)};
  }
  return ProfileGestureResult{.handled = true, .accepted = true, .edited = std::move(edited)};
}

}  // namespace

std::vector<ProfileSource> ImportSources(const CompanionProfiles& profiles,
                                         const CharacterKey& self, std::uint16_t job) {
  std::vector<ProfileSource> sources;
  const auto add = [&sources](std::uint16_t from_job, const std::string& name, std::string label) {
    if (std::ranges::none_of(sources, [from_job, &name](const ProfileSource& one) {
          return one.job == from_job && one.name == name;
        })) {
      sources.push_back(ProfileSource{.job = from_job, .name = name, .label = std::move(label)});
    }
  };
  // D'ABORD LE PROFIL ACTIF DE CHAQUE AUTRE PERSONNAGE, type par type, sous son
  // nom : c'est ce qui le distingue d'un profil de ce type pris au hasard. Ceux
  // de la même classe sont déjà les nôtres.
  for (const CompanionCharacter& character : profiles.characters) {
    if (SameCharacter(character.key, self)) {
      continue;
    }
    for (const auto& [from_job, chosen] : character.active) {
      if (from_job == job) {
        continue;
      }
      for (std::size_t context = 0; context < chosen.size(); ++context) {
        const std::string name = ActiveProfileOf(profiles, character.key, from_job,
                                                 static_cast<ro::sim::CompanionAiContext>(context));
        if (name != kBasicProfileName) {
          add(from_job, name,
              character.key.name + " · " + CompanionClassName(from_job) + " — " +
                  ProfileLabelOf(name));
        }
      }
    }
  }
  // PUIS LES PROFILS DES AUTRES TYPES.
  for (const auto& [from_job, named] : profiles.by_job) {
    if (from_job == job) {
      continue;
    }
    for (const NamedProfile& profile : named) {
      add(from_job, profile.name,
          CompanionClassName(from_job) + " — " + ProfileLabelOf(profile.name));
    }
  }
  return sources;
}

ProfileGestureResult ApplyProfileGesture(const ro::ui::CompanionAiWindowAction& action,
                                         CompanionAiSession& session, const SeatedCompanion& seated,
                                         std::span<const std::string> profile_keys,
                                         std::span<const ProfileSource> imports) {
  if (!seated.seat->applied.has_value()) {
    return ProfileGestureResult{.handled = action.verb >= CompanionAiVerb::kNewProfile &&
                                           action.verb <= CompanionAiVerb::kAnswerProposal};
  }
  const std::uint16_t job = seated.seat->applied->first;
  CompanionProfileBook book(session.profiles(), session.edits());
  const std::string named = TrimmedProfileName(action.name);
  switch (action.verb) {
    case CompanionAiVerb::kNewProfile:
      return Outcome(book.Create(job, named, kBasicProfileName), named);
    case CompanionAiVerb::kDuplicateProfile:
      return Outcome(book.Create(job, named, action.profile), named);
    case CompanionAiVerb::kRenameProfile:
      return Outcome(session.Rename({&seated, 1}, job, action.profile, named), named);
    case CompanionAiVerb::kDeleteProfile:
      return Outcome(session.Delete(job, action.profile), std::string());
    case CompanionAiVerb::kImportProfile: {
      if (action.choice < 0 || static_cast<std::size_t>(action.choice) >= imports.size()) {
        return ProfileGestureResult{.handled = true};
      }
      const ProfileSource& source = imports[static_cast<std::size_t>(action.choice)];
      return Outcome(book.Import(job, named, source.job, source.name), named);
    }
    case CompanionAiVerb::kSetContextProfile: {
      // Sans personnage connu, aucune fiche ne s'écrit.
      if (!session.character_known() || action.context < 0 ||
          static_cast<std::size_t>(action.context) >= ro::sim::kCompanionAiContextCount) {
        return ProfileGestureResult{.handled = true};
      }
      const auto context = static_cast<ro::sim::CompanionAiContext>(action.context);
      if (action.choice < 0) {
        return Outcome(book.ClearActive(session.character(), job, context));
      }
      if (static_cast<std::size_t>(action.choice) >= profile_keys.size()) {
        return ProfileGestureResult{.handled = true};
      }
      return Outcome(book.SetActive(session.character(), job, context,
                                    profile_keys[static_cast<std::size_t>(action.choice)]));
    }
    case CompanionAiVerb::kAnswerProposal:
      session.Answer(seated, action.accept);
      return ProfileGestureResult{.handled = true, .accepted = true};
    default:
      break;
  }
  return ProfileGestureResult{};
}

}  // namespace moonclient
