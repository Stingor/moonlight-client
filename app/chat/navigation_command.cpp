#include "chat/navigation_command.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <optional>
#include <utility>

namespace moonclient {
namespace {

constexpr std::string_view kNaviCommand = "/navi";
constexpr std::string_view kNavigationCommand = "/navigation";
constexpr std::string_view kBlanks = " \t";
constexpr std::string_view kCoordinateSeparators = "/, \t";

[[nodiscard]] char Folded(char letter) {
  return static_cast<char>(std::tolower(static_cast<unsigned char>(letter)));
}

[[nodiscard]] bool SameWord(std::string_view word, std::string_view command) {
  return std::ranges::equal(word, command,
                            [](char left, char right) { return Folded(left) == Folded(right); });
}

[[nodiscard]] std::string_view Trimmed(std::string_view text) {
  const std::size_t first = text.find_first_not_of(kBlanks);
  if (first == std::string_view::npos) {
    return {};
  }
  return text.substr(first, text.find_last_not_of(kBlanks) - first + 1);
}

// Un entier positif ou nul, et rien d'autre dans le mot.
[[nodiscard]] std::optional<int> CoordinateOf(std::string_view word) {
  int value = 0;
  const auto [end, error] = std::from_chars(word.data(), word.data() + word.size(), value);
  if (error != std::errc{} || end != word.data() + word.size() || value < 0) {
    return std::nullopt;
  }
  return value;
}

// Ce qui suit le nom de la carte : « x/y », « x,y » ou « x y ».
[[nodiscard]] std::optional<PlaceLink> PlaceOf(std::string_view args) {
  const std::size_t map_end = args.find_first_of(kBlanks);
  if (map_end == std::string_view::npos) {
    return std::nullopt;
  }
  const std::string_view cells = Trimmed(args.substr(map_end));
  // Le séparateur : une barre, une virgule ou un blanc.
  const std::size_t split = cells.find_first_of(kCoordinateSeparators);
  if (split == std::string_view::npos) {
    return std::nullopt;
  }
  const std::optional<int> x = CoordinateOf(cells.substr(0, split));
  const std::optional<int> y = CoordinateOf(Trimmed(cells.substr(split + 1)));
  if (!x.has_value() || !y.has_value()) {
    return std::nullopt;
  }
  PlaceLink place;
  place.map = std::string(args.substr(0, map_end));
  std::ranges::transform(place.map, place.map.begin(), Folded);
  place.x = *x;
  place.y = *y;
  return place;
}

}  // namespace

NavigationCommand ReadNavigationCommand(std::string_view line) {
  const std::string_view trimmed = Trimmed(line);
  const std::string_view word = trimmed.substr(0, trimmed.find_first_of(kBlanks));
  const bool coordinates_allowed = SameWord(word, kNaviCommand);
  if (!coordinates_allowed && !SameWord(word, kNavigationCommand)) {
    return {};
  }
  const std::string_view args = Trimmed(trimmed.substr(word.size()));
  NavigationCommand read;
  if (args.empty()) {
    read.kind = NavigationCommandKind::kOpen;
    return read;
  }
  if (coordinates_allowed) {
    if (std::optional<PlaceLink> place = PlaceOf(args); place.has_value()) {
      read.kind = NavigationCommandKind::kGuide;
      read.place = std::move(*place);
      return read;
    }
  }
  read.kind = NavigationCommandKind::kSearch;
  read.term = std::string(args);
  return read;
}

std::string StaffWarpCommand(const PlaceLink& place) {
  std::string line = "@warp ";
  line += place.map;
  if (place.x > 0 && place.y > 0) {
    line += ',';
    line += std::to_string(place.x);
    line += ',';
    line += std::to_string(place.y);
  }
  return line;
}

}  // namespace moonclient
