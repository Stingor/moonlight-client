#include "ro/proto/chat_author_packets.h"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <utility>

#include "ro/core/byte_writer.h"

namespace ro::proto {
namespace {

// Où la longueur d'une trame variable s'écrit : juste après l'opcode.
constexpr std::size_t kLengthOffset = 2;

[[nodiscard]] bool KnownScope(std::uint8_t scope) {
  return scope >= static_cast<std::uint8_t>(ChatAuthorScope::kPublic) &&
         scope <= static_cast<std::uint8_t>(ChatAuthorScope::kChannel);
}

}  // namespace

bool Decode(std::span<const std::uint8_t> frame, ChatAuthorNotice& notice) {
  ChatAuthorNotice read;
  if (!Decode(frame, read.fixed) || !KnownScope(read.fixed.scope) || read.fixed.name.empty()) {
    return false;
  }
  const std::span<const std::uint8_t> tail = frame.subspan(ZcMoonlightChatAuthor::kWireSize);
  const bool channel = read.fixed.scope == static_cast<std::uint8_t>(ChatAuthorScope::kChannel);
  // ⚠ HORS D'UN CANAL, LA TRAME S'ARRÊTE AU BLOC FIXE : une queue y dirait que
  // la disposition a changé sans nous.
  if (!channel && !tail.empty()) {
    return false;
  }
  if (std::ranges::find(tail, std::uint8_t{0}) != tail.end()) {
    return false;
  }
  read.channel.assign(tail.begin(), tail.end());
  notice = std::move(read);
  return true;
}

std::vector<std::uint8_t> Encode(const ChatAuthorNotice& notice) {
  std::vector<std::uint8_t> frame = Encode(notice.fixed);
  core::ByteWriter writer(frame);
  writer.Text(notice.channel);
  if (frame.size() > std::numeric_limits<std::uint16_t>::max() ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(frame.size()))) {
    return {};
  }
  return frame;
}

}  // namespace ro::proto
