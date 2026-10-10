// LES OPÉRATIONS SUR LES PROFILS NOMMÉS ET LES FICHES. Voir
// `game/companion_ai_profiles.h`.

#include "game/companion_ai_profiles.h"

#include <algorithm>
#include <map>
#include <string>
#include <utility>

namespace moonclient {
namespace {

using ro::sim::CompanionAiContext;

// UN OCTET DE SUITE d'un caractère UTF-8 : 10xxxxxx.
constexpr unsigned char kContinuationMask = 0xC0;
constexpr unsigned char kContinuationBits = 0x80;
// LE PREMIER SUFFIXE d'un nom pris : « (2) ».
constexpr int kFirstSuffix = 2;

[[nodiscard]] std::size_t GenreOf(std::uint16_t job) noexcept {
  return static_cast<std::size_t>(ro::sim::AiCompanionOfJob(job));
}

// UNE CLÉ DE PERSONNAGE QUI DÉSIGNE QUELQU'UN : un cid, ou à défaut un nom.
[[nodiscard]] bool KeyUsable(const CharacterKey& key) noexcept {
  return key.cid != 0 || !key.name.empty();
}

[[nodiscard]] std::size_t IndexOf(CompanionAiContext context) noexcept {
  return static_cast<std::size_t>(context);
}

// `text` raccourci à `most` octets au plus, sans couper un caractère UTF-8, et
// sans espace au bout.
[[nodiscard]] std::string CutTo(std::string text, std::size_t most) {
  if (text.size() > most) {
    std::size_t end = most;
    while (end > 0 &&
           (static_cast<unsigned char>(text[end]) & kContinuationMask) == kContinuationBits) {
      --end;
    }
    text.resize(end);
  }
  while (!text.empty() && text.back() == ' ') {
    text.pop_back();
  }
  return text;
}

}  // namespace

std::string TrimmedProfileName(std::string_view typed) {
  const std::size_t first = typed.find_first_not_of(' ');
  if (first == std::string_view::npos) {
    return std::string();
  }
  const std::size_t last = typed.find_last_not_of(' ');
  return std::string(typed.substr(first, last - first + 1));
}

ProfileRefusal NameRefusal(const CompanionProfiles& profiles, std::uint16_t job,
                           std::string_view name, std::string_view renamed) {
  if (name.empty()) {
    return ProfileRefusal::kEmptyName;
  }
  if (name.size() > kProfileNameMostBytes) {
    return ProfileRefusal::kNameTooLong;
  }
  if (name != TrimmedProfileName(name) || HasControlCharacter(name)) {
    return ProfileRefusal::kBadCharacter;
  }
  if (ReservedProfileLabel(name)) {
    return ProfileRefusal::kReservedName;
  }
  if (name != renamed && profiles.Find(job, name) != nullptr) {
    return ProfileRefusal::kNameTaken;
  }
  return ProfileRefusal::kNone;
}

std::string FreeProfileName(const CompanionProfiles& profiles, std::uint16_t job,
                            std::string_view wanted) {
  const std::string base = CutTo(TrimmedProfileName(wanted), kProfileNameMostBytes);
  if (NameRefusal(profiles, job, base) == ProfileRefusal::kNone) {
    return base;
  }
  // Un type tient au plus `kCompanionProfileMost` profils : un suffixe de plus
  // trouve toujours une place.
  const int last_suffix = kFirstSuffix + static_cast<int>(kCompanionProfileMost);
  for (int suffix = kFirstSuffix; suffix <= last_suffix; ++suffix) {
    const std::string tail = " (" + std::to_string(suffix) + ")";
    const std::string name = CutTo(base, kProfileNameMostBytes - tail.size()) + tail;
    if (NameRefusal(profiles, job, name) == ProfileRefusal::kNone) {
      return name;
    }
  }
  return base;
}

std::vector<std::size_t> RulesMissingSkill(std::span<const ro::sim::CompanionRule> rules,
                                           std::span<const std::uint16_t> owned_skill_ids) {
  std::vector<std::size_t> missing;
  for (std::size_t index = 0; index < rules.size(); ++index) {
    const ro::sim::CompanionRule& rule = rules[index];
    if (rule.action == ro::sim::RuleAction::kCastSkill &&
        std::ranges::find(owned_skill_ids, rule.skill_id) == owned_skill_ids.end()) {
      missing.push_back(index);
    }
  }
  return missing;
}

bool CompanionProfileBook::HasProfiles(std::uint16_t job) const {
  const auto found = profiles_.by_job.find(job);
  return found != profiles_.by_job.end() && !found->second.empty();
}

void CompanionProfileBook::Store(std::uint16_t job, std::string_view name,
                                 CompanionProfile profile) {
  // Les entrées écartées d'un autre profil sont des places dans SON fichier.
  profile.rejected_rules.clear();
  edits_.SetWhole(job, name, profile);
  profiles_.by_job[job].push_back(
      NamedProfile{.name = std::string(name), .profile = std::move(profile)});
}

ProfileRefusal CompanionProfileBook::Create(std::uint16_t job, std::string_view name,
                                            std::string_view source) {
  const std::string kept = TrimmedProfileName(name);
  if (const ProfileRefusal refusal = NameRefusal(profiles_, job, kept);
      refusal != ProfileRefusal::kNone) {
    return refusal;
  }
  const auto type = profiles_.by_job.find(job);
  if (type != profiles_.by_job.end() && type->second.size() >= kCompanionProfileMost) {
    return ProfileRefusal::kTypeFull;
  }
  CompanionProfile copy;
  if (source != kBasicProfileName) {
    const CompanionProfile* found = profiles_.Find(job, source);
    if (found == nullptr) {
      return ProfileRefusal::kUnknownProfile;
    }
    copy = *found;
  }
  Store(job, kept, std::move(copy));
  return ProfileRefusal::kNone;
}

ProfileRefusal CompanionProfileBook::Rename(std::uint16_t job, std::string_view from,
                                            std::string_view to) {
  if (from == kBasicProfileName) {
    return ProfileRefusal::kBasicLocked;
  }
  const auto type = profiles_.by_job.find(job);
  if (type == profiles_.by_job.end()) {
    return ProfileRefusal::kUnknownProfile;
  }
  const auto found = std::ranges::find_if(
      type->second, [from](const NamedProfile& one) { return one.name == from; });
  if (found == type->second.end()) {
    return ProfileRefusal::kUnknownProfile;
  }
  const std::string kept = TrimmedProfileName(to);
  if (const ProfileRefusal refusal = NameRefusal(profiles_, job, kept, from);
      refusal != ProfileRefusal::kNone) {
    return refusal;
  }
  if (kept == from) {
    return ProfileRefusal::kNone;
  }
  const std::string old_name = found->name;
  found->name = kept;
  for (CompanionCharacter& character : profiles_.characters) {
    RenameInCharacter(character, job, old_name, kept);
  }
  edits_.Rename(job, old_name, kept);
  // 🔴 « MON PROFIL » EST LE REPLI D'UN PVE SANS CHOIX : renommé, il ne le
  // serait plus, et ces personnages retomberaient sur l'IA basique. Ils
  // reçoivent son nouveau nom en choix explicite.
  if (old_name == kOwnProfileName) {
    const std::size_t pve = IndexOf(CompanionAiContext::kPvE);
    for (CompanionCharacter& character : profiles_.characters) {
      std::optional<std::string>& chosen = character.active[job][pve];
      if (!chosen.has_value()) {
        chosen = kept;
        edits_.SetCharacter(character);
      }
    }
  }
  return ProfileRefusal::kNone;
}

void CompanionProfileBook::Know(const CharacterKey& key) {
  if (!KeyUsable(key)) {
    return;
  }
  const bool known = profiles_.FindCharacter(key) != nullptr;
  CompanionCharacter& character = CharacterOf(key);
  if (!known) {
    edits_.SetCharacter(character);
  }
}

bool CompanionProfileBook::MarkTold(const CharacterKey& key, const EvolutionProposal& proposal) {
  if (!KeyUsable(key)) {
    return false;
  }
  CompanionCharacter& character = CharacterOf(key);
  const std::pair pair(proposal.from_job, proposal.to_job);
  if (std::ranges::find(character.told, pair) != character.told.end()) {
    return false;
  }
  character.told.push_back(pair);
  edits_.SetCharacter(character);
  return true;
}

ProfileRefusal CompanionProfileBook::Delete(std::uint16_t job, std::string_view name) {
  if (name == kBasicProfileName) {
    return ProfileRefusal::kBasicLocked;
  }
  const auto type = profiles_.by_job.find(job);
  if (type == profiles_.by_job.end() ||
      std::erase_if(type->second, [name](const NamedProfile& one) { return one.name == name; }) ==
          0) {
    return ProfileRefusal::kUnknownProfile;
  }
  if (type->second.empty()) {
    profiles_.by_job.erase(type);
  }
  for (CompanionCharacter& character : profiles_.characters) {
    RenameInCharacter(character, job, name, std::nullopt);
  }
  edits_.Delete(job, name);
  return ProfileRefusal::kNone;
}

ProfileRefusal CompanionProfileBook::Import(std::uint16_t to_job, std::string_view name,
                                            std::uint16_t from_job, std::string_view from_name) {
  const std::string kept = TrimmedProfileName(name);
  if (const ProfileRefusal refusal = NameRefusal(profiles_, to_job, kept);
      refusal != ProfileRefusal::kNone) {
    return refusal;
  }
  const auto type = profiles_.by_job.find(to_job);
  if (type != profiles_.by_job.end() && type->second.size() >= kCompanionProfileMost) {
    return ProfileRefusal::kTypeFull;
  }
  CompanionProfile copy;
  if (from_name != kBasicProfileName) {
    const CompanionProfile* found = profiles_.Find(from_job, from_name);
    if (found == nullptr) {
      return ProfileRefusal::kUnknownProfile;
    }
    copy = *found;
  }
  // 🔴 DES RÉGLAGES ABSENTS VALENT L'IA BASIQUE DE LEUR TYPE : d'un autre type,
  // ils se recopient tels que l'usine les donne à son espèce, et non à celle
  // de la destination.
  if (!copy.settings.has_value() && from_job != to_job) {
    copy.settings =
        ro::sim::BasicCompanionAiSettings(ro::sim::AiCompanionOfJob(from_job), from_job);
  }
  Store(to_job, kept, std::move(copy));
  return ProfileRefusal::kNone;
}

std::string ActiveProfileOf(const CompanionProfiles& profiles, const CharacterKey& key,
                            std::uint16_t job, CompanionAiContext context) {
  const auto usable = [&profiles, job](const std::optional<std::string>& name) {
    return name.has_value() && (*name == kBasicProfileName || profiles.Find(job, *name) != nullptr);
  };
  if (const CompanionCharacter* character = profiles.FindCharacter(key); character != nullptr) {
    if (const auto chosen = character->active.find(job); chosen != character->active.end()) {
      if (const std::optional<std::string>& own = chosen->second[IndexOf(context)]; usable(own)) {
        return *own;
      }
      // PVP OU WOE SANS CHOIX : celui du PvE.
      if (const std::optional<std::string>& pve = chosen->second[IndexOf(CompanionAiContext::kPvE)];
          usable(pve)) {
        return *pve;
      }
    }
  }
  // PVE SANS CHOIX : « Mon profil » s'il existe, sinon l'IA basique.
  return profiles.Find(job, kOwnProfileName) != nullptr ? std::string(kOwnProfileName)
                                                        : std::string(kBasicProfileName);
}

std::string CompanionProfileBook::Active(const CharacterKey& key, std::uint16_t job,
                                         CompanionAiContext context) const {
  return ActiveProfileOf(profiles_, key, job, context);
}

ProfileRefusal CompanionProfileBook::ClearActive(const CharacterKey& key, std::uint16_t job,
                                                 CompanionAiContext context) {
  if (!KeyUsable(key)) {
    return ProfileRefusal::kUnknownProfile;
  }
  CompanionCharacter& character = CharacterOf(key);
  character.active[job][IndexOf(context)].reset();
  edits_.SetCharacter(character);
  return ProfileRefusal::kNone;
}

ProfileRefusal CompanionProfileBook::SetActive(const CharacterKey& key, std::uint16_t job,
                                               CompanionAiContext context, std::string_view name) {
  if (!KeyUsable(key) || (name != kBasicProfileName && profiles_.Find(job, name) == nullptr)) {
    return ProfileRefusal::kUnknownProfile;
  }
  CompanionCharacter& character = CharacterOf(key);
  character.active[job][IndexOf(context)] = std::string(name);
  edits_.SetCharacter(character);
  return ProfileRefusal::kNone;
}

CompanionCharacter& CompanionProfileBook::CharacterOf(const CharacterKey& key) {
  const auto found = std::ranges::find_if(
      profiles_.characters,
      [&key](const CompanionCharacter& character) { return SameCharacter(character.key, key); });
  if (found == profiles_.characters.end()) {
    return profiles_.characters.emplace_back(CompanionCharacter{.key = key});
  }
  if (!key.name.empty() && found->key.name != key.name) {
    found->key.name = key.name;
    edits_.SetCharacter(*found);
  }
  return *found;
}

std::optional<EvolutionProposal> CompanionProfileBook::NoteForm(const CharacterKey& key,
                                                                std::uint16_t job) {
  if (job == 0) {
    return std::nullopt;
  }
  CompanionCharacter& character = CharacterOf(key);
  std::uint16_t& previous = character.previous_form[GenreOf(job)];
  if (previous == job) {
    return std::nullopt;
  }
  const EvolutionProposal proposal{.from_job = previous, .to_job = job};
  const std::pair pair(proposal.from_job, proposal.to_job);
  // 🔴 POSÉE, ELLE RESTE jusqu'à la réponse : un geste qui crée entre-temps un
  // profil pour la nouvelle classe ne la fait pas tomber.
  if (std::ranges::find(character.proposed, pair) != character.proposed.end()) {
    return proposal;
  }
  const bool declined = std::ranges::find(character.declined, pair) != character.declined.end();
  const auto active = character.active.find(job);
  const bool chosen =
      active != character.active.end() &&
      std::ranges::any_of(active->second,
                          [](const std::optional<std::string>& one) { return one.has_value(); });
  if (previous != 0 && !declined && !chosen && !HasProfiles(job) && HasProfiles(previous)) {
    character.proposed.push_back(pair);
    edits_.SetCharacter(character);
    return proposal;
  }
  previous = job;
  edits_.SetCharacter(character);
  return std::nullopt;
}

void CompanionProfileBook::Answer(const CharacterKey& key, const EvolutionProposal& proposal,
                                  bool accept) {
  const std::pair pair(proposal.from_job, proposal.to_job);
  if (accept) {
    // LE PROFIL ACTIF DE CHAQUE CONTEXTE de l'ancienne forme, replis compris :
    // la nouvelle forme se conduit comme elle.
    ContextProfiles resolved;
    for (std::size_t context = 0; context < resolved.size(); ++context) {
      resolved[context] = Active(key, proposal.from_job, static_cast<CompanionAiContext>(context));
    }
    // Un nom déjà pris dans la nouvelle classe — un profil créé pendant que la
    // proposition attendait — fait prendre à la copie un nom libre.
    const auto source = profiles_.by_job.find(proposal.from_job);
    const std::vector<NamedProfile> copied =
        source != profiles_.by_job.end() ? source->second : std::vector<NamedProfile>();
    std::map<std::string, std::string> renamed;
    for (const NamedProfile& named : copied) {
      const auto type = profiles_.by_job.find(proposal.to_job);
      if (type != profiles_.by_job.end() && type->second.size() >= kCompanionProfileMost) {
        break;
      }
      const std::string name = FreeProfileName(profiles_, proposal.to_job, named.name);
      Store(proposal.to_job, name, named.profile);
      renamed[named.name] = name;
    }
    CompanionCharacter& character = CharacterOf(key);
    for (std::size_t context = 0; context < resolved.size(); ++context) {
      std::optional<std::string>& chosen = character.active[proposal.to_job][context];
      if (*resolved[context] == kBasicProfileName) {
        chosen = resolved[context];
      } else if (const auto copy = renamed.find(*resolved[context]); copy != renamed.end()) {
        chosen = copy->second;
      }
    }
  }
  CompanionCharacter& character = CharacterOf(key);
  std::erase(character.proposed, pair);
  std::erase(character.told, pair);
  if (!accept && std::ranges::find(character.declined, pair) == character.declined.end()) {
    character.declined.push_back(pair);
  }
  character.previous_form[GenreOf(proposal.to_job)] = proposal.to_job;
  edits_.SetCharacter(character);
}

}  // namespace moonclient
