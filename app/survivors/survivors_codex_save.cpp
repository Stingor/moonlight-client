#include "survivors/survivors_codex_save.h"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <yaml-cpp/yaml.h>

namespace moonclient {
namespace {

[[nodiscard]] std::string YamlText(const YAML::Node& node) {
  YAML::Emitter out;
  out << node;
  return out.c_str();
}

// Un texte YAML gardé tel quel ; rien s'il ne se relit plus.
[[nodiscard]] std::optional<YAML::Node> KeptNode(const std::string& yaml) {
  try {
    return YAML::Load(yaml);
  } catch (const YAML::Exception&) {
    return std::nullopt;
  }
}

// Une liste d'une catégorie connue : chaque chaîne une fois, et chaque entrée
// qui n'en est pas une à sa place.
void ReadList(const YAML::Node& listed, CodexCategory category, SurvivorsCodexSeen& seen) {
  std::vector<SurvivorsCodexMark>& marks = seen.lists[static_cast<std::size_t>(category)];
  for (const YAML::Node& entry : listed) {
    if (entry.IsScalar()) {
      seen.Add(category, entry.Scalar());
    } else {
      marks.push_back(SurvivorsCodexMark{.text = YamlText(entry), .foreign = true});
    }
  }
}

void WriteList(YAML::Emitter& out, const std::vector<SurvivorsCodexMark>& marks) {
  out << YAML::Flow << YAML::BeginSeq;
  for (const SurvivorsCodexMark& mark : marks) {
    if (!mark.foreign) {
      out << mark.text;
    } else if (const std::optional<YAML::Node> kept = KeptNode(mark.text)) {
      out << *kept;
    }
  }
  out << YAML::EndSeq;
}

}  // namespace

SurvivorsCodexSeen ReadSurvivorsCodex(std::string_view yaml) {
  SurvivorsCodexSeen seen;
  try {
    const YAML::Node whole = YAML::Load(std::string(yaml));
    if (!whole.IsMap()) return seen;
    for (const auto& entry : whole) {
      if (!entry.first.IsScalar()) continue;
      const std::string& key = entry.first.Scalar();
      const std::optional<CodexCategory> category = CodexCategoryOf(key);
      if (!category.has_value()) {
        seen.foreign_categories.push_back(
            SurvivorsCodexKept{.key = key, .yaml = YamlText(entry.second)});
      } else if (entry.second.IsSequence()) {
        ReadList(entry.second, *category, seen);
      } else if (!entry.second.IsNull()) {
        seen.other_shapes[static_cast<std::size_t>(*category)] = YamlText(entry.second);
      }
    }
  } catch (const YAML::Exception&) {
    return SurvivorsCodexSeen{};
  }
  return seen;
}

std::string WriteSurvivorsCodex(const SurvivorsCodexSeen& seen) {
  YAML::Emitter out;
  out << YAML::BeginMap;
  for (std::size_t index = 0; index < kCodexCategoryCount; ++index) {
    const char* key = CodexCategoryKey(static_cast<CodexCategory>(index));
    const std::vector<SurvivorsCodexMark>& marks = seen.lists[index];
    if (!marks.empty()) {
      out << YAML::Key << key << YAML::Value;
      WriteList(out, marks);
    } else if (const std::optional<YAML::Node> kept = KeptNode(seen.other_shapes[index]);
               !seen.other_shapes[index].empty() && kept.has_value()) {
      out << YAML::Key << key << YAML::Value << *kept;
    }
  }
  for (const SurvivorsCodexKept& kept : seen.foreign_categories) {
    if (const std::optional<YAML::Node> node = KeptNode(kept.yaml)) {
      out << YAML::Key << kept.key << YAML::Value << *node;
    }
  }
  out << YAML::EndMap;
  return out.c_str();
}

}  // namespace moonclient
