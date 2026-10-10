#include "ro/net/npc_illustration.h"

#include <optional>

#include "ro/proto/map_packets.h"

#include "frame_apply.h"

namespace ro::net {

FrameOutcome ApplyNpcIllustrationFrame(const Frame& frame, sim::NpcIllustrationSlot& slot) {
  if (frame.opcode != proto::ZcShowImage::kOpcode) {
    return FrameOutcome::kUnhandled;
  }
  return Decoded<proto::ZcShowImage>(frame, [&slot](const auto& packet) {
    const std::optional<sim::IllustrationPlace> place = sim::IllustrationPlaceOf(packet.type);
    if (!place.has_value() || packet.image.empty()) {
      slot.Clear();
    } else {
      slot.Show(sim::NpcIllustration{.image = packet.image, .place = *place});
    }
    return FrameOutcome::kApplied;
  });
}

}  // namespace ro::net
