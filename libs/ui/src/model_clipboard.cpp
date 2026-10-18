#include "ro/ui/model_clipboard.h"

#include <charconv>
#include <cstddef>
#include <format>
#include <system_error>
#include <vector>

namespace ro::ui {
namespace {

// Le mot qui ouvre la ligne, et ce qui sépare ses champs.
constexpr std::string_view kClipboardTag = "moonclient-modèle";
constexpr char kFieldSeparator = '|';
// Le tag, le fichier, la rotation, l'échelle, l'opacité.
constexpr std::size_t kFieldCount = 5;

// Les champs de `text`, coupés à chaque séparateur.
std::vector<std::string_view> Fields(std::string_view text) {
  std::vector<std::string_view> fields;
  std::size_t from = 0;
  while (true) {
    const std::size_t at = text.find(kFieldSeparator, from);
    fields.push_back(
        text.substr(from, at == std::string_view::npos ? text.size() - from : at - from));
    if (at == std::string_view::npos) {
      return fields;
    }
    from = at + 1;
  }
}

// Un nombre, et rien d'autre que des espaces autour.
std::optional<float> NumberOf(std::string_view text) {
  while (!text.empty() && text.front() == ' ') {
    text.remove_prefix(1);
  }
  while (!text.empty() && (text.back() == ' ' || text.back() == '\r' || text.back() == '\n')) {
    text.remove_suffix(1);
  }
  float value = 0.0F;
  const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
  if (text.empty() || error != std::errc() || end != text.data() + text.size()) {
    return std::nullopt;
  }
  return value;
}

// Trois nombres séparés par des espaces.
std::optional<std::array<float, 3>> TripleOf(std::string_view text) {
  std::array<float, 3> values{};
  std::size_t count = 0;
  std::size_t from = 0;
  while (from < text.size()) {
    if (text[from] == ' ') {
      ++from;
      continue;
    }
    std::size_t to = text.find(' ', from);
    if (to == std::string_view::npos) {
      to = text.size();
    }
    const std::optional<float> value = NumberOf(text.substr(from, to - from));
    if (!value.has_value() || count == values.size()) {
      return std::nullopt;
    }
    values[count++] = *value;
    from = to;
  }
  if (count != values.size()) {
    return std::nullopt;
  }
  return values;
}

}  // namespace

std::string ModelClipboardText(const ClipboardModel& model) {
  return std::format("{}{}{}{}{} {} {}{}{} {} {}{}{}", kClipboardTag, kFieldSeparator,
                     model.model_file, kFieldSeparator, model.rotation[0], model.rotation[1],
                     model.rotation[2], kFieldSeparator, model.scale[0], model.scale[1],
                     model.scale[2], kFieldSeparator,
                     model.opacity.has_value() ? std::format("{}", *model.opacity) : std::string());
}

std::optional<ClipboardModel> ParseModelClipboard(std::string_view text) {
  while (!text.empty() && (text.back() == '\r' || text.back() == '\n' || text.back() == ' ')) {
    text.remove_suffix(1);
  }
  const std::vector<std::string_view> fields = Fields(text);
  if (fields.size() != kFieldCount || fields[0] != kClipboardTag || fields[1].empty()) {
    return std::nullopt;
  }
  const std::optional<std::array<float, 3>> rotation = TripleOf(fields[2]);
  const std::optional<std::array<float, 3>> scale = TripleOf(fields[3]);
  if (!rotation.has_value() || !scale.has_value()) {
    return std::nullopt;
  }
  ClipboardModel model{
      .model_file = std::string(fields[1]), .rotation = *rotation, .scale = *scale};
  if (!fields[4].empty()) {
    const std::optional<float> opacity = NumberOf(fields[4]);
    if (!opacity.has_value() || !(*opacity >= 0.0F && *opacity <= 1.0F)) {
      return std::nullopt;
    }
    model.opacity = opacity;
  }
  return model;
}

}  // namespace ro::ui
