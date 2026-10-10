#include "ro/formats/talk_lines.h"

namespace ro::formats {
namespace {

// Ce que le client retire de la réplique : il copie caractère par caractère et
// saute ceux-là.
constexpr std::string_view kStrippedFromLine = "\r\n\t";

// Le séparateur de lignes, celui que le client compte.
constexpr char kLineSeparator = '\r';

}  // namespace

std::string TalkLine(std::string_view contents, std::size_t index) {
  if (index == 0) {
    return {};
  }
  std::size_t line = 0;
  std::string said;
  for (const char letter : contents) {
    if (letter == kLineSeparator) {
      ++line;
      if (line > index) {
        break;
      }
      continue;
    }
    if (line == index && kStrippedFromLine.find(letter) == std::string_view::npos) {
      said.push_back(letter);
    }
  }
  return said;
}

}  // namespace ro::formats
