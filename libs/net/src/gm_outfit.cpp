// L'habit du GM d'un joueur. Voir l'en-tête.

#include "ro/net/gm_outfit.h"

#include <vector>

#include "ro/proto/map_packets.h"
#include "ro/sim/world.h"

#include "frame_apply.h"

namespace ro::net {

FrameOutcome ApplyGmOutfitFrame(const Frame& frame, sim::World& world) {
  if (frame.opcode != proto::ZcMoonlightGmOutfit::kOpcode) {
    return FrameOutcome::kUnhandled;
  }
  return Decoded<proto::ZcMoonlightGmOutfit>(frame, [&world](const auto& packet) {
    return world.TellGmOutfit(packet.GID, packet.worn != 0) ? FrameOutcome::kApplied
                                                            : FrameOutcome::kIgnored;
  });
}

core::Result<void> SendGmOutfit(Connection& link, bool worn) {
  proto::CzMoonlightGmOutfit request;
  request.packetLength = static_cast<std::int16_t>(proto::CzMoonlightGmOutfit::kWireSize);
  request.worn = worn ? 1 : 0;
  const std::vector<std::uint8_t> bytes = proto::Encode(request);
  if (bytes.empty()) {
    return core::Fail(core::ErrorCode::kInvalidArgument, "habit du GM non encodable");
  }
  link.Send(bytes);
  return {};
}

}  // namespace ro::net
