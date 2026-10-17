// Voir l'en-tête.

#include "game/companion_ai_state_file.h"

#include <algorithm>
#include <chrono>
#include <optional>
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
namespace {

using namespace companion_yaml;
using ro::sim::CompanionRuleMemory;
using ro::sim::CompanionSkillMemory;

// ⚠ GRAVÉS, comme tous les champs de savedata.
constexpr const char* kWrittenField = "written_at_ms";
constexpr const char* kStandbyField = "standby";
constexpr const char* kSkillsField = "skills";
constexpr const char* kSkillField = "skill";
constexpr const char* kCastAgoField = "cast_ago_ms";
constexpr const char* kSuspendedField = "suspended_for_ms";
constexpr const char* kIndexField = "index";
constexpr const char* kActedAgoField = "acted_ago_ms";
constexpr const char* kRuleField = "rule";

constexpr const char* kFileHeader =
    "# Ce que l'IA des compagnons de moonclient garde d'une session a l'autre :\n"
    "# les delais des regles, les traces des skills, les suspensions et\n"
    "# l'attente. Reecrit par le client ; rien n'y est a regler.\n";

[[nodiscard]] std::optional<std::uint32_t> Duration(const YAML::Node& map, const char* field) {
  return NumberOf<std::uint32_t>(map[field]);
}

void EmitDuration(YAML::Emitter& out, const char* field,
                  const std::optional<std::uint32_t>& duration_ms) {
  if (duration_ms.has_value()) {
    out << YAML::Key << field << YAML::Value << *duration_ms;
  }
}

[[nodiscard]] std::optional<StoredCompanionState> ReadCompanion(const YAML::Node& entry) {
  const std::optional<std::uint16_t> job =
      entry.IsMap() ? NumberOf<std::uint16_t>(entry[kClassField]) : std::nullopt;
  const std::optional<std::int64_t> written =
      entry.IsMap() ? NumberOf<std::int64_t>(entry[kWrittenField]) : std::nullopt;
  if (!job.has_value() || *job == 0 || !written.has_value()) {
    return std::nullopt;
  }
  StoredCompanionState state{.job = *job, .written_wall_ms = *written};
  state.standby = NumberOf<bool>(entry[kStandbyField]).value_or(false);
  if (const YAML::Node skills = entry[kSkillsField]; skills && skills.IsSequence()) {
    for (const YAML::Node& skill : skills) {
      const std::optional<std::uint16_t> skill_id =
          skill.IsMap() ? NumberOf<std::uint16_t>(skill[kSkillField]) : std::nullopt;
      if (skill_id.has_value()) {
        state.skills.push_back(
            CompanionSkillMemory{.skill_id = *skill_id,
                                 .cast_ago_ms = Duration(skill, kCastAgoField),
                                 .suspended_for_ms = Duration(skill, kSuspendedField)});
      }
    }
  }
  if (const YAML::Node profiles = entry[kProfilesField]; profiles && profiles.IsSequence()) {
    for (const YAML::Node& profile : profiles) {
      const YAML::Node name = profile.IsMap() ? profile[kNameField] : YAML::Node();
      const YAML::Node rules = profile.IsMap() ? profile[kRulesField] : YAML::Node();
      if (!name || !name.IsScalar() || !rules || !rules.IsSequence()) {
        continue;
      }
      std::vector<CompanionRuleMemory>& kept = state.rules_by_profile[name.Scalar()];
      for (const YAML::Node& rule : rules) {
        RuleReadFailure failure;
        const std::optional<std::size_t> index =
            rule.IsMap() ? NumberOf<std::size_t>(rule[kIndexField]) : std::nullopt;
        std::optional<ro::sim::CompanionRule> read =
            rule.IsMap() ? ReadRule(rule[kRuleField], failure) : std::nullopt;
        if (index.has_value() && *index < ro::sim::kCompanionRuleMost && read.has_value()) {
          kept.push_back(CompanionRuleMemory{.index = *index,
                                             .rule = std::move(*read),
                                             .acted_ago_ms = Duration(rule, kActedAgoField),
                                             .suspended_for_ms = Duration(rule, kSuspendedField)});
        }
      }
    }
  }
  return state;
}

void EmitCompanion(YAML::Emitter& out, const StoredCompanionState& state) {
  out << YAML::BeginMap;
  out << YAML::Key << kClassField << YAML::Value << state.job;
  out << YAML::Key << kWrittenField << YAML::Value << state.written_wall_ms;
  out << YAML::Key << kStandbyField << YAML::Value << state.standby;
  out << YAML::Key << kSkillsField << YAML::Value << YAML::BeginSeq;
  for (const CompanionSkillMemory& skill : state.skills) {
    out << YAML::Flow << YAML::BeginMap << YAML::Key << kSkillField << YAML::Value
        << skill.skill_id;
    EmitDuration(out, kCastAgoField, skill.cast_ago_ms);
    EmitDuration(out, kSuspendedField, skill.suspended_for_ms);
    out << YAML::EndMap;
  }
  out << YAML::EndSeq;
  out << YAML::Key << kProfilesField << YAML::Value << YAML::BeginSeq;
  for (const auto& [name, rules] : state.rules_by_profile) {
    out << YAML::BeginMap << YAML::Key << kNameField << YAML::Value << name;
    out << YAML::Key << kRulesField << YAML::Value << YAML::BeginSeq;
    for (const CompanionRuleMemory& rule : rules) {
      out << YAML::BeginMap << YAML::Key << kIndexField << YAML::Value << rule.index;
      EmitDuration(out, kActedAgoField, rule.acted_ago_ms);
      EmitDuration(out, kSuspendedField, rule.suspended_for_ms);
      out << YAML::Key << kRuleField << YAML::Value;
      EmitRule(out, rule.rule, YAML::Node());
      out << YAML::EndMap;
    }
    out << YAML::EndSeq << YAML::EndMap;
  }
  out << YAML::EndSeq;
  out << YAML::EndMap;
}

[[nodiscard]] CharacterKey KeyOf(const YAML::Node& entry) {
  CharacterKey key;
  if (!entry.IsMap()) {
    return key;
  }
  key.cid = NumberOf<std::uint32_t>(entry[kCidField]).value_or(0);
  if (const YAML::Node name = entry[kNameField]; name && name.IsScalar()) {
    key.name = name.Scalar();
  }
  return key;
}

// LA FICHE DE CE PERSONNAGE : ses compagnons de même classe remplacés, les
// autres et ses champs inconnus gardés.
void EmitCharacter(YAML::Emitter& out, const YAML::Node& existing, const CharacterKey& key,
                   const std::vector<StoredCompanionState>& companions) {
  out << YAML::BeginMap;
  if (key.cid != 0) {
    out << YAML::Key << kCidField << YAML::Value << key.cid;
  }
  if (!key.name.empty()) {
    out << YAML::Key << kNameField << YAML::Value << key.name;
  }
  out << YAML::Key << kCompanionsField << YAML::Value << YAML::BeginSeq;
  const YAML::Node kept = existing ? existing[kCompanionsField] : YAML::Node();
  if (kept && kept.IsSequence()) {
    for (const YAML::Node& entry : kept) {
      const std::optional<std::uint16_t> job =
          entry.IsMap() ? NumberOf<std::uint16_t>(entry[kClassField]) : std::nullopt;
      if (!job.has_value() ||
          std::ranges::none_of(companions, [&job](const StoredCompanionState& state) {
            return state.job == *job;
          })) {
        out << entry;
      }
    }
  }
  for (const StoredCompanionState& state : companions) {
    EmitCompanion(out, state);
  }
  out << YAML::EndSeq;
  if (existing && existing.IsMap()) {
    for (const auto& field : existing) {
      if (!IsField(field.first, kCidField) && !IsField(field.first, kNameField) &&
          !IsField(field.first, kCompanionsField)) {
        out << YAML::Key << field.first << YAML::Value << field.second;
      }
    }
  }
  out << YAML::EndMap;
}

void EmitCharacters(YAML::Emitter& out, const YAML::Node& existing, const CharacterKey& key,
                    const std::vector<StoredCompanionState>& companions) {
  out << YAML::BeginSeq;
  bool placed = false;
  if (existing && existing.IsSequence()) {
    for (const YAML::Node& entry : existing) {
      if (!placed && entry.IsMap() && SameCharacter(KeyOf(entry), key)) {
        EmitCharacter(out, entry, key, companions);
        placed = true;
      } else {
        out << entry;
      }
    }
  }
  if (!placed) {
    EmitCharacter(out, YAML::Node(), key, companions);
  }
  out << YAML::EndSeq;
}

// LE FICHIER ILLISIBLE MIS DE CÔTÉ, sous le premier nom libre ; faux quand
// aucun ne l'est, ou que le disque refuse.
[[nodiscard]] bool SetAside(const std::filesystem::path& path) {
  for (int rank = 1; rank <= kStateSetAsideMost; ++rank) {
    std::filesystem::path aside = path;
    aside += kStateSetAsideSuffix;
    if (rank > 1) {
      aside += "." + std::to_string(rank);
    }
    std::error_code failed;
    if (std::filesystem::exists(aside, failed)) {
      continue;
    }
    std::filesystem::rename(path, aside, failed);
    return !failed;
  }
  return false;
}

}  // namespace

std::filesystem::path CompanionStatePath(const std::filesystem::path& client_root) {
  return client_root / kSaveFolder / kCompanionStateFileName;
}

CompanionStateRead ParseCompanionState(std::string_view text) {
  CompanionStateRead read;
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
  if (NumberOf<int>(whole[kVersionField]) != kCompanionStateFileVersion) {
    read.unknown_version = true;
    return read;
  }
  const YAML::Node characters = whole[kCharactersField];
  if (!characters || !characters.IsSequence()) {
    return read;
  }
  for (const YAML::Node& entry : characters) {
    StoredCharacterState character{.key = KeyOf(entry)};
    if (character.key.cid == 0 && character.key.name.empty()) {
      continue;
    }
    const YAML::Node companions = entry[kCompanionsField];
    if (companions && companions.IsSequence()) {
      for (const YAML::Node& companion : companions) {
        if (std::optional<StoredCompanionState> state = ReadCompanion(companion);
            state.has_value()) {
          character.companions.push_back(std::move(*state));
        }
      }
    }
    read.characters.push_back(std::move(character));
  }
  return read;
}

std::optional<std::string> MergeCompanionState(
    std::string_view current_text, const CharacterKey& key,
    const std::vector<StoredCompanionState>& companions) {
  YAML::Node whole;
  try {
    whole = YAML::Load(std::string(current_text));
  } catch (const YAML::Exception&) {
    return std::nullopt;
  }
  if (!whole || whole.IsNull()) {
    whole = YAML::Node(YAML::NodeType::Map);
    whole[kVersionField] = kCompanionStateFileVersion;
  }
  const YAML::Node characters = whole.IsMap() ? whole[kCharactersField] : YAML::Node();
  if (!whole.IsMap() || NumberOf<int>(whole[kVersionField]) != kCompanionStateFileVersion ||
      (characters && !characters.IsNull() && !characters.IsSequence())) {
    return std::nullopt;
  }
  YAML::Emitter out;
  out << YAML::BeginMap;
  for (const auto& field : whole) {
    out << YAML::Key << field.first << YAML::Value;
    if (IsField(field.first, kCharactersField)) {
      EmitCharacters(out, field.second, key, companions);
    } else {
      out << field.second;
    }
  }
  if (!characters) {
    out << YAML::Key << kCharactersField << YAML::Value;
    EmitCharacters(out, YAML::Node(), key, companions);
  }
  out << YAML::EndMap;
  if (!out.good()) {
    return std::nullopt;
  }
  return std::string(kFileHeader) + out.c_str() + "\n";
}

CompanionStateRead LoadCompanionState(const std::filesystem::path& client_root) {
  const std::filesystem::path path = CompanionStatePath(client_root);
  const std::optional<std::string> text = ReadWhole(path);
  if (!text.has_value()) {
    ro::core::LogWarning("IA des compagnons : {} ne s'ouvre pas", ro::core::Utf8FromPath(path));
    return CompanionStateRead{.unreadable = true};
  }
  CompanionStateRead read = ParseCompanionState(*text);
  if (read.unreadable || read.unknown_version) {
    ro::core::LogWarning(
        "IA des compagnons : {} n'est ni relu ni réécrit (illisible, ou d'une "
        "version inconnue)",
        ro::core::Utf8FromPath(path));
  }
  return read;
}

StateWrite SaveCompanionState(const std::filesystem::path& client_root, const CharacterKey& key,
                              const std::vector<StoredCompanionState>& companions) {
  const std::filesystem::path path = CompanionStatePath(client_root);
  const std::optional<std::string> current = ReadWhole(path);
  if (!current.has_value()) {
    return StateWrite::kFailed;
  }
  if (ParseCompanionState(*current).unknown_version) {
    return StateWrite::kUnknownVersion;
  }
  std::optional<std::string> merged = MergeCompanionState(*current, key, companions);
  if (!merged.has_value()) {
    // ILLISIBLE, OU D'UNE FORME QU'ON NE SAIT PAS COMPLÉTER : mis de côté, puis
    // refait à neuf.
    if (!SetAside(path)) {
      return StateWrite::kFailed;
    }
    ro::core::LogWarning("IA des compagnons : {} illisible, mis de côté et refait",
                         ro::core::Utf8FromPath(path));
    merged = MergeCompanionState("", key, companions);
  }
  if (!merged.has_value() || !ro::core::WriteFileAtomically(path, *merged)) {
    return StateWrite::kFailed;
  }
  return StateWrite::kWritten;
}

std::vector<StoredCompanionState> ComparableStates(std::vector<StoredCompanionState> states,
                                                   std::uint64_t elapsed_ms) {
  for (StoredCompanionState& state : states) {
    state.written_wall_ms = 0;
    state.skills =
        ro::sim::AgedMemory(ro::sim::CompanionMemory{.skills = std::move(state.skills)}, elapsed_ms)
            .skills;
    for (auto& [profile, rules] : state.rules_by_profile) {
      rules = ro::sim::AgedMemory(ro::sim::CompanionMemory{.rules = std::move(rules)}, elapsed_ms)
                  .rules;
    }
    std::erase_if(state.rules_by_profile, [](const auto& entry) { return entry.second.empty(); });
  }
  return states;
}

std::int64_t WallNowMs() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

ro::sim::CompanionMemory MemoryOf(const StoredCompanionState& state, std::string_view profile,
                                  std::int64_t wall_now_ms) {
  ro::sim::CompanionMemory memory{.skills = state.skills, .standby = state.standby};
  if (const auto found = state.rules_by_profile.find(std::string(profile));
      found != state.rules_by_profile.end()) {
    memory.rules = found->second;
  }
  const std::int64_t elapsed_ms = std::max<std::int64_t>(wall_now_ms - state.written_wall_ms, 0);
  return ro::sim::AgedMemory(std::move(memory), static_cast<std::uint64_t>(elapsed_ms));
}

}  // namespace moonclient
