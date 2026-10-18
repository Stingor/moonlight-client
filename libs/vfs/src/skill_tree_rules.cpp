// Voir l'en-tête.

#include "ro/vfs/skill_tree_rules.h"

#include <algorithm>
#include <expected>
#include <limits>
#include <string>

#include <yaml-cpp/yaml.h>

#include "yaml_required.h"

namespace ro::vfs {
namespace {

constexpr int kSupportedVersion = 1;
// Un prérequis s'écrit [compétence, rang].
constexpr std::size_t kNeedSkillField = 0;
constexpr std::size_t kNeedLevelField = 1;
constexpr std::size_t kNeedFieldCount = 2;
constexpr std::int64_t kMostSkill = std::numeric_limits<std::uint16_t>::max();
// Les niveaux requis et les métiers voyagent sur trente-deux bits.
constexpr std::int64_t kMostWide = std::numeric_limits<std::uint32_t>::max();

core::Error Refused(std::string detail) {
  return core::Error{core::ErrorCode::kCorruptData,
                     "arbres des compétences : " + std::move(detail)};
}

[[nodiscard]] std::uint16_t SkillNumber(const YAML::Node& node) {
  return static_cast<std::uint16_t>(Whole(node, 0, kMostSkill));
}

// Un champ facultatif : son défaut quand il manque.
[[nodiscard]] std::uint32_t LevelOr(const YAML::Node& entry, const char* key) {
  const YAML::Node value = entry[key];
  return value ? static_cast<std::uint32_t>(Whole(value, 0, kMostWide)) : 0U;
}

[[nodiscard]] ServerSkillRule ReadRule(const YAML::Node& entry) {
  if (!entry.IsMap()) {
    throw YAML::RepresentationException(entry.Mark(), "une compétence n'est pas une table");
  }
  ServerSkillRule rule{.skill_id = SkillNumber(Required(entry, "id")),
                       .max_level = SkillNumber(Required(entry, "max")),
                       .base_level = LevelOr(entry, "base_level"),
                       .job_level = LevelOr(entry, "job_level")};
  if (const YAML::Node points = entry["by_points"]; points) {
    rule.by_points = points.as<bool>();
  }
  const YAML::Node needs = entry["needs"];
  if (!needs) {
    return rule;
  }
  if (!needs.IsSequence()) {
    throw YAML::RepresentationException(needs.Mark(), "« needs » n'est pas une liste");
  }
  for (const YAML::Node& need : needs) {
    if (!need.IsSequence() || need.size() != kNeedFieldCount) {
      throw YAML::RepresentationException(need.Mark(),
                                          "un prérequis n'est pas une paire [compétence, rang]");
    }
    rule.needs.emplace_back(SkillNumber(need[kNeedSkillField]), SkillNumber(need[kNeedLevelField]));
  }
  return rule;
}

[[nodiscard]] std::vector<ServerSkillRule> ReadRules(const YAML::Node& list) {
  if (!list.IsSequence()) {
    throw YAML::RepresentationException(list.Mark(), "un arbre n'est pas une liste");
  }
  std::vector<ServerSkillRule> rules;
  rules.reserve(list.size());
  for (const YAML::Node& entry : list) {
    rules.push_back(ReadRule(entry));
  }
  return rules;
}

}  // namespace

core::Result<SkillTreeRules> SkillTreeRules::Load(Vfs& vfs) {
  auto bytes = vfs.Read(std::string(kSkillTreeRulesPath));
  if (!bytes) {
    return std::unexpected(bytes.error());
  }
  const std::string_view text(reinterpret_cast<const char*>(bytes->data()), bytes->size());
  return Parse(text);
}

core::Result<SkillTreeRules> SkillTreeRules::Parse(std::string_view yaml_text) {
  SkillTreeRules loaded;
  // yaml-cpp signale par exception : elle ne franchit pas cette fonction.
  try {
    const YAML::Node whole = YAML::Load(std::string(yaml_text));
    if (!whole.IsMap()) {
      return std::unexpected(Refused("le texte n'est pas une table YAML"));
    }
    const YAML::Node version = whole["version"];
    if (!version || version.as<int>() != kSupportedVersion) {
      return std::unexpected(Refused("version non gérée"));
    }
    loaded.skill_free_ = Required(whole, "skill_free").as<bool>();
    const YAML::Node jobs = Required(whole, "jobs");
    if (!jobs.IsMap()) {
      return std::unexpected(Refused("« jobs » n'est pas une table"));
    }
    for (const auto& pair : jobs) {
      loaded.jobs_.push_back(
          ServerJobSkills{.job = static_cast<std::uint32_t>(Whole(pair.first, 0, kMostWide)),
                          .skills = ReadRules(pair.second)});
    }
    loaded.guild_ = ReadRules(Required(whole, "guild"));
  } catch (const YAML::Exception& failure) {
    return std::unexpected(Refused(failure.what()));
  }
  std::ranges::sort(loaded.jobs_, {}, &ServerJobSkills::job);
  return loaded;
}

}  // namespace ro::vfs
