// Le message d'une Talkie Box. Voir l'en-tête.

#include "ro/net/talkbox.h"

#include "ro/core/log.h"
#include "ro/core/text.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/world.h"

#include "frame_apply.h"

namespace ro::net {

FrameOutcome ApplyTalkboxFrame(const Frame& frame, sim::World& world, std::uint32_t now_ms) {
  if (frame.opcode != proto::ZcTalkboxChatcontents::kOpcode) {
    return FrameOutcome::kUnhandled;
  }
  return Decoded<proto::ZcTalkboxChatcontents>(frame, [&world, now_ms](const auto& packet) {
    if (world.SaySkillUnit(packet.aid, core::Utf8FromWire(packet.message), now_ms)) {
      return FrameOutcome::kApplied;
    }
    // Une boîte qu'on ne voit pas : le serveur diffuse en zone, et l'unité peut
    // être hors de la scène.
    core::LogDebug("Talkie Box {} : unité inconnue, le message ne paraîtra pas", packet.aid);
    return FrameOutcome::kIgnored;
  });
}

}  // namespace ro::net
