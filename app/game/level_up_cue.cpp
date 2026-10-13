#include "game/level_up_cue.h"

#include "ro/proto/map_packets.h"
#include "ro/sim/level_up_effect.h"
#include "ro/sim/player_status.h"

namespace moonclient {

std::optional<std::string_view> CueLevelUp(const ro::net::Frame& frame, ro::sim::World& world,
                                           std::uint32_t now_ms) {
  if (frame.opcode != ro::proto::ZcParChange::kOpcode) {
    return std::nullopt;
  }
  ro::proto::ZcParChange packet;
  if (!ro::proto::Decode(frame.bytes, packet)) {
    return std::nullopt;
  }
  const ro::sim::Actor* player = world.player();
  if (player == nullptr) {
    return std::nullopt;
  }
  switch (static_cast<ro::sim::ServerParameter>(packet.varID)) {
    case ro::sim::ServerParameter::kBaseLevel:
      static_cast<void>(world.RecordEffect(world.player_id(),
                                           ro::sim::BaseLevelUpEffectId(player->look.job), now_ms));
      return ro::sim::kBaseLevelUpSound;
    case ro::sim::ServerParameter::kJobLevel:
      static_cast<void>(
          world.RecordEffect(world.player_id(), ro::sim::JobLevelUpEffectId(packet.count), now_ms));
      return std::nullopt;
    default:
      return std::nullopt;
  }
}

}  // namespace moonclient
