// Voir l'en-tête.

#include "ro/vfs/status_table.h"

#include <algorithm>
#include <cstdint>
#include <expected>
#include <utility>

#include <yaml-cpp/yaml.h>

namespace ro::vfs {
namespace {

constexpr int kSupportedVersion = 1;
constexpr int kMostColorChannel = 255;

core::Error Refused(std::string detail) {
  return core::Error{core::ErrorCode::kCorruptData, "table des états : " + std::move(detail)};
}

// Une ligne de description telle que la table l'écrit.
struct Line {
  std::string text;
  std::optional<StatusColor> color;
};

// `[rouge, vert, bleu]`, chacun de 0 à 255.
[[nodiscard]] core::Result<StatusColor> ParseColor(const YAML::Node& node) {
  if (!node.IsSequence() || node.size() != StatusColor{}.size()) {
    return std::unexpected(Refused("une couleur n'est pas trois composantes"));
  }
  StatusColor color{};
  for (std::size_t channel = 0; channel < color.size(); ++channel) {
    const int value = node[channel].as<int>();
    if (value < 0 || value > kMostColorChannel) {
      return std::unexpected(Refused("une composante de couleur sort de 0 … 255"));
    }
    color.at(channel) = static_cast<std::uint8_t>(value);
  }
  return color;
}

[[nodiscard]] core::Result<Line> ParseLine(const YAML::Node& node) {
  if (!node.IsMap()) {
    return std::unexpected(Refused("une ligne n'est pas une table"));
  }
  Line line;
  bool has_text = false;
  for (const auto& field : node) {
    const std::string key = field.first.as<std::string>();
    if (key == "text") {
      line.text = field.second.as<std::string>();
      has_text = true;
    } else if (key == "color") {
      core::Result<StatusColor> color = ParseColor(field.second);
      if (!color) {
        return std::unexpected(color.error());
      }
      line.color = *color;
    } else {
      return std::unexpected(Refused("champ de ligne inconnu : " + key));
    }
  }
  if (!has_text) {
    return std::unexpected(Refused("une ligne sans texte"));
  }
  return line;
}

// Les lignes d'un état : la première est son nom, une ligne « %s » la place de
// la durée, les autres sa description. Une première ligne vide ne nomme rien,
// et l'état n'a alors pas de description non plus.
void ApplyLines(const std::vector<Line>& lines, StatusInfo& into) {
  if (lines.empty() || lines.front().text.empty()) {
    return;
  }
  into.name = lines.front().text;
  into.name_color = lines.front().color;
  for (std::size_t rank = 1; rank < lines.size(); ++rank) {
    const Line& line = lines.at(rank);
    if (line.text.empty()) {
      continue;
    }
    if (line.text == kStatusTimePlaceholder) {
      // Sa place est ce qu'on retient d'elle : le rang qu'elle occupe parmi les
      // lignes gardées.
      into.time_line = static_cast<int>(into.description.size());
      into.time_color = line.color;
      continue;
    }
    into.description.push_back(line.text);
    into.description_colors.push_back(line.color);
  }
}

// Un état de la table, ou un refus.
[[nodiscard]] core::Result<StatusInfo> ParseStatus(const YAML::Node& node) {
  if (!node.IsMap()) {
    return std::unexpected(Refused("un état n'est pas une table"));
  }
  StatusInfo info;
  for (const auto& field : node) {
    const std::string key = field.first.as<std::string>();
    if (key == "constant") {
      if (!field.second.IsScalar()) {
        return std::unexpected(Refused("« constant » n'est pas un nom"));
      }
    } else if (key == "icon") {
      info.icon_file = field.second.as<std::string>();
    } else if (key == "priority") {
      const std::string name = field.second.as<std::string>();
      const auto found = std::ranges::find(kStatusPriorityNames, name);
      if (found == kStatusPriorityNames.end()) {
        return std::unexpected(Refused("seau inconnu : " + name));
      }
      info.priority = static_cast<StatusPriority>(found - kStatusPriorityNames.begin());
    } else if (key == "lines") {
      if (!field.second.IsSequence()) {
        return std::unexpected(Refused("« lines » n'est pas une liste"));
      }
      std::vector<Line> lines;
      for (const YAML::Node& line_node : field.second) {
        core::Result<Line> line = ParseLine(line_node);
        if (!line) {
          return std::unexpected(line.error());
        }
        lines.push_back(std::move(*line));
      }
      ApplyLines(lines, info);
    } else {
      return std::unexpected(Refused("champ d'état inconnu : " + key));
    }
  }
  return info;
}

}  // namespace

const StatusInfo* StatusTable::Find(std::uint32_t effect_id) const {
  const auto found = by_id.find(effect_id);
  return found != by_id.end() ? &found->second : nullptr;
}

std::string_view StatusTable::NameOf(std::uint32_t effect_id) const {
  const StatusInfo* found = Find(effect_id);
  return found != nullptr ? std::string_view(found->name) : std::string_view();
}

std::size_t StatusTable::drawn() const noexcept {
  std::size_t count = 0;
  for (const auto& [effect_id, info] : by_id) {
    if (!info.icon_file.empty()) {
      ++count;
    }
  }
  return count;
}

core::Result<StatusTable> ParseStatusTable(std::string_view yaml_text) {
  StatusTable table;
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
    const YAML::Node statuses = whole["statuses"];
    if (!statuses || !statuses.IsMap()) {
      return std::unexpected(Refused("« statuses » n'est pas une table"));
    }
    for (const auto& pair : statuses) {
      core::Result<StatusInfo> info = ParseStatus(pair.second);
      if (!info) {
        return std::unexpected(info.error());
      }
      if (info->name.empty() && info->icon_file.empty()) {
        continue;
      }
      table.by_id[pair.first.as<std::uint32_t>()] = std::move(*info);
    }
  } catch (const YAML::Exception& failure) {
    return std::unexpected(Refused(failure.what()));
  }
  return table;
}

core::Result<StatusTable> ReadStatusTable(Vfs& vfs) {
  auto bytes = vfs.Read(std::string(kStatusTablePath));
  if (!bytes) {
    if (bytes.error().code == core::ErrorCode::kNotFound) {
      return StatusTable{};
    }
    return std::unexpected(core::Error{
        bytes.error().code, std::string(kStatusTablePath) + " : " + bytes.error().detail});
  }
  const std::string_view text(reinterpret_cast<const char*>(bytes->data()), bytes->size());
  return ParseStatusTable(text);
}

}  // namespace ro::vfs
