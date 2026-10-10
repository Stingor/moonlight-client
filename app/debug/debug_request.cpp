#include "debug/debug_request.h"

#include <charconv>
#include <vector>

namespace moonclient {
namespace {

// Les mots de la ligne, sans les espaces ni le retour chariot final.
[[nodiscard]] std::vector<std::string_view> WordsOf(std::string_view line) {
  std::vector<std::string_view> words;
  std::size_t at = 0;
  while (at < line.size()) {
    while (at < line.size() && (line[at] == ' ' || line[at] == '\r' || line[at] == '\t')) {
      ++at;
    }
    const std::size_t start = at;
    while (at < line.size() && line[at] != ' ' && line[at] != '\r' && line[at] != '\t') {
      ++at;
    }
    if (at > start) {
      words.push_back(line.substr(start, at - start));
    }
  }
  return words;
}

[[nodiscard]] std::optional<std::int64_t> IntOf(std::string_view text) {
  std::int64_t value = 0;
  const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
  if (error != std::errc() || end != text.data() + text.size()) {
    return std::nullopt;
  }
  return value;
}

}  // namespace

std::int64_t DebugRequest::IntParam(std::string_view key, std::int64_t fallback) const {
  const auto found = params.find(key);
  if (found == params.end()) {
    return fallback;
  }
  return IntOf(found->second).value_or(fallback);
}

std::string_view DebugRequest::TextParam(std::string_view key) const {
  const auto found = params.find(key);
  return found == params.end() ? std::string_view() : std::string_view(found->second);
}

std::optional<DebugRequest> ParseDebugRequest(std::string_view line) {
  const std::vector<std::string_view> words = WordsOf(line);
  if (words.size() < 2) {
    return std::nullopt;
  }
  const std::optional<std::int64_t> id = IntOf(words[0]);
  if (!id.has_value()) {
    return std::nullopt;
  }
  DebugRequest request{.id = *id, .method = std::string(words[1])};
  for (std::size_t rank = 2; rank < words.size(); ++rank) {
    const std::size_t equal = words[rank].find('=');
    if (equal == std::string_view::npos || equal == 0) {
      continue;
    }
    request.params.emplace(std::string(words[rank].substr(0, equal)),
                           std::string(words[rank].substr(equal + 1)));
  }
  return request;
}

}  // namespace moonclient
