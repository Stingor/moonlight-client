// Le journal des blocages de chat : la demande et la réponse. Voir l'en-tête.

#include "ro/net/chat_blocks.h"

#include "ro/core/text.h"
#include "ro/proto/map_packets.h"

namespace ro::net {
namespace {

static_assert(kChatBlocksReplyOpcode == proto::ZcMoonlightChatBlocks::kOpcode);

[[nodiscard]] ChatBlocksStatus StatusOf(std::uint8_t wire) {
  switch (wire) {
    case static_cast<std::uint8_t>(ChatBlocksStatus::kFound):
      return ChatBlocksStatus::kFound;
    case static_cast<std::uint8_t>(ChatBlocksStatus::kMissing):
      return ChatBlocksStatus::kMissing;
    case static_cast<std::uint8_t>(ChatBlocksStatus::kDenied):
      return ChatBlocksStatus::kDenied;
    case static_cast<std::uint8_t>(ChatBlocksStatus::kLogOff):
      return ChatBlocksStatus::kLogOff;
    default:
      break;
  }
  return ChatBlocksStatus::kReadFailed;
}

[[nodiscard]] std::optional<ChatBlockKind> KindOf(std::uint8_t wire) {
  switch (wire) {
    case static_cast<std::uint8_t>(ChatBlockKind::kMute):
      return ChatBlockKind::kMute;
    case static_cast<std::uint8_t>(ChatBlockKind::kUnmute):
      return ChatBlockKind::kUnmute;
    default:
      break;
  }
  return std::nullopt;
}

}  // namespace

core::Result<void> RequestChatBlocks(Connection& link, std::uint32_t aid) {
  proto::CzMoonlightReqChatBlocks request;
  // ⚠ LA TAILLE EST ÉCRITE, le serveur la LIT.
  request.packetLength = static_cast<std::int16_t>(proto::CzMoonlightReqChatBlocks::kWireSize);
  request.aid = aid;
  link.Send(proto::Encode(request));
  return {};
}

std::optional<ChatBlocksReply> DecodeChatBlocks(std::span<const std::uint8_t> frame) {
  proto::ZcMoonlightChatBlocks packet;
  if (!proto::Decode(frame, packet)) {
    return std::nullopt;
  }
  ChatBlocksReply reply;
  reply.aid = packet.aid;
  reply.status = StatusOf(packet.status);
  if (reply.status != ChatBlocksStatus::kFound) {
    return reply;
  }
  reply.blocks.reserve(packet.entries.size());
  for (const proto::MoonlightChatBlock& entry : packet.entries) {
    const std::optional<ChatBlockKind> kind = KindOf(entry.kind);
    if (!kind.has_value()) {
      continue;
    }
    // Le serveur écrit le nom dans la page de codes du jeu.
    reply.blocks.push_back(ChatBlock{.date_s = entry.date,
                                     .minutes = entry.minutes,
                                     .kind = *kind,
                                     .author = core::Utf8FromWire(entry.author)});
  }
  return reply;
}

std::optional<ChatBlocksReply> ReadChatBlocks(const Frame& frame) {
  if (frame.opcode != proto::ZcMoonlightChatBlocks::kOpcode) {
    return std::nullopt;
  }
  return DecodeChatBlocks(frame.bytes);
}

}  // namespace ro::net
