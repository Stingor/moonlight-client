#include "chat/chat_mention_piece.h"

#include "ro/net/chat_target.h"

namespace moonclient {

std::string MentionPiece(std::string_view before, std::string_view name) {
  std::string piece;
  if (before.find_first_not_of(' ') != std::string_view::npos) {
    if (before.back() != ' ') {
      piece += ' ';
    }
    piece += ro::net::kAtCommandSymbol;
  }
  piece += name;
  piece += ' ';
  return piece;
}

}  // namespace moonclient
