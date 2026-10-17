#include "ro/core/ascii.h"

namespace ro::core {

std::string AsciiLower(std::string_view text) {
  std::string lowered;
  lowered.reserve(text.size());
  for (const char character : text) {
    lowered.push_back(static_cast<char>(AsciiLower(static_cast<std::uint8_t>(character))));
  }
  return lowered;
}

bool AsciiLess(std::string_view left, std::string_view right) {
  return AsciiCompareFolded(left, right) < 0;
}

int AsciiCompareFolded(std::string_view left, std::string_view right) {
  const std::size_t shared = left.size() < right.size() ? left.size() : right.size();
  for (std::size_t rank = 0; rank < shared; ++rank) {
    const std::uint8_t one = AsciiLower(static_cast<std::uint8_t>(left[rank]));
    const std::uint8_t other = AsciiLower(static_cast<std::uint8_t>(right[rank]));
    if (one != other) {
      return one < other ? -1 : 1;
    }
  }
  if (left.size() == right.size()) {
    return 0;
  }
  return left.size() < right.size() ? -1 : 1;
}

bool AsciiEqualsFolded(std::string_view left, std::string_view right) {
  if (left.size() != right.size()) {
    return false;
  }
  for (std::size_t rank = 0; rank < left.size(); ++rank) {
    if (AsciiLower(static_cast<std::uint8_t>(left[rank])) !=
        AsciiLower(static_cast<std::uint8_t>(right[rank]))) {
      return false;
    }
  }
  return true;
}

bool AsciiStartsWithFolded(std::string_view text, std::string_view prefix) {
  return text.size() >= prefix.size() && AsciiEqualsFolded(text.substr(0, prefix.size()), prefix);
}

bool AsciiContainsFolded(std::string_view haystack, std::string_view needle) {
  for (std::size_t start = 0; start + needle.size() <= haystack.size(); ++start) {
    if (AsciiEqualsFolded(haystack.substr(start, needle.size()), needle)) {
      return true;
    }
  }
  return false;
}

}  // namespace ro::core
