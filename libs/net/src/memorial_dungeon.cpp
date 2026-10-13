// L'instance du joueur. Voir l'en-tête.

#include "ro/net/memorial_dungeon.h"

#include <utility>

#include "ro/core/text.h"
#include "ro/proto/map_packets.h"
#include "ro/proto/memorial_packets.h"

#include "frame_apply.h"

namespace ro::net {
namespace {

// LES TYPES DE `ZC 0x02CE` (`e_instance_notify` du serveur).
constexpr std::uint32_t kWireNotify = 0;
constexpr std::uint32_t kWireLiveTimeout = 1;
constexpr std::uint32_t kWireEnterTimeout = 2;
constexpr std::uint32_t kWireDestroyed = 3;
constexpr std::uint32_t kWireCreateFailed = 4;

// Le plus grand mode que le serveur écrive (`IM_CLAN`) ; au-delà, inconnu.
constexpr std::uint8_t kLastOwnerKind = static_cast<std::uint8_t>(sim::MemorialOwnerKind::kClan);

[[nodiscard]] FrameOutcome ApplyChange(const proto::InstanceChangeNotice& packet,
                                       sim::MemorialDungeon& dungeon) {
  switch (packet.type) {
    case kWireNotify:
      dungeon.TellIdleLimit(packet.limit_unix_s);
      return FrameOutcome::kApplied;
    case kWireLiveTimeout:
      dungeon.TellEnded(sim::MemorialEnding::kExpired);
      return FrameOutcome::kApplied;
    case kWireEnterTimeout:
      dungeon.TellEnded(sim::MemorialEnding::kEntryExpired);
      return FrameOutcome::kApplied;
    case kWireDestroyed:
      dungeon.TellEnded(sim::MemorialEnding::kDestroyed);
      return FrameOutcome::kApplied;
    case kWireCreateFailed:
      // La fenêtre se ferme sans un mot, comme dans le jeu.
      dungeon.Reset();
      return FrameOutcome::kApplied;
    default:
      return FrameOutcome::kIgnored;
  }
}

[[nodiscard]] sim::MemorialDetails DetailsOf(const proto::ZcMoonlightInstanceInfo& packet) {
  sim::MemorialDetails details;
  details.duration_limit_s = packet.duration_limit;
  details.idle_timeout_s = packet.idle_timeout;
  details.hard_mode = packet.hardmode;
  details.turbo = packet.turbo != 0;
  details.owner_kind = packet.mode <= kLastOwnerKind
                           ? static_cast<sim::MemorialOwnerKind>(packet.mode)
                           : sim::MemorialOwnerKind::kUnknown;
  details.owner_name = core::Utf8FromCp1252(packet.owner_name);
  details.destroyable = packet.destroyable != 0;
  details.may_destroy = packet.may_destroy != 0;
  details.on_instance_map = packet.on_instance_map != 0;
  details.players_inside = packet.players_inside;
  return details;
}

}  // namespace

FrameOutcome ApplyMemorialDungeonFrame(const Frame& frame, sim::MemorialDungeon& dungeon,
                                       std::int64_t now_unix_s) {
  switch (frame.opcode) {
    case proto::InstanceCreatedNotice::kOpcode:
      return Decoded<proto::InstanceCreatedNotice>(frame, [&dungeon](const auto& packet) {
        dungeon.TellCreated(core::Utf8FromCp1252(packet.name), packet.wait_position);
        return FrameOutcome::kApplied;
      });
    case proto::InstanceWaitNotice::kOpcode:
      return Decoded<proto::InstanceWaitNotice>(frame, [&dungeon](const auto& packet) {
        if (packet.wait_position == proto::InstanceWaitNotice::kCancelled) {
          dungeon.TellReservationCancelled();
        } else {
          dungeon.TellWaitPosition(packet.wait_position);
        }
        return FrameOutcome::kApplied;
      });
    case proto::InstanceStatusNotice::kOpcode:
      return Decoded<proto::InstanceStatusNotice>(frame, [&dungeon](const auto& packet) {
        dungeon.TellOpen(core::Utf8FromCp1252(packet.name), packet.keep_limit_unix_s,
                         packet.idle_limit_unix_s);
        return FrameOutcome::kApplied;
      });
    case proto::InstanceChangeNotice::kOpcode:
      return Decoded<proto::InstanceChangeNotice>(
          frame, [&dungeon](const auto& packet) { return ApplyChange(packet, dungeon); });
    case proto::ZcMoonlightInstanceInfo::kOpcode:
      return Decoded<proto::ZcMoonlightInstanceInfo>(
          frame, [&dungeon, now_unix_s](const auto& packet) {
            dungeon.TellDetails(DetailsOf(packet), packet.server_time, now_unix_s);
            return FrameOutcome::kApplied;
          });
    default:
      return FrameOutcome::kUnhandled;
  }
}

void RequestMemorialDungeonDestroy(Connection& link) {
  link.Send(proto::EncodeInstanceDestroyCommand());
}

}  // namespace ro::net
