// Voir l'en-tête.

#include "ro/net/homunculus_frames.h"

#include <utility>
#include <vector>

#include "ro/core/text.h"
#include "ro/proto/config_packets.h"
#include "ro/proto/homunculus_packets.h"
#include "ro/proto/map_packets.h"

#include "frame_apply.h"
#include "send_encoded.h"

namespace ro::net {
namespace {

// Les états du changement d'état, numérotés comme le serveur.
constexpr std::uint8_t kStateAcknowledged = 0;
constexpr std::uint8_t kStateIntimacy = 1;
constexpr std::uint8_t kStateHunger = 2;

[[nodiscard]] sim::HomunculusSheet SheetOf(const proto::HomunculusProperties& wire) {
  return sim::HomunculusSheet{
      .name = core::Utf8FromWire(wire.name),
      .renamed = (wire.flags & proto::kHomunculusRenamedFlag) != 0,
      .resting = (wire.flags & proto::kHomunculusRestingFlag) != 0,
      .alive = (wire.flags & proto::kHomunculusAliveFlag) != 0,
      .level = wire.level,
      .hunger = wire.hunger,
      .intimacy = wire.intimacy,
      .atk = wire.atk,
      .matk = wire.matk,
      .hit = wire.hit,
      .crit = wire.crit,
      .def = wire.def,
      .mdef = wire.mdef,
      .flee = wire.flee,
      .amotion_ms = wire.amotion_ms,
      .hp = wire.hp,
      .max_hp = wire.max_hp,
      .sp = wire.sp,
      .max_sp = wire.max_sp,
      .exp = wire.exp,
      .exp_next = wire.exp_next,
      .skill_points = wire.skill_points,
      .attack_range_cells = wire.attack_range_cells,
  };
}

// ⚠ HP ET SP NE SE LISENT QUE SUR LEURS TRENTE-DEUX BITS BAS ; l'expérience,
// entière.
FrameOutcome ApplyParameter(const proto::HomunculusParameter& changed,
                            sim::Homunculus& homunculus) {
  const auto low = static_cast<std::uint32_t>(changed.value);
  switch (changed.type) {
    case proto::kHomunculusParameterExp:
      homunculus.SetExp(static_cast<std::int64_t>(changed.value));
      return FrameOutcome::kApplied;
    case proto::kHomunculusParameterHp:
      homunculus.SetHp(low);
      return FrameOutcome::kApplied;
    case proto::kHomunculusParameterSp:
      homunculus.SetSp(low);
      return FrameOutcome::kApplied;
    default:
      return FrameOutcome::kIgnored;
  }
}

FrameOutcome ApplyState(const proto::ZcChangestateMer& changed, sim::Homunculus& homunculus) {
  switch (changed.state) {
    case kStateAcknowledged:
      homunculus.Acknowledge(changed.gid);
      return FrameOutcome::kApplied;
    case kStateIntimacy:
      homunculus.SetIntimacy(static_cast<int>(changed.data));
      return FrameOutcome::kApplied;
    case kStateHunger:
      homunculus.SetHunger(static_cast<int>(changed.data));
      return FrameOutcome::kApplied;
    default:
      // L'accessoire d'autrefois : le serveur ne l'envoie plus.
      return FrameOutcome::kIgnored;
  }
}

[[nodiscard]] core::Result<void> Command(Connection& link, proto::HomunculusCommand command) {
  return SendEncoded(link, proto::Encode(proto::HomunculusCommandRequest{.command = command}));
}

}  // namespace

FrameOutcome ApplyHomunculusFrame(const Frame& frame, sim::Homunculus& homunculus,
                                  HomunculusEcho& echo) {
  echo = HomunculusEcho{};
  switch (frame.opcode) {
    case proto::HomunculusProperties::kOpcode: {
      const auto properties = proto::DecodeHomunculusProperties(frame.bytes);
      if (!properties) {
        return FrameOutcome::kUndecodable;
      }
      if (homunculus.TakeSheet(SheetOf(*properties))) {
        echo.kind = HomunculusEchoKind::kStarving;
      }
      return FrameOutcome::kApplied;
    }
    case proto::HomunculusParameter::kOpcode: {
      const auto changed = proto::DecodeHomunculusParameter(frame.bytes);
      return changed ? ApplyParameter(*changed, homunculus) : FrameOutcome::kUndecodable;
    }
    case proto::ZcChangestateMer::kOpcode:
      return Decoded<proto::ZcChangestateMer>(
          frame, [&homunculus](const auto& packet) { return ApplyState(packet, homunculus); });
    case proto::ZcHoskillinfoList::kOpcode:
      return Decoded<proto::ZcHoskillinfoList>(frame, [&homunculus](const auto& packet) {
        std::vector<sim::HomunculusSkill> skills;
        skills.reserve(packet.skills.size());
        for (const proto::ZcHoskillinfoListSub& entry : packet.skills) {
          skills.push_back(sim::HomunculusSkill{.skill_id = entry.id,
                                                .targeting = entry.inf,
                                                .level = entry.level,
                                                .sp_cost = entry.sp,
                                                .range_cells = entry.range,
                                                .upgradable = entry.upgradable != 0,
                                                .server_name = entry.name});
        }
        homunculus.TakeSkills(std::move(skills));
        return FrameOutcome::kApplied;
      });
    case proto::ZcHoskillinfoUpdate::kOpcode:
      return Decoded<proto::ZcHoskillinfoUpdate>(frame, [&homunculus](const auto& packet) {
        homunculus.UpdateSkill(sim::HomunculusSkill{.skill_id = packet.skill_id,
                                                    .level = packet.Level,
                                                    .sp_cost = packet.SP,
                                                    .range_cells = packet.AttackRange,
                                                    .upgradable = packet.upgradable});
        return FrameOutcome::kApplied;
      });
    case proto::ZcFeedMer::kOpcode:
      return Decoded<proto::ZcFeedMer>(frame, [&echo](const auto& packet) {
        echo.kind = packet.result != 0 ? HomunculusEchoKind::kFed : HomunculusEchoKind::kNotFed;
        echo.item_id = packet.itemId;
        return FrameOutcome::kIgnored;
      });
    case proto::ConfigAck::kOpcode: {
      const auto ack = proto::DecodeConfigAck(frame.bytes);
      if (!ack) {
        return FrameOutcome::kUndecodable;
      }
      if (ack->type != static_cast<std::uint32_t>(proto::ConfigType::kHomunculusAutofeed)) {
        return FrameOutcome::kUnhandled;
      }
      homunculus.SetAutoFeed(ack->value != 0);
      echo.kind =
          ack->value != 0 ? HomunculusEchoKind::kAutoFeedOn : HomunculusEchoKind::kAutoFeedOff;
      return FrameOutcome::kApplied;
    }
    default:
      return FrameOutcome::kUnhandled;
  }
}

core::Result<void> RequestHomunculusFeed(Connection& link) {
  return Command(link, proto::HomunculusCommand::kFeed);
}

core::Result<void> RequestHomunculusDelete(Connection& link) {
  return Command(link, proto::HomunculusCommand::kDelete);
}

core::Result<void> RequestHomunculusRename(Connection& link, std::string_view name) {
  return SendEncoded(
      link, proto::Encode(proto::HomunculusRenameRequest{.name = core::Cp1252FromUtf8(name)}));
}

core::Result<void> RequestHomunculusAutoFeed(Connection& link, bool on) {
  return SendEncoded(
      link, proto::Encode(proto::ConfigRequest{.type = proto::ConfigType::kHomunculusAutofeed,
                                               .value = on ? 1U : 0U}));
}

}  // namespace ro::net
