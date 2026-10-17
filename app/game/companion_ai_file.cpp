#include "game/companion_ai_file.h"

#include <algorithm>
#include <array>
#include <fstream>
#include <optional>
#include <sstream>
#include <system_error>
#include <utility>
#include <vector>

#include <yaml-cpp/yaml.h>

#include "ro/core/error.h"
#include "ro/core/file_write.h"
#include "ro/core/log.h"
#include "ro/core/utf8_path.h"

#include "game/companion_ai_yaml.h"
#include "game/save_folder.h"

namespace moonclient {

using ro::sim::CompanionTactic;
using ro::sim::TacticKey;
using ro::sim::TacticKeyKind;

// ── LES TACTIQUES, LUES ET ÉCRITES ──────────────────────────────────────────

namespace companion_yaml {
namespace {

// LES CLÉS GÉNÉRIQUES, dans l'ordre de leurs mots.
constexpr std::array<TacticKeyKind, kGenericWords.size()> kGenericKinds = {
    TacticKeyKind::kAnyMonster, TacticKeyKind::kChest, TacticKeyKind::kStill,
    TacticKeyKind::kMinion};

// Le mot d'une clé générique.
[[nodiscard]] const char* GenericWordOf(TacticKeyKind kind) {
  for (std::size_t index = 0; index < kGenericKinds.size(); ++index) {
    if (kGenericKinds[index] == kind) {
      return kGenericWords[index].data();
    }
  }
  return kGenericWords.front().data();
}

}  // namespace

std::optional<TacticKey> ReadTacticKey(const YAML::Node& entry, std::string& why) {
  if (const YAML::Node monster = entry[kMonsterField]; monster) {
    const std::optional<std::uint16_t> job = NumberOf<std::uint16_t>(monster);
    if (!job.has_value()) {
      why = "classe de monstre illisible";
      return std::nullopt;
    }
    return TacticKey{.kind = TacticKeyKind::kMonster, .job = *job};
  }
  if (const std::optional<std::size_t> generic = IndexOf(kGenericWords, entry[kGenericField]);
      generic.has_value()) {
    return TacticKey{.kind = kGenericKinds[*generic]};
  }
  why = "ni monstre ni ligne générique connue";
  return std::nullopt;
}

std::optional<CompanionTactic> ReadTactic(const YAML::Node& entry, std::string& why) {
  CompanionTactic tactic;
  const std::optional<TacticKey> key = ReadTacticKey(entry, why);
  if (!key.has_value()) {
    return std::nullopt;
  }
  tactic.key = *key;

  FieldFault fault;
  const bool read = ReadWord(entry, kRankField, kRankWords, tactic.rank, fault) &&
                    ReadWord(entry, kTriggerField, kTriggerWords, tactic.trigger, fault) &&
                    ReadWord(entry, kAvoidField, kAvoidWords, tactic.avoid, fault) &&
                    ReadNumber(entry, kAvoidThresholdField, tactic.avoid_threshold_cells, fault) &&
                    ReadNumber(entry, kAvoidStepField, tactic.avoid_step_cells, fault) &&
                    ReadWord(entry, kSkillUseField, kSkillUseWords, tactic.skill_use, fault) &&
                    ReadNumber(entry, kSkillUseCountField, tactic.skill_use_count, fault) &&
                    ReadNumber(entry, kOpeningField, tactic.opening_skill_id, fault) &&
                    ReadNumber(entry, kHarassField, tactic.harass, fault) &&
                    ReadNumber(entry, kSpReserveField, tactic.sp_reserve_points, fault) &&
                    ReadNumber(entry, kWeightField, tactic.weight, fault) &&
                    ReadWord(entry, kShareField, kShareWords, tactic.share, fault) &&
                    ReadWord(entry, kChaseField, kChaseWords, tactic.chase, fault);
  if (!read) {
    why = Describe(fault);
    return std::nullopt;
  }
  CapReadCells(tactic.avoid_threshold_cells, ro::sim::kTacticAvoidMostCells);
  CapReadCells(tactic.avoid_step_cells, ro::sim::kTacticAvoidMostCells);
  if (!ro::sim::ValidTactic(tactic)) {
    why = "valeur hors des bornes";
    return std::nullopt;
  }
  return tactic;
}

void EmitTactic(YAML::Emitter& out, const CompanionTactic& tactic) {
  out << YAML::BeginMap;
  if (tactic.key.kind == TacticKeyKind::kMonster) {
    out << YAML::Key << kMonsterField << YAML::Value << tactic.key.job;
  } else {
    out << YAML::Key << kGenericField << YAML::Value << GenericWordOf(tactic.key.kind);
  }
  out << YAML::Key << kRankField << YAML::Value << WordOf(kRankWords, tactic.rank);
  out << YAML::Key << kTriggerField << YAML::Value << WordOf(kTriggerWords, tactic.trigger);
  out << YAML::Key << kAvoidField << YAML::Value << WordOf(kAvoidWords, tactic.avoid);
  out << YAML::Key << kAvoidThresholdField << YAML::Value << tactic.avoid_threshold_cells;
  out << YAML::Key << kAvoidStepField << YAML::Value << tactic.avoid_step_cells;
  out << YAML::Key << kSkillUseField << YAML::Value << WordOf(kSkillUseWords, tactic.skill_use);
  out << YAML::Key << kSkillUseCountField << YAML::Value << tactic.skill_use_count;
  out << YAML::Key << kOpeningField << YAML::Value << tactic.opening_skill_id;
  out << YAML::Key << kHarassField << YAML::Value << tactic.harass;
  out << YAML::Key << kSpReserveField << YAML::Value << tactic.sp_reserve_points;
  out << YAML::Key << kWeightField << YAML::Value << tactic.weight;
  out << YAML::Key << kShareField << YAML::Value << WordOf(kShareWords, tactic.share);
  out << YAML::Key << kChaseField << YAML::Value << WordOf(kChaseWords, tactic.chase);
  out << YAML::EndMap;
}

std::optional<std::string> ReadWhole(const std::filesystem::path& path) {
  std::error_code missing;
  if (!std::filesystem::exists(path, missing)) {
    return std::string();
  }
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    return std::nullopt;
  }
  std::ostringstream text;
  text << file.rdbuf();
  return text.str();
}

}  // namespace companion_yaml

namespace {

using namespace companion_yaml;

// UN OCTET DE CONTRÔLE : sous l'espace, ou DEL.
constexpr unsigned char kFirstPrintable = 0x20;
constexpr unsigned char kDelete = 0x7F;
// LES CONTRÔLES C1 EN UTF-8 : l'octet de tête, et la plage de l'octet suivant.
constexpr unsigned char kC1Lead = 0xC2;
constexpr unsigned char kC1First = 0x80;
constexpr unsigned char kC1Last = 0x9F;
// LES LIBELLÉS DES PROFILS RÉSERVÉS, en français et en espagnol, comme les
// catalogues de langue les donnent ; en anglais, celui de l'IA basique est sa
// clé gravée, et celui de « Mon profil » sa clé, qui reste permise.
constexpr std::array<std::string_view, 5> kReservedLabels = {
    kBasicProfileName, "IA basique", "IA básica", "Mon profil", "Mi perfil"};

// CE QUE LE JOURNAL DIT D'UNE RÈGLE ÉCARTÉE. ⚠ LE JOURNAL N'EST PAS TRADUIT :
// la fenêtre traduit la raison elle-même.
[[nodiscard]] std::string DescribeRejection(const RuleReadFailure& failure) {
  switch (failure.why) {
    case RuleRejection::kNotARule:
      return "règle illisible";
    case RuleRejection::kUnknownWord:
      return "valeur inconnue pour " + failure.field;
    case RuleRejection::kBadNumber:
      return "nombre illisible pour " + failure.field;
    case RuleRejection::kNoCondition:
      return "règle sans condition";
    case RuleRejection::kBadCondition:
      return "condition illisible ou hors du cadre";
    case RuleRejection::kOutOfFrame:
      return "règle hors du cadre";
    case RuleRejection::kBeyondLimit:
      return "au-delà de la dernière règle permise";
  }
  return "règle illisible";
}

void ReadLines(const YAML::Node& lines, const std::string& prefix, CompanionProfile& own,
               std::vector<std::string>& rejected) {
  if (!lines || !lines.IsSequence()) {
    return;
  }
  for (const YAML::Node& entry : lines) {
    std::string why;
    const std::optional<CompanionTactic> tactic =
        entry.IsMap() ? ReadTactic(entry, why) : std::nullopt;
    if (!tactic.has_value()) {
      rejected.push_back(prefix + (why.empty() ? std::string("ligne illisible") : why));
      continue;
    }
    if (!own.tactics.Set(*tactic)) {
      rejected.push_back(prefix + "table pleine");
    }
  }
}

// LES RÈGLES : une règle écartée garde sa place dans le fichier, et la note
// qui la dit retient laquelle.
void ReadRules(const YAML::Node& rules, const std::string& prefix, CompanionProfile& own,
               std::vector<std::string>& rejected) {
  if (!rules || !rules.IsSequence()) {
    return;
  }
  std::size_t position = 0;
  for (const YAML::Node& entry : rules) {
    RuleReadFailure failure;
    std::optional<ro::sim::CompanionRule> rule = ReadRule(entry, failure);
    if (rule.has_value() && own.rules.size() >= ro::sim::kCompanionRuleMost) {
      rule.reset();
      failure = RuleReadFailure{.why = RuleRejection::kBeyondLimit};
    }
    if (rule.has_value()) {
      own.rules.push_back(std::move(*rule));
      own.rule_extras.push_back(UnknownRuleFields(entry));
    } else {
      rejected.push_back(prefix + "règle " + std::to_string(position + 1) + " : " +
                         DescribeRejection(failure));
      own.rejected_rules.push_back(
          RejectedRule{.position = position, .why = failure.why, .field = failure.field});
    }
    ++position;
  }
}

void ReadFriends(const YAML::Node& friends, const std::string& prefix, CompanionProfile& own,
                 std::vector<std::string>& rejected) {
  if (!friends || !friends.IsSequence()) {
    return;
  }
  for (const YAML::Node& entry : friends) {
    if (!ReadableFriend(entry)) {
      rejected.push_back(prefix + "ami illisible");
    } else if (own.friends.size() >= ro::sim::kCompanionFriendMost) {
      rejected.push_back(prefix + "ami au-delà du dernier permis");
    } else {
      own.friends.push_back(entry.Scalar());
    }
  }
}

void ReadProfile(const YAML::Node& profile, std::uint16_t job, const std::string& prefix,
                 CompanionProfile& own, std::vector<std::string>& rejected) {
  ReadLines(profile[kTacticsField], prefix, own, rejected);
  if (const YAML::Node settings = profile[kSettingsField]; settings) {
    std::vector<std::string> refused;
    own.settings = companion_yaml::ReadSettings(
        settings, ro::sim::BasicCompanionAiSettings(ro::sim::AiCompanionOfJob(job), job), refused);
    for (const std::string& why : refused) {
      rejected.push_back(prefix + why);
    }
  }
  ReadConducts(profile[kPlayersField], prefix, own.conducts, rejected);
  ReadRules(profile[kRulesField], prefix, own, rejected);
  ReadFriends(profile[kFriendsField], prefix, own, rejected);
}

void ReadType(const YAML::Node& type, CompanionAiFileRead& into) {
  const std::optional<std::uint16_t> job = NumberOf<std::uint16_t>(type[kClassField]);
  if (!job.has_value() || *job == 0) {
    into.rejected.emplace_back("un type sans classe");
    return;
  }
  const YAML::Node profiles = type[kProfilesField];
  if (!profiles || !profiles.IsSequence()) {
    return;
  }
  const std::string type_prefix = "classe " + std::to_string(*job) + " : ";
  std::vector<NamedProfile> named;
  for (const YAML::Node& profile : profiles) {
    const YAML::Node name = profile.IsMap() ? profile[kNameField] : YAML::Node();
    if (!name || !name.IsScalar() || !ValidProfileName(name.Scalar())) {
      into.rejected.push_back(type_prefix + "profil sans nom recevable");
      continue;
    }
    // 🔴 UN NOM EST UNIQUE DANS SON TYPE : le premier l'emporte, le double reste
    // dans le fichier sans être relu.
    if (std::ranges::any_of(
            named, [&name](const NamedProfile& seen) { return seen.name == name.Scalar(); })) {
      into.rejected.push_back(type_prefix + "profil en double : " + name.Scalar());
      continue;
    }
    if (named.size() >= kCompanionProfileMost) {
      into.rejected.push_back(type_prefix + "profil au-delà du dernier permis : " + name.Scalar());
      continue;
    }
    NamedProfile read{.name = name.Scalar()};
    ReadProfile(profile, *job, type_prefix + "profil " + read.name + " : ", read.profile,
                into.rejected);
    named.push_back(std::move(read));
  }
  if (!named.empty()) {
    std::vector<NamedProfile>& kept = into.profiles.by_job[*job];
    for (NamedProfile& profile : named) {
      if (std::ranges::none_of(
              kept, [&profile](const NamedProfile& seen) { return seen.name == profile.name; })) {
        kept.push_back(std::move(profile));
      }
    }
  }
}

void ReadCharacters(const YAML::Node& characters, CompanionAiFileRead& into) {
  if (!characters || !characters.IsSequence()) {
    return;
  }
  for (const YAML::Node& entry : characters) {
    std::optional<CompanionCharacter> character = ReadCharacter(entry, into.rejected);
    if (!character.has_value()) {
      into.rejected.emplace_back("une fiche sans personnage");
      continue;
    }
    if (into.profiles.FindCharacter(character->key) != nullptr) {
      into.rejected.push_back("fiche en double : " + character->key.name);
      continue;
    }
    into.profiles.characters.push_back(std::move(*character));
  }
}

}  // namespace

// ── LE MODÈLE ───────────────────────────────────────────────────────────────

bool SameCharacter(const CharacterKey& stored, const CharacterKey& wanted) noexcept {
  if (wanted.cid != 0) {
    return stored.cid == wanted.cid;
  }
  return stored.cid == 0 && !wanted.name.empty() && stored.name == wanted.name;
}

const CompanionProfile* CompanionProfiles::Find(std::uint16_t job, std::string_view name) const {
  const auto type = by_job.find(job);
  if (type == by_job.end()) {
    return nullptr;
  }
  const auto found = std::ranges::find_if(
      type->second, [name](const NamedProfile& profile) { return profile.name == name; });
  return found != type->second.end() ? &found->profile : nullptr;
}

CompanionProfile* CompanionProfiles::Find(std::uint16_t job, std::string_view name) {
  return const_cast<CompanionProfile*>(std::as_const(*this).Find(job, name));
}

CompanionProfile& CompanionProfiles::At(std::uint16_t job, std::string_view name) {
  if (CompanionProfile* found = Find(job, name); found != nullptr) {
    return *found;
  }
  return by_job[job].emplace_back(NamedProfile{.name = std::string(name)}).profile;
}

const CompanionCharacter* CompanionProfiles::FindCharacter(const CharacterKey& key) const {
  const auto found = std::ranges::find_if(characters, [&key](const CompanionCharacter& character) {
    return SameCharacter(character.key, key);
  });
  return found != characters.end() ? &*found : nullptr;
}

bool HasControlCharacter(std::string_view text) noexcept {
  for (std::size_t index = 0; index < text.size(); ++index) {
    const auto byte = static_cast<unsigned char>(text[index]);
    if (byte < kFirstPrintable || byte == kDelete) {
      return true;
    }
    // C1 : U+0080 à U+009F s'écrivent 0xC2 suivi de 0x80 à 0x9F.
    if (byte == kC1Lead && index + 1 < text.size()) {
      const auto next = static_cast<unsigned char>(text[index + 1]);
      if (next >= kC1First && next <= kC1Last) {
        return true;
      }
    }
  }
  return false;
}

bool ReservedProfileLabel(std::string_view name) noexcept {
  return std::ranges::find(kReservedLabels, name) != kReservedLabels.end();
}

bool ValidProfileName(std::string_view name) noexcept {
  return !name.empty() && name.size() <= kProfileNameMostBytes && name.front() != ' ' &&
         name.back() != ' ' && !ReservedProfileLabel(name) && !HasControlCharacter(name);
}

bool RenameInCharacter(CompanionCharacter& character, std::uint16_t job, std::string_view from,
                       const std::optional<std::string>& to) {
  const auto found = character.active.find(job);
  if (found == character.active.end()) {
    return false;
  }
  bool changed = false;
  for (std::optional<std::string>& chosen : found->second) {
    if (chosen.has_value() && *chosen == from) {
      chosen = to;
      changed = true;
    }
  }
  return changed;
}

CompanionProfileEdits::ProfileEdits& CompanionProfileEdits::Of(std::uint16_t job,
                                                               std::string_view profile) {
  return by_job[job].profiles[std::string(profile)];
}

void CompanionProfileEdits::Set(std::uint16_t job, std::string_view profile,
                                const CompanionTactic& tactic) {
  Record(job, profile, tactic.key, tactic);
}

void CompanionProfileEdits::Remove(std::uint16_t job, std::string_view profile, TacticKey key) {
  Record(job, profile, key, std::nullopt);
}

void CompanionProfileEdits::Record(std::uint16_t job, std::string_view profile, TacticKey key,
                                   const std::optional<CompanionTactic>& line) {
  TacticEdits& edits = Of(job, profile).tactics;
  for (auto& [edited_key, edited_line] : edits) {
    if (edited_key == key) {
      edited_line = line;
      return;
    }
  }
  edits.emplace_back(key, line);
}

void CompanionProfileEdits::SetSettings(std::uint16_t job, std::string_view profile,
                                        const ro::sim::CompanionAiSettings& settings) {
  Of(job, profile).settings = settings;
}

void CompanionProfileEdits::SetConducts(std::uint16_t job, std::string_view profile,
                                        const ro::sim::CompanionPlayerConducts& conducts) {
  Of(job, profile).conducts = std::vector<ro::sim::CompanionPlayerConduct>(conducts.lines().begin(),
                                                                           conducts.lines().end());
}

void CompanionProfileEdits::SetRules(std::uint16_t job, std::string_view profile,
                                     const std::vector<ro::sim::CompanionRule>& rules,
                                     const std::vector<std::string>& extras) {
  ProfileEdits& edits = Of(job, profile);
  edits.rules = rules;
  edits.rule_extras = extras;
}

void CompanionProfileEdits::SetFriends(std::uint16_t job, std::string_view profile,
                                       const std::vector<std::string>& friends) {
  Of(job, profile).friends = friends;
}

void CompanionProfileEdits::SetWhole(std::uint16_t job, std::string_view profile,
                                     const CompanionProfile& whole) {
  std::vector<std::string>& created = by_job[job].created;
  if (std::ranges::find(created, profile) == created.end()) {
    created.emplace_back(profile);
  }
  ProfileEdits& edits = Of(job, profile);
  edits = ProfileEdits{.whole = true};
  for (const CompanionTactic& tactic : whole.tactics.lines()) {
    edits.tactics.emplace_back(tactic.key, tactic);
  }
  edits.settings = whole.settings;
  SetConducts(job, profile, whole.conducts);
  SetRules(job, profile, whole.rules, whole.rule_extras);
  SetFriends(job, profile, whole.friends);
}

void CompanionProfileEdits::Rename(std::uint16_t job, std::string_view from, std::string_view to) {
  TypeEdits& type = by_job[job];
  if (const auto pending = type.profiles.find(std::string(from)); pending != type.profiles.end()) {
    ProfileEdits moved = std::move(pending->second);
    type.profiles.erase(pending);
    type.profiles[std::string(to)] = std::move(moved);
  }
  std::ranges::replace(type.created, std::string(from), std::string(to));
  type.ops.push_back(ProfileOp{
      .kind = ProfileOp::Kind::kRename, .name = std::string(from), .to = std::string(to)});
  for (CompanionCharacter& character : characters) {
    RenameInCharacter(character, job, from, std::string(to));
  }
}

void CompanionProfileEdits::Delete(std::uint16_t job, std::string_view name) {
  TypeEdits& type = by_job[job];
  type.profiles.erase(std::string(name));
  std::erase(type.created, std::string(name));
  type.ops.push_back(ProfileOp{.kind = ProfileOp::Kind::kDelete, .name = std::string(name)});
  for (CompanionCharacter& character : characters) {
    RenameInCharacter(character, job, name, std::nullopt);
  }
}

void CompanionProfileEdits::SetCharacter(const CompanionCharacter& character) {
  const auto found = std::ranges::find_if(characters, [&character](const CompanionCharacter& one) {
    return SameCharacter(one.key, character.key);
  });
  if (found != characters.end()) {
    *found = character;
  } else {
    characters.push_back(character);
  }
}

const CompanionProfileEdits::ProfileEdits* CompanionProfileEdits::Find(
    std::uint16_t job, std::string_view profile) const {
  const auto type = by_job.find(job);
  if (type == by_job.end()) {
    return nullptr;
  }
  const auto found = type->second.profiles.find(std::string(profile));
  return found != type->second.profiles.end() ? &found->second : nullptr;
}

ro::sim::CompanionAiSettings SettingsOfProfile(ro::sim::AiCompanion companion, std::uint16_t job,
                                               const CompanionProfile* profile) {
  ro::sim::CompanionAiSettings settings = ro::sim::BasicCompanionAiSettings(companion, job);
  if (profile == nullptr) {
    return settings;
  }
  if (profile->settings.has_value()) {
    settings = *profile->settings;
  }
  settings.tactics = profile->tactics;
  settings.player_conducts = profile->conducts;
  settings.rules = profile->rules;
  settings.friends = profile->friends;
  return settings;
}

// ── LE FICHIER ──────────────────────────────────────────────────────────────

std::filesystem::path CompanionAiFilePath(const std::filesystem::path& client_root) {
  return client_root / kSaveFolder / kCompanionAiFileName;
}

CompanionAiFileRead ParseCompanionAi(std::string_view text) {
  CompanionAiFileRead read;
  YAML::Node whole;
  try {
    whole = YAML::Load(std::string(text));
  } catch (const YAML::Exception&) {
    read.unreadable = true;
    return read;
  }
  if (!whole || whole.IsNull()) {
    return read;
  }
  if (!whole.IsMap()) {
    read.unreadable = true;
    return read;
  }
  if (NumberOf<int>(whole[kVersionField]) != kCompanionAiFileVersion) {
    read.unknown_version = true;
    return read;
  }
  const YAML::Node types = whole[kTypesField];
  if (types && types.IsSequence()) {
    for (const YAML::Node& type : types) {
      if (type.IsMap()) {
        ReadType(type, read);
      }
    }
  }
  ReadCharacters(whole[kCharactersField], read);
  return read;
}

std::string FormatCompanionAi(const CompanionProfiles& profiles) {
  CompanionProfileEdits edits;
  for (const auto& [job, named] : profiles.by_job) {
    for (const NamedProfile& profile : named) {
      edits.SetWhole(job, profile.name, profile.profile);
    }
  }
  for (const CompanionCharacter& character : profiles.characters) {
    edits.SetCharacter(character);
  }
  // Un texte vide se fusionne toujours.
  return MergeCompanionAi("", edits).value_or(std::string());
}

CompanionAiFileRead LoadCompanionAi(const std::filesystem::path& client_root) {
  const std::filesystem::path path = CompanionAiFilePath(client_root);
  const std::optional<std::string> text = ReadWhole(path);
  if (!text.has_value()) {
    ro::core::LogWarning("IA des compagnons : {} ne s'ouvre pas", ro::core::Utf8FromPath(path));
    return CompanionAiFileRead{.unreadable = true};
  }
  CompanionAiFileRead read = ParseCompanionAi(*text);
  if (read.unreadable) {
    ro::core::LogWarning("IA des compagnons : {} illisible, il ne sera pas réécrit",
                         ro::core::Utf8FromPath(path));
  }
  if (read.unknown_version) {
    ro::core::LogWarning(
        "IA des compagnons : {} porte une version inconnue, il n'est ni relu ni réécrit",
        ro::core::Utf8FromPath(path));
  }
  for (const std::string& why : read.rejected) {
    ro::core::LogWarning(
        "IA des compagnons : écarté à la lecture, gardé tel quel dans le fichier — {}", why);
  }
  return read;
}

std::optional<CompanionAiFileRead> SaveCompanionAiEdits(const std::filesystem::path& client_root,
                                                        const CompanionProfileEdits& edits) {
  const std::filesystem::path path = CompanionAiFilePath(client_root);
  // ⚠ RELU JUSTE AVANT D'ÉCRIRE : un autre client sur la même installation a pu
  // l'enregistrer entre-temps, et ses lignes survivent à la fusion.
  const std::optional<std::string> current = ReadWhole(path);
  std::vector<CompanionRenameShift> shifts;
  const std::optional<std::string> merged =
      current.has_value() ? MergeCompanionAi(*current, edits, &shifts) : std::nullopt;
  if (!merged.has_value()) {
    ro::core::LogWarning(
        "IA des compagnons : {} n'est pas réécrit (illisible, ou d'une version inconnue) ; les "
        "modifications ne valent que pour cette session",
        ro::core::Utf8FromPath(path));
    return std::nullopt;
  }
  if (const ro::core::Result<void> written = ro::core::WriteFileAtomically(path, *merged);
      !written) {
    ro::core::LogWarning("IA des compagnons : écriture impossible ({})",
                         ro::core::Describe(written.error()));
    return std::nullopt;
  }
  for (const CompanionRenameShift& shift : shifts) {
    ro::core::LogWarning(
        "IA des compagnons : un autre client a écrit un profil « {} » (classe {}) ; le profil "
        "renommé est enregistré sous « {} »",
        shift.wanted, shift.job, shift.written);
  }
  return ParseCompanionAi(*merged);
}

}  // namespace moonclient
