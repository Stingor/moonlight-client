#include "ro/net/zone_move.h"

#include "ro/net/map_name.h"
#include "ro/proto/map_packets.h"

namespace ro::net {

std::optional<CharacterPhase::Handoff> ServerMoveOf(const Frame& frame,
                                                    std::uint32_t character_id) {
  if (frame.opcode != proto::ZcNpcackServermove::kOpcode) {
    return std::nullopt;
  }
  proto::ZcNpcackServermove packet;
  if (!proto::Decode(frame.bytes, packet)) {
    return std::nullopt;
  }
  return CharacterPhase::Handoff{.character_id = character_id,
                                 .map_name = MapBaseName(packet.mapName),
                                 .address = ServerAddress{packet.ip, packet.port}};
}

}  // namespace ro::net
