#include "ro/net/member_statuses.h"

#include <algorithm>
#include <cstddef>
#include <utility>

#include "ro/core/byte_reader.h"
#include "ro/net/target_info.h"
#include "ro/proto/map_packets.h"

namespace ro::net {
namespace {

[[nodiscard]] bool IsMember(const sim::PartyRoster& party, std::uint32_t account_id,
                            std::uint32_t own_account_id) {
  return account_id != 0 && account_id != own_account_id && party.Find(account_id) != nullptr;
}

// Une transition annoncée à la zone. `remaining_ms` et `total_ms` restent nuls
// pour les paquets qui ne portent pas de durée.
struct Transition {
  std::uint32_t account_id = 0;
  sim::StatusEffectId effect = 0;
  bool active = false;
  std::uint32_t remaining_ms = 0;
  std::uint32_t total_ms = 0;
};

[[nodiscard]] std::optional<Transition> TransitionOf(const Frame& frame) {
  switch (frame.opcode) {
    case proto::PacketStatusChange::kOpcode: {
      proto::PacketStatusChange packet;
      if (!proto::Decode(frame.bytes, packet)) {
        return std::nullopt;
      }
      return Transition{.account_id = packet.AID,
                        .effect = static_cast<sim::StatusEffectId>(packet.index),
                        .active = packet.state != 0,
                        .remaining_ms = packet.Left,
                        .total_ms = packet.Total};
    }
    case proto::PacketStatusChange2::kOpcode: {
      proto::PacketStatusChange2 packet;
      if (!proto::Decode(frame.bytes, packet)) {
        return std::nullopt;
      }
      return Transition{.account_id = packet.AID,
                        .effect = static_cast<sim::StatusEffectId>(packet.index),
                        .active = packet.state != 0,
                        .remaining_ms = packet.Left,
                        .total_ms = packet.Left};
    }
    case proto::PacketStatusChangeEnd::kOpcode: {
      proto::PacketStatusChangeEnd packet;
      if (!proto::Decode(frame.bytes, packet)) {
        return std::nullopt;
      }
      return Transition{.account_id = packet.AID,
                        .effect = static_cast<sim::StatusEffectId>(packet.index),
                        .active = packet.state != 0};
    }
    // Un état encore posé sur un acteur qui entre en vue : jamais un retrait.
    case proto::ZcEfstSetEnter::kOpcode: {
      proto::ZcEfstSetEnter packet;
      if (!proto::Decode(frame.bytes, packet)) {
        return std::nullopt;
      }
      return Transition{.account_id = packet.targetID,
                        .effect = static_cast<sim::StatusEffectId>(packet.type),
                        .active = true,
                        .remaining_ms = packet.duration,
                        .total_ms = packet.duration};
    }
    default:
      return std::nullopt;
  }
}

[[nodiscard]] FrameOutcome ApplyList(const Frame& frame, sim::MemberStatuses& statuses,
                                     const sim::PartyRoster& party, std::uint32_t own_account_id,
                                     std::uint32_t now_ms) {
  proto::ZcMoonlightStatusList packet;
  if (!proto::Decode(frame.bytes, packet)) {
    return FrameOutcome::kUndecodable;
  }
  if (!IsMember(party, packet.gid, own_account_id)) {
    return FrameOutcome::kUnhandled;
  }
  // Introuvable ou refusé : garder l'ancienne liste montrerait un passé pour un
  // présent.
  if (packet.status == kStatusListRefused) {
    statuses.Refuse(packet.gid, now_ms);
    return FrameOutcome::kApplied;
  }
  if (packet.status != kTargetAnswerOk) {
    statuses.Forget(packet.gid);
    return FrameOutcome::kApplied;
  }
  const std::optional<std::vector<sim::StatusReport>> reports =
      ReadStatusReports(frame, packet.count);
  if (!reports.has_value()) {
    return FrameOutcome::kUndecodable;
  }
  statuses.Tell(packet.gid, *reports, now_ms);
  return FrameOutcome::kApplied;
}

}  // namespace

std::optional<std::vector<sim::StatusReport>> ReadStatusReports(const Frame& frame,
                                                                std::uint8_t count) {
  core::ByteReader reader(frame.bytes);
  reader.Seek(kStatusListFixedSize);
  // Une trame tronquée annoncerait plus qu'elle ne porte, un serveur bavard plus
  // que son propre plafond : on prend le plus petit des trois.
  const std::size_t fits = reader.remaining() / kStatusListEntrySize;
  const std::size_t kept =
      std::min({static_cast<std::size_t>(count), fits, kStatusListMostEntries});
  std::vector<sim::StatusReport> reports;
  reports.reserve(kept);
  for (std::size_t rank = 0; rank < kept; ++rank) {
    sim::StatusReport report;
    report.effect = reader.U16();
    report.remaining_ms = reader.U32();
    report.total_ms = reader.U32();
    reports.push_back(report);
  }
  if (!reader.ok()) {
    return std::nullopt;
  }
  return reports;
}

std::optional<StatusListAnswer> ReadStatusList(const Frame& frame) {
  proto::ZcMoonlightStatusList packet;
  if (frame.opcode != proto::ZcMoonlightStatusList::kOpcode ||
      !proto::Decode(frame.bytes, packet)) {
    return std::nullopt;
  }
  StatusListAnswer answer{.gid = packet.gid, .status = packet.status};
  if (packet.status == kTargetAnswerOk) {
    std::optional<std::vector<sim::StatusReport>> reports = ReadStatusReports(frame, packet.count);
    if (!reports.has_value()) {
      return std::nullopt;
    }
    answer.reports = std::move(*reports);
  }
  return answer;
}

FrameOutcome ApplyMemberStatusFrame(const Frame& frame, sim::MemberStatuses& statuses,
                                    const sim::PartyRoster& party, std::uint32_t own_account_id,
                                    std::uint32_t now_ms) {
  if (frame.opcode == proto::ZcMoonlightStatusList::kOpcode) {
    return ApplyList(frame, statuses, party, own_account_id, now_ms);
  }
  const std::optional<Transition> transition = TransitionOf(frame);
  if (!transition.has_value() || !IsMember(party, transition->account_id, own_account_id)) {
    return FrameOutcome::kUnhandled;
  }
  statuses.Apply(transition->account_id, transition->effect, transition->active,
                 transition->remaining_ms, transition->total_ms, now_ms);
  return FrameOutcome::kApplied;
}

}  // namespace ro::net
