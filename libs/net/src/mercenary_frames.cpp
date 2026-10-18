// Voir l'en-tête.

#include "ro/net/mercenary_frames.h"

#include <optional>
#include <utility>
#include <vector>

#include "ro/core/text.h"
#include "ro/proto/map_packets.h"
#include "ro/proto/mercenary_packets.h"

#include "frame_apply.h"
#include "send_encoded.h"

namespace ro::net {
namespace {

// Les deux messages du client par lesquels le serveur annonce son départ.
constexpr std::uint16_t kContractEndedMessage = 1266;
constexpr std::uint16_t kDiedMessage = 1267;

// Le genre de disparition d'un acteur MORT.
constexpr std::uint8_t kVanishedDead = 1;

std::optional<sim::MercenaryStat> StatOfVariable(std::uint16_t variable) {
  switch (variable) {
    case proto::kMercenaryVarHp:
      return sim::MercenaryStat::kHp;
    case proto::kMercenaryVarMaxHp:
      return sim::MercenaryStat::kMaxHp;
    case proto::kMercenaryVarSp:
      return sim::MercenaryStat::kSp;
    case proto::kMercenaryVarMaxSp:
      return sim::MercenaryStat::kMaxSp;
    case proto::kMercenaryVarAtk:
      return sim::MercenaryStat::kAtk;
    case proto::kMercenaryVarMatk:
      return sim::MercenaryStat::kMatk;
    case proto::kMercenaryVarHit:
      return sim::MercenaryStat::kHit;
    case proto::kMercenaryVarCrit:
      return sim::MercenaryStat::kCrit;
    case proto::kMercenaryVarDef:
      return sim::MercenaryStat::kDef;
    case proto::kMercenaryVarMdef:
      return sim::MercenaryStat::kMdef;
    case proto::kMercenaryVarFlee:
      return sim::MercenaryStat::kFlee;
    case proto::kMercenaryVarAmotion:
      return sim::MercenaryStat::kAmotion;
    case proto::kMercenaryVarKills:
      return sim::MercenaryStat::kKills;
    case proto::kMercenaryVarFaith:
      return sim::MercenaryStat::kFaith;
    default:
      return std::nullopt;
  }
}

sim::MercenaryProfile ProfileOf(const proto::MercenaryInfo& info) {
  return sim::MercenaryProfile{
      .actor_id = info.actor_id,
      // Le nom vient de la base du serveur, dans la page de codes du jeu.
      .name = core::Utf8FromCp1252(info.name),
      .level = info.level,
      .hp = info.hp,
      .max_hp = info.max_hp,
      .sp = info.sp,
      .max_sp = info.max_sp,
      .atk = info.atk,
      .matk = info.matk,
      .hit = info.hit,
      .crit = info.crit,
      .def = info.def,
      .mdef = info.mdef,
      .flee = info.flee,
      .amotion_ms = info.amotion_ms,
      .range_cells = info.range_cells,
      .faith = info.faith,
      .calls = info.calls,
      .kills = info.kills,
      .expire_unix_s = info.expire_unix_s,
  };
}

FrameOutcome ApplyMessage(const Frame& frame, sim::MercenaryBook& book) {
  return Decoded<proto::ZcMsg>(frame, [&book](const proto::ZcMsg& packet) {
    if (packet.msgId == kContractEndedMessage) {
      book.Part(sim::MercenaryFarewell::kContractEnded);
      return FrameOutcome::kApplied;
    }
    if (packet.msgId == kDiedMessage) {
      book.Part(sim::MercenaryFarewell::kDied);
      return FrameOutcome::kApplied;
    }
    return FrameOutcome::kUnhandled;
  });
}

// ⚠ LA DISPARITION APPARTIENT AU MONDE, qui la traite aussi : on la regarde
// sans la prendre.
FrameOutcome ApplyVanish(const Frame& frame, sim::MercenaryBook& book) {
  proto::ZcNotifyVanish packet;
  if (book.present() && proto::Decode(frame.bytes, packet) && packet.type == kVanishedDead &&
      packet.gid == book.profile().actor_id) {
    book.Part(sim::MercenaryFarewell::kNone);
  }
  return FrameOutcome::kUnhandled;
}

core::Result<void> SendCommand(Connection& link, std::uint8_t command) {
  return SendEncoded(link,
                     proto::EncodeMercenaryCommand(proto::MercenaryCommand{.command = command}));
}

}  // namespace

FrameOutcome ApplyMercenaryFrame(const Frame& frame, sim::MercenaryBook& book,
                                 MercenaryClock clock) {
  switch (frame.opcode) {
    case proto::MercenaryInfo::kOpcode: {
      const auto info = proto::DecodeMercenaryInfo(frame.bytes);
      if (!info) {
        return FrameOutcome::kUndecodable;
      }
      book.Meet(ProfileOf(*info), clock.now_ms, clock.now_unix_s);
      return FrameOutcome::kApplied;
    }
    case proto::MercenaryStatus::kOpcode: {
      const auto status = proto::DecodeMercenaryStatus(frame.bytes);
      if (!status) {
        return FrameOutcome::kUndecodable;
      }
      const std::optional<sim::MercenaryStat> stat = StatOfVariable(status->variable);
      return stat.has_value() && book.Change(*stat, status->value) ? FrameOutcome::kApplied
                                                                   : FrameOutcome::kIgnored;
    }
    case proto::MercenarySkillList::kOpcode: {
      auto list = proto::DecodeMercenarySkillList(frame.bytes);
      if (!list) {
        return FrameOutcome::kUndecodable;
      }
      std::vector<sim::MercenarySkill> skills;
      skills.reserve(list->skills.size());
      for (proto::MercenarySkillEntry& entry : list->skills) {
        skills.push_back(sim::MercenarySkill{
            .skill_id = entry.skill_id,
            .targeting = static_cast<std::uint16_t>(entry.targeting),
            .level = entry.level,
            .sp_cost = entry.sp_cost,
            .range_cells = entry.range_cells,
            .name = std::move(entry.name),
        });
      }
      book.SetSkills(std::move(skills));
      return FrameOutcome::kApplied;
    }
    case proto::ZcMsg::kOpcode:
      return ApplyMessage(frame, book);
    case proto::ZcNotifyVanish::kOpcode:
      return ApplyVanish(frame, book);
    default:
      return FrameOutcome::kUnhandled;
  }
}

core::Result<void> RequestMercenaryInfo(Connection& link) {
  return SendCommand(link, proto::kMercenaryCommandInfo);
}

core::Result<void> RequestMercenaryDismiss(Connection& link) {
  return SendCommand(link, proto::kMercenaryCommandDismiss);
}

}  // namespace ro::net
