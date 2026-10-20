// L'élément d'attaque d'un joueur. Voir l'en-tête.

#include "ro/net/attack_element.h"

#include "ro/proto/map_packets.h"
#include "ro/sim/world.h"

#include "frame_apply.h"

namespace ro::net {

FrameOutcome ApplyAttackElementFrame(const Frame& frame, sim::World& world) {
  if (frame.opcode != proto::ZcMoonlightAttackElement::kOpcode) {
    return FrameOutcome::kUnhandled;
  }
  return Decoded<proto::ZcMoonlightAttackElement>(frame, [&world](const auto& packet) {
    return world.TellAttackElement(packet.GID, sim::ElementOfWire(packet.element))
               ? FrameOutcome::kApplied
               : FrameOutcome::kIgnored;
  });
}

}  // namespace ro::net
