// Les succès du personnage. Voir l'en-tête.

#include "ro/sim/achievement_book.h"

#include <algorithm>

namespace ro::sim {

AchievementRecord& AchievementBook::Upsert(std::uint32_t achievement_id) {
  const auto found =
      std::ranges::lower_bound(records_, achievement_id, {}, &AchievementRecord::achievement_id);
  if (found != records_.end() && found->achievement_id == achievement_id) {
    return *found;
  }
  return *records_.insert(found, AchievementRecord{.achievement_id = achievement_id});
}

void AchievementBook::ApplyList(const AchievementStanding& standing,
                                std::span<const AchievementRecord> records) {
  standing_ = standing;
  for (const AchievementRecord& record : records) {
    Upsert(record.achievement_id).progress = record.progress;
    if (record.progress.rewarded) {
      SettleClaim(record.achievement_id);
    }
  }
  ++revision_;
}

bool AchievementBook::ApplyUpdate(const AchievementStanding& standing,
                                  const AchievementRecord& record) {
  standing_ = standing;
  if (record.achievement_id == kNoAchievement) {
    ++revision_;
    return false;
  }
  const AchievementProgress* before = Find(record.achievement_id);
  const bool newly_completed =
      record.progress.completed && (before == nullptr || !before->completed);
  Upsert(record.achievement_id).progress = record.progress;
  if (record.progress.rewarded) {
    SettleClaim(record.achievement_id);
  }
  ++revision_;
  return newly_completed;
}

void AchievementBook::SettleClaim(std::uint32_t achievement_id) {
  if (std::erase(pending_claims_, achievement_id) != 0) {
    ++revision_;
  }
}

const AchievementProgress* AchievementBook::Find(std::uint32_t achievement_id) const {
  const auto found =
      std::ranges::lower_bound(records_, achievement_id, {}, &AchievementRecord::achievement_id);
  if (found == records_.end() || found->achievement_id != achievement_id) {
    return nullptr;
  }
  return &found->progress;
}

bool AchievementBook::CanClaim(std::uint32_t achievement_id, bool rewards) const {
  const AchievementProgress* progress = Find(achievement_id);
  return rewards && progress != nullptr && progress->completed && !progress->rewarded &&
         !ClaimPending(achievement_id);
}

bool AchievementBook::NoteClaim(std::uint32_t achievement_id, bool rewards) {
  if (!CanClaim(achievement_id, rewards)) {
    return false;
  }
  pending_claims_.push_back(achievement_id);
  ++revision_;
  return true;
}

bool AchievementBook::ClaimPending(std::uint32_t achievement_id) const {
  return std::ranges::find(pending_claims_, achievement_id) != pending_claims_.end();
}

TrackOutcome AchievementBook::ToggleTracking(std::uint32_t achievement_id) {
  if (std::erase(tracked_, achievement_id) != 0) {
    ++revision_;
    return TrackOutcome::kUntracked;
  }
  if (tracked_.size() >= kMostTrackedAchievements) {
    return TrackOutcome::kFull;
  }
  tracked_.push_back(achievement_id);
  ++revision_;
  return TrackOutcome::kTracked;
}

bool AchievementBook::IsTracked(std::uint32_t achievement_id) const {
  return std::ranges::find(tracked_, achievement_id) != tracked_.end();
}

}  // namespace ro::sim
