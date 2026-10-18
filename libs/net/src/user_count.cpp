// La question du nombre de joueurs connectés, et sa réponse. Voir l'en-tête.

#include "ro/net/user_count.h"

#include <utility>

#include "ro/proto/map_packets.h"
#include "ro/proto/user_count_packet.h"

#include "send_encoded.h"

namespace ro::net {

core::Result<void> RequestUserCount(Connection& link) {
  return SendEncoded(link, proto::EncodeUserCountRequest());
}

std::optional<std::int32_t> ReadUserCount(const Frame& frame) {
  if (frame.opcode != proto::ZcUserCount::kOpcode) {
    return std::nullopt;
  }
  proto::ZcUserCount packet;
  if (!proto::Decode(frame.bytes, packet)) {
    return std::nullopt;
  }
  return packet.playersCount;
}

}  // namespace ro::net
