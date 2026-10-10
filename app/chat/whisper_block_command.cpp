#include "chat/whisper_block_command.h"

#include <algorithm>
#include <cctype>

namespace moonclient {
namespace {

constexpr std::string_view kBlockWord = "/ex";
constexpr std::string_view kAllowWord = "/in";
constexpr std::string_view kBlockEveryoneWord = "/exall";
constexpr std::string_view kAllowEveryoneWord = "/inall";
constexpr std::string_view kBlanks = " \t";

[[nodiscard]] bool SameWord(std::string_view typed, std::string_view word) {
  return std::ranges::equal(typed, word, [](char left, char right) {
    return std::tolower(static_cast<unsigned char>(left)) ==
           std::tolower(static_cast<unsigned char>(right));
  });
}

[[nodiscard]] std::string_view Trimmed(std::string_view text) {
  const std::size_t first = text.find_first_not_of(kBlanks);
  if (first == std::string_view::npos) {
    return {};
  }
  return text.substr(first, text.find_last_not_of(kBlanks) - first + 1);
}

}  // namespace

WhisperBlockCommand ReadWhisperBlockCommand(std::string_view line) {
  const std::size_t space = line.find(' ');
  const std::string_view word = line.substr(0, space);
  const std::string_view name =
      Trimmed(space == std::string_view::npos ? std::string_view() : line.substr(space + 1));
  if (SameWord(word, kBlockEveryoneWord)) {
    return {.kind = WhisperBlockCommandKind::kBlockEveryone};
  }
  if (SameWord(word, kAllowEveryoneWord)) {
    return {.kind = WhisperBlockCommandKind::kAllowEveryone};
  }
  if (SameWord(word, kBlockWord)) {
    return name.empty() ? WhisperBlockCommand{.kind = WhisperBlockCommandKind::kList}
                        : WhisperBlockCommand{.kind = WhisperBlockCommandKind::kBlock,
                                              .name = std::string(name)};
  }
  if (SameWord(word, kAllowWord)) {
    return name.empty() ? WhisperBlockCommand{.kind = WhisperBlockCommandKind::kAllowWithoutName}
                        : WhisperBlockCommand{.kind = WhisperBlockCommandKind::kAllow,
                                              .name = std::string(name)};
  }
  return {};
}

}  // namespace moonclient
