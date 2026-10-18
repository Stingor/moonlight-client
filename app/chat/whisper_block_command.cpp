#include "chat/whisper_block_command.h"

#include "ro/core/ascii.h"

namespace moonclient {
namespace {

constexpr std::string_view kBlockWord = "/ex";
constexpr std::string_view kAllowWord = "/in";
constexpr std::string_view kBlockEveryoneWord = "/exall";
constexpr std::string_view kAllowEveryoneWord = "/inall";

}  // namespace

WhisperBlockCommand ReadWhisperBlockCommand(std::string_view line) {
  const std::size_t space = line.find(' ');
  const std::string_view word = line.substr(0, space);
  const std::string_view name = ro::core::AsciiTrimmed(
      space == std::string_view::npos ? std::string_view() : line.substr(space + 1));
  if (ro::core::AsciiEqualsFolded(word, kBlockEveryoneWord)) {
    return {.kind = WhisperBlockCommandKind::kBlockEveryone};
  }
  if (ro::core::AsciiEqualsFolded(word, kAllowEveryoneWord)) {
    return {.kind = WhisperBlockCommandKind::kAllowEveryone};
  }
  if (ro::core::AsciiEqualsFolded(word, kBlockWord)) {
    return name.empty() ? WhisperBlockCommand{.kind = WhisperBlockCommandKind::kList}
                        : WhisperBlockCommand{.kind = WhisperBlockCommandKind::kBlock,
                                              .name = std::string(name)};
  }
  if (ro::core::AsciiEqualsFolded(word, kAllowWord)) {
    return name.empty() ? WhisperBlockCommand{.kind = WhisperBlockCommandKind::kAllowWithoutName}
                        : WhisperBlockCommand{.kind = WhisperBlockCommandKind::kAllow,
                                              .name = std::string(name)};
  }
  return {};
}

}  // namespace moonclient
