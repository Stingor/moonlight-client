#include "ro/ui/hour_range_text.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <format>

#include "imgui.h"

namespace ro::ui {
namespace {

constexpr int kMinutesPerHour = 60;
constexpr int kHoursPerDay = 24;
constexpr int kMinutesPerDay = kMinutesPerHour * kHoursPerDay;
constexpr float kMinutesPerHourFloat = 60.0F;
// Une borne compte un ou deux chiffres d'heure, et deux de minutes.
constexpr std::size_t kMostHourDigits = 2;
constexpr std::size_t kMinuteDigits = 2;
constexpr int kDecimalBase = 10;
// Ce qui sépare les deux bornes : le tiret, le demi-cadratin, le cadratin.
constexpr std::array<std::string_view, 3> kDashes = {"-", "–", "—"};

[[nodiscard]] bool IsBlank(char letter) {
  return letter == ' ' || letter == '\t' || letter == '\n' || letter == '\r';
}

[[nodiscard]] bool IsDigit(char letter) { return letter >= '0' && letter <= '9'; }

[[nodiscard]] std::string_view Trimmed(std::string_view text) {
  while (!text.empty() && IsBlank(text.front())) {
    text.remove_prefix(1);
  }
  while (!text.empty() && IsBlank(text.back())) {
    text.remove_suffix(1);
  }
  return text;
}

// Le nombre que forment les chiffres de `text` à partir de `at`, au plus
// `most` ; `at` avance d'autant. Rien sans aucun chiffre.
[[nodiscard]] std::optional<int> ReadDigits(std::string_view text, std::size_t& at,
                                            std::size_t most) {
  int value = 0;
  std::size_t count = 0;
  while (at < text.size() && count < most && IsDigit(text[at])) {
    value = value * kDecimalBase + (text[at] - '0');
    ++at;
    ++count;
  }
  if (count == 0) {
    return std::nullopt;
  }
  return value;
}

// Une borne en minutes depuis minuit : « 20:30 », « 20h30 », « 20h », « 20 ».
[[nodiscard]] std::optional<int> MinutesOfBound(std::string_view text) {
  text = Trimmed(text);
  std::size_t at = 0;
  const std::optional<int> hour = ReadDigits(text, at, kMostHourDigits);
  if (!hour.has_value()) {
    return std::nullopt;
  }
  int minute = 0;
  if (at < text.size()) {
    const char mark = text[at++];
    if (mark != ':' && mark != 'h' && mark != 'H') {
      return std::nullopt;
    }
    if (at < text.size()) {
      const std::size_t start = at;
      const std::optional<int> read = ReadDigits(text, at, kMinuteDigits);
      if (!read.has_value() || at - start != kMinuteDigits || at != text.size()) {
        return std::nullopt;
      }
      minute = *read;
    } else if (mark == ':') {
      // « 20: » est une saisie coupée, pas une heure.
      return std::nullopt;
    }
  }
  if (*hour > kHoursPerDay || minute >= kMinutesPerHour || (*hour == kHoursPerDay && minute != 0)) {
    return std::nullopt;
  }
  return *hour * kMinutesPerHour + minute;
}

[[nodiscard]] std::string BoundText(float hours) {
  const int minutes =
      std::clamp(static_cast<int>(std::lround(hours * kMinutesPerHourFloat)), 0, kMinutesPerDay);
  return std::format("{:02}:{:02}", minutes / kMinutesPerHour, minutes % kMinutesPerHour);
}

}  // namespace

std::string HourRangeText(const HourRange& range) {
  return BoundText(range.from_hour) + "-" + BoundText(range.to_hour);
}

std::optional<HourRange> ParseHourRange(std::string_view text) {
  std::size_t dash_at = std::string_view::npos;
  std::size_t dash_size = 0;
  for (const std::string_view dash : kDashes) {
    const std::size_t found = text.find(dash);
    if (found < dash_at) {
      dash_at = found;
      dash_size = dash.size();
    }
  }
  if (dash_at == std::string_view::npos) {
    return std::nullopt;
  }
  const std::optional<int> from = MinutesOfBound(text.substr(0, dash_at));
  const std::optional<int> to = MinutesOfBound(text.substr(dash_at + dash_size));
  if (!from.has_value() || !to.has_value()) {
    return std::nullopt;
  }
  return HourRange{.from_hour = static_cast<float>(*from) / kMinutesPerHourFloat,
                   .to_hour = static_cast<float>(*to) / kMinutesPerHourFloat};
}

std::optional<HourRange> ClipboardHourRange() {
  const char* clipboard = ImGui::GetClipboardText();
  if (clipboard == nullptr) {
    return std::nullopt;
  }
  return ParseHourRange(clipboard);
}

}  // namespace ro::ui
