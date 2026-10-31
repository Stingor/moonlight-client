// Le saut cosmétique. Voir l'en-tête.

#include "ro/net/jump.h"

#include "ro/proto/map_packets.h"
#include "ro/sim/jump.h"
#include "ro/sim/world.h"

#include "frame_apply.h"

namespace ro::net {

core::Result<void> JumpPlayer(Connection& link, sim::World& world, std::uint32_t now_ms) {
  const sim::Actor* self = world.player();
  if (self == nullptr || sim::Airborne(*self, now_ms)) {
    return {};
  }
  proto::CzMoonlightJump request;
  request.packetLength = static_cast<std::int16_t>(proto::CzMoonlightJump::kWireSize);
  link.Send(proto::Encode(request));
  static_cast<void>(world.RecordJump(world.player_id(), now_ms));
  return {};
}

FrameOutcome ApplyJumpFrame(const Frame& frame, sim::World& world, std::uint32_t now_ms) {
  if (frame.opcode != proto::ZcMoonlightJump::kOpcode) {
    return FrameOutcome::kUnhandled;
  }
  return Decoded<proto::ZcMoonlightJump>(frame, [&world, now_ms](const auto& packet) {
    if (packet.gid == world.player_id()) {
      return FrameOutcome::kIgnored;
    }
    return world.RecordJump(packet.gid, now_ms) ? FrameOutcome::kApplied : FrameOutcome::kIgnored;
  });
}

}  // namespace ro::net
