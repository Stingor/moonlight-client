// Le maître d'un monstre invoqué. Voir l'en-tête.

#include "ro/net/unit_master.h"

#include "ro/proto/map_packets.h"
#include "ro/sim/summon_master.h"
#include "ro/sim/world.h"

#include "frame_apply.h"

namespace ro::net {

FrameOutcome ApplyUnitMasterFrame(const Frame& frame, sim::World& world) {
  if (frame.opcode != proto::ZcMoonlightUnitMaster::kOpcode) {
    return FrameOutcome::kUnhandled;
  }
  return Decoded<proto::ZcMoonlightUnitMaster>(frame, [&world](const auto& packet) {
    return sim::TellSummonMaster(world, packet.GID, packet.master_id) ? FrameOutcome::kApplied
                                                                      : FrameOutcome::kIgnored;
  });
}

}  // namespace ro::net
