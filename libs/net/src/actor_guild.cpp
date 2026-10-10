#include "ro/net/actor_guild.h"

#include <algorithm>
#include <cstdint>
#include <utility>

#include "ro/proto/map_packets.h"
#include "ro/sim/world.h"

namespace ro::net {

FrameOutcome ApplyActorGuildFrame(const Frame& frame, sim::World& world) {
  if (frame.opcode != proto::ZcChangeGuild::kOpcode) {
    return FrameOutcome::kUnhandled;
  }
  proto::ZcChangeGuild packet;
  if (!proto::Decode(frame.bytes, packet)) {
    return FrameOutcome::kUndecodable;
  }
  const sim::Actor* found = world.Find(packet.AID);
  if (found == nullptr) {
    return FrameOutcome::kIgnored;
  }
  // RÉANNONCÉ, COMME LE SERVEUR LE FAIT À CHAQUE PAS : `Spawn` garde ce que les
  // autres paquets ont dit — vie, carte de nom, effets, enseigne —, et seule la
  // guilde change ici.
  sim::Actor again = *found;
  again.guild_id = static_cast<std::uint32_t>(std::max(packet.guild_id, 0));
  world.Spawn(std::move(again));
  return FrameOutcome::kApplied;
}

}  // namespace ro::net
