// Voir l'en-tête.

#include "ro/vfs/monster_talk_table.h"

#include <expected>
#include <utility>

#include <yaml-cpp/yaml.h>

#include "ro/core/log.h"

namespace ro::vfs {
namespace {

constexpr int kSupportedVersion = 1;
constexpr std::string_view kAttackNode = "attack";

core::Error Refused(std::string detail) {
  return core::Error{core::ErrorCode::kCorruptData,
                     "répliques des monstres : " + std::move(detail)};
}

}  // namespace

std::span<const std::string> MonsterTalkTable::AttackLinesOf(std::string_view species) const {
  const auto found = attack_lines_by_species.find(std::string(species));
  if (found == attack_lines_by_species.end()) {
    return {};
  }
  return found->second;
}

core::Result<MonsterTalkTable> ParseMonsterTalkTable(std::string_view yaml_text) {
  MonsterTalkTable table;
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
    const YAML::Node species = whole["species"];
    if (!species || !species.IsMap()) {
      return std::unexpected(Refused("« species » n'est pas une table"));
    }
    for (const auto& pair : species) {
      const std::string name = pair.first.as<std::string>();
      if (!pair.second.IsMap()) {
        return std::unexpected(Refused("l'espèce " + name + " n'est pas une table"));
      }
      std::vector<std::string>& said = table.attack_lines_by_species[name];
      for (const auto& occasion : pair.second) {
        if (occasion.first.as<std::string>() != kAttackNode || !occasion.second.IsSequence()) {
          return std::unexpected(
              Refused("occasion inconnue : " + occasion.first.as<std::string>()));
        }
        for (const YAML::Node& line : occasion.second) {
          said.push_back(line.as<std::string>());
        }
      }
    }
  } catch (const YAML::Exception& failure) {
    return std::unexpected(Refused(failure.what()));
  }
  return table;
}

MonsterTalkTable ReadMonsterTalkTable(Vfs& vfs) {
  auto bytes = vfs.Read(std::string(kMonsterTalkPath));
  if (!bytes) {
    core::LogWarning("{} absente — les monstres se tairont", kMonsterTalkPath);
    return {};
  }
  const std::string_view text(reinterpret_cast<const char*>(bytes->data()), bytes->size());
  core::Result<MonsterTalkTable> table = ParseMonsterTalkTable(text);
  if (!table) {
    core::LogWarning("{} — les monstres se tairont", core::Describe(table.error()));
    return {};
  }
  return std::move(*table);
}

}  // namespace ro::vfs
