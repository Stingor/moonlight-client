// La demande de mémorisation, et ses réponses. Voir l'en-tête.

#include "ro/net/warp_memo.h"

#include <utility>

#include "ro/proto/manual_packets.h"
#include "ro/proto/map_packets.h"

#include "send_encoded.h"

namespace ro::net {

core::Result<void> RequestWarpMemo(Connection& link) {
  return SendEncoded(link, proto::Encode(proto::RememberWarpPointRequest{}));
}

std::optional<std::uint8_t> ReadWarpMemoAck(const Frame& frame) {
  if (frame.opcode != proto::ZcAckRememberWarppoint::kOpcode) {
    return std::nullopt;
  }
  proto::ZcAckRememberWarppoint packet;
  if (!proto::Decode(frame.bytes, packet)) {
    return std::nullopt;
  }
  return packet.type;
}

std::optional<std::int16_t> ReadMapInfoNotice(const Frame& frame) {
  if (frame.opcode != proto::ZcNotifyMapinfo::kOpcode) {
    return std::nullopt;
  }
  proto::ZcNotifyMapinfo packet;
  if (!proto::Decode(frame.bytes, packet)) {
    return std::nullopt;
  }
  return packet.type;
}

}  // namespace ro::net
