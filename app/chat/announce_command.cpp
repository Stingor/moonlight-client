#include "chat/announce_command.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <format>

namespace moonclient {
namespace {

struct Spelling {
  std::string_view word;
  AnnounceCommandKind kind;
  bool named;
};

constexpr auto kSpellings = std::to_array<Spelling>({
    {.word = "/where", .kind = AnnounceCommandKind::kWhere, .named = false},
    {.word = "/b", .kind = AnnounceCommandKind::kBroadcast, .named = true},
    {.word = "/nb", .kind = AnnounceCommandKind::kBroadcast, .named = false},
    {.word = "/lb", .kind = AnnounceCommandKind::kLocalBroadcast, .named = true},
    {.word = "/nlb", .kind = AnnounceCommandKind::kLocalBroadcast, .named = false},
});

[[nodiscard]] bool SameWord(std::string_view typed, std::string_view word) {
  return std::ranges::equal(typed, word, [](char left, char right) {
    return std::tolower(static_cast<unsigned char>(left)) ==
           std::tolower(static_cast<unsigned char>(right));
  });
}

}  // namespace

AnnounceCommand ReadAnnounceCommand(std::string_view line) {
  const std::size_t space = line.find(' ');
  const std::string_view word = line.substr(0, space);
  const std::string_view rest =
      space == std::string_view::npos ? std::string_view() : line.substr(space + 1);
  for (const Spelling& spelling : kSpellings) {
    if (SameWord(word, spelling.word)) {
      return AnnounceCommand{.kind = spelling.kind,
                             .named = spelling.named,
                             .message = spelling.kind == AnnounceCommandKind::kWhere
                                            ? std::string()
                                            : std::string(rest)};
    }
  }
  return {};
}

std::string AnnouncedText(const AnnounceCommand& command, std::string_view speaker) {
  if (command.message.empty()) {
    return {};
  }
  return command.named ? std::format("{} : {}", speaker, command.message) : command.message;
}

std::string WhereLine(std::string_view map_name, int x, int y) {
  return std::format("{} {} {}", map_name, x, y);
}

}  // namespace moonclient
