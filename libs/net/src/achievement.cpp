// Les succès. Voir l'en-tête pour les trois paquets et le sens de l'accusé.

#include "ro/net/achievement.h"

#include <algorithm>
#include <vector>

#include "ro/proto/map_packets.h"
#include "ro/proto/title_packets.h"

#include "send_encoded.h"

namespace ro::net {
namespace {

// L'octet de l'accusé qui dit la réussite ; tout autre est un refus.
constexpr std::uint8_t kClaimGranted = 1;

[[nodiscard]] sim::AchievementRecord RecordOf(const proto::AchievementEntry& entry) {
  sim::AchievementRecord record{.achievement_id = entry.achievement_id};
  record.progress.completed = entry.completed;
  std::ranges::copy(entry.objectives, record.progress.counts.begin());
  record.progress.completed_at = entry.completed_at;
  record.progress.rewarded = entry.rewarded;
  return record;
}

[[nodiscard]] sim::AchievementRecord RecordOf(const proto::AchListInfo& entry) {
  sim::AchievementRecord record{.achievement_id = entry.ach_id};
  record.progress.completed = entry.completed != 0;
  std::ranges::copy(entry.objective, record.progress.counts.begin());
  record.progress.completed_at = entry.completed_at;
  record.progress.rewarded = entry.reward != 0;
  return record;
}

}  // namespace

AchievementFrameResult ApplyAchievementFrame(const Frame& frame, sim::AchievementBook& book) {
  AchievementFrameResult result;
  switch (frame.opcode) {
    case proto::AchievementList::kOpcode: {
      const auto list = proto::DecodeAchievementList(frame.bytes);
      if (!list) {
        result.outcome = FrameOutcome::kUndecodable;
        return result;
      }
      std::vector<sim::AchievementRecord> records;
      records.reserve(list->entries.size());
      for (const proto::AchievementEntry& entry : list->entries) {
        records.push_back(RecordOf(entry));
      }
      book.ApplyList(sim::AchievementStanding{.total_points = list->total_points,
                                              .rank = list->rank,
                                              .rank_points = list->current_rank_points,
                                              .rank_span = list->next_rank_points},
                     records);
      result.outcome = FrameOutcome::kApplied;
      return result;
    }

    case proto::PacketAchievementUpdate::kOpcode: {
      proto::PacketAchievementUpdate packet;
      if (!proto::Decode(frame.bytes, packet)) {
        result.outcome = FrameOutcome::kUndecodable;
        return result;
      }
      const sim::AchievementRecord record = RecordOf(packet.ach);
      if (book.ApplyUpdate(sim::AchievementStanding{.total_points = packet.total_points,
                                                    .rank = packet.rank,
                                                    .rank_points = packet.current_rank_points,
                                                    .rank_span = packet.next_rank_points},
                           record)) {
        result.completed = record.achievement_id;
      }
      result.outcome = FrameOutcome::kApplied;
      return result;
    }

    case proto::PacketAchievementRewardAck::kOpcode: {
      proto::PacketAchievementRewardAck packet;
      if (!proto::Decode(frame.bytes, packet)) {
        result.outcome = FrameOutcome::kUndecodable;
        return result;
      }
      book.SettleClaim(packet.ach_id);
      if (packet.failed != kClaimGranted) {
        result.refused_claim = packet.ach_id;
      }
      result.outcome = FrameOutcome::kApplied;
      return result;
    }

    default:
      return result;
  }
}

core::Result<void> ClaimAchievementReward(Connection& link, sim::AchievementBook& book,
                                          std::uint32_t achievement_id, bool rewards) {
  if (!book.CanClaim(achievement_id, rewards)) {
    return core::Fail(core::ErrorCode::kInvalidArgument,
                      "réclamation refusée d'avance : succès inachevé, déjà réclamé, en vol ou "
                      "sans récompense");
  }
  if (auto sent = SendEncoded(
          link, proto::Encode(proto::AchievementRewardRequest{.achievement_id = achievement_id}));
      !sent) {
    return sent;
  }
  static_cast<void>(book.NoteClaim(achievement_id, rewards));
  return {};
}

}  // namespace ro::net
