// La classe d'un PNJ ou d'un monstre qui change. Voir l'en-tête.

#include "ro/net/class_change.h"

#include "ro/proto/map_packets.h"
#include "ro/sim/world.h"

#include "frame_apply.h"

namespace ro::net {

FrameOutcome ApplyClassChangeFrame(const Frame& frame, sim::World& world) {
  if (frame.opcode != proto::ZcNpcspriteChange::kOpcode) {
    return FrameOutcome::kUnhandled;
  }
  return Decoded<proto::ZcNpcspriteChange>(frame, [&world](const auto& packet) {
    const sim::Actor* actor = world.Find(packet.GID);
    if (actor == nullptr || actor->kind == sim::ActorKind::kPlayer) {
      return FrameOutcome::kIgnored;
    }
    sim::Appearance look = actor->look;
    look.job = static_cast<std::uint16_t>(packet.class_);
    look.sex = 0;
    return world.SetLook(packet.GID, look) ? FrameOutcome::kApplied : FrameOutcome::kIgnored;
  });
}

}  // namespace ro::net
