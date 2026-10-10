#include "game/yaml_text.h"

#include <array>

namespace moonclient {

std::string_view YamlText::In(std::string_view requested, std::string_view file_language) const {
  const std::array<std::string_view, 3> wanted = {requested, file_language,
                                                  kYamlTextFallbackLanguage};
  for (const std::string_view language : wanted) {
    if (language.empty()) {
      continue;
    }
    for (const auto& [code, written] : by_language) {
      if (code == language) {
        return written;
      }
    }
  }
  return by_language.empty() ? std::string_view() : std::string_view(by_language.front().second);
}

YamlText ReadYamlText(const YAML::Node& node) {
  YamlText text;
  if (!node) {
    return text;
  }
  if (node.IsScalar()) {
    if (std::string plain = node.as<std::string>(); !plain.empty()) {
      text.by_language.emplace_back(std::string(kYamlTextFallbackLanguage), std::move(plain));
    }
    return text;
  }
  if (!node.IsMap()) {
    return text;
  }
  for (const auto& entry : node) {
    if (!entry.first.IsScalar() || !entry.second.IsScalar()) {
      continue;
    }
    std::string written = entry.second.as<std::string>();
    if (!written.empty()) {
      text.by_language.emplace_back(entry.first.as<std::string>(), std::move(written));
    }
  }
  return text;
}

}  // namespace moonclient
