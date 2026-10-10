// Un Flag Graffiti posé au sol. Voir l'en-tête.

#include "ro/net/flag_graffiti.h"

#include "ro/proto/map_packets.h"
#include "ro/sim/actor.h"
#include "ro/sim/world.h"

#include "frame_apply.h"

namespace ro::net {

FrameOutcome ApplyFlagGraffitiFrame(const Frame& frame, sim::World& world, std::uint32_t now_ms) {
  if (frame.opcode != proto::ZcMoonlightFlagGraffiti::kOpcode) {
    return FrameOutcome::kUnhandled;
  }
  return Decoded<proto::ZcMoonlightFlagGraffiti>(frame, [&world, now_ms](const auto& packet) {
    sim::SkillUnit unit;
    unit.id = packet.unit_id;
    unit.creator_id = packet.creator_id;
    unit.cell = sim::CellPosition{static_cast<std::uint16_t>(packet.x),
                                  static_cast<std::uint16_t>(packet.y)};
    unit.unit_id = sim::kFlagGraffitiUnit;
    unit.placed_unit_id = sim::kFlagGraffitiUnit;
    unit.skill_level = packet.level;
    unit.placed_ms = now_ms;
    unit.look_since_ms = now_ms;
    unit.emblem_guild_id = static_cast<std::int32_t>(packet.guild_id);
    unit.emblem_version = static_cast<std::int32_t>(packet.emblem_id);
    world.PlaceSkillUnit(unit);
    return FrameOutcome::kApplied;
  });
}

}  // namespace ro::net
