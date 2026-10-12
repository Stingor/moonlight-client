#include "ro/vfs/random_option_names.h"

#include <expected>
#include <string>
#include <utility>

#include <yaml-cpp/yaml.h>

namespace ro::vfs {
namespace {

constexpr int kSupportedVersion = 1;

core::Error Refused(std::string detail) {
  return core::Error{core::ErrorCode::kCorruptData, "options aléatoires : " + std::move(detail)};
}

}  // namespace

std::string FillOptionTemplate(std::string_view pattern, int value) {
  std::string out;
  out.reserve(pattern.size() + 8);
  bool value_written = false;
  for (std::size_t at = 0; at < pattern.size(); ++at) {
    const char c = pattern[at];
    if (c != '%') {
      out.push_back(c);
      continue;
    }
    if (at + 1 >= pattern.size()) {
      return {};
    }
    const char spec = pattern[at + 1];
    ++at;
    if (spec == '%') {
      out.push_back('%');
    } else if (spec == 'd' && !value_written) {
      out += std::to_string(value);
      value_written = true;
    } else {
      return {};  // une conversion qu'on ne sait pas remplir sans risque
    }
  }
  return out;
}

std::string RandomOptionNames::Format(std::uint32_t option_id, int value) const {
  const auto found = templates.find(option_id);
  if (found == templates.end()) {
    return {};
  }
  return FillOptionTemplate(found->second, value);
}

core::Result<RandomOptionNames> ParseRandomOptionNames(std::string_view yaml_text) {
  RandomOptionNames names;
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
    const YAML::Node options = whole["options"];
    if (!options || !options.IsMap()) {
      return std::unexpected(Refused("« options » n'est pas une table"));
    }
    for (const auto& pair : options) {
      const auto option_id = pair.first.as<std::uint32_t>();
      if (!pair.second.IsMap()) {
        return std::unexpected(
            Refused("l'option " + std::to_string(option_id) + " n'est pas une table"));
      }
      const YAML::Node name = pair.second["name"];
      if (!name) {
        continue;  // une option que le serveur connaît sans libellé
      }
      if (!name.IsScalar()) {
        return std::unexpected(
            Refused("le nom de l'option " + std::to_string(option_id) + " n'est pas un texte"));
      }
      names.templates[option_id] = name.as<std::string>();
    }
  } catch (const YAML::Exception& failure) {
    return std::unexpected(Refused(failure.what()));
  }
  return names;
}

core::Result<RandomOptionNames> ReadRandomOptionNames(Vfs& vfs) {
  auto bytes = vfs.Read(std::string(kRandomOptionsPath));
  if (!bytes) {
    if (bytes.error().code == core::ErrorCode::kNotFound) {
      return RandomOptionNames{};
    }
    return std::unexpected(bytes.error());
  }
  const std::string_view text(reinterpret_cast<const char*>(bytes->data()), bytes->size());
  return ParseRandomOptionNames(text);
}

}  // namespace ro::vfs
