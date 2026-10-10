#include "ro/sim/quest_log.h"

#include <algorithm>
#include <utility>

namespace ro::sim {

const QuestRecord* QuestLog::Find(std::uint32_t quest_id) const {
  const auto found = std::ranges::find(quests_, quest_id, &QuestRecord::quest_id);
  return found == quests_.end() ? nullptr : &*found;
}

QuestRecord* QuestLog::FindMutable(std::uint32_t quest_id) {
  const auto found = std::ranges::find(quests_, quest_id, &QuestRecord::quest_id);
  return found == quests_.end() ? nullptr : &*found;
}

void QuestLog::Replace(std::vector<QuestRecord> quests) {
  quests_ = std::move(quests);
  pending_endings_.clear();
  ++revision_;
}

void QuestLog::Add(QuestRecord quest) {
  if (QuestRecord* same = FindMutable(quest.quest_id)) {
    *same = std::move(quest);
  } else {
    quests_.push_back(std::move(quest));
  }
  ++revision_;
}

bool QuestLog::SetHuntTotal(std::uint32_t hunt_id, std::uint32_t mob_id, std::int16_t total,
                            std::int16_t killed) {
  for (QuestRecord& quest : quests_) {
    for (QuestHunt& hunt : quest.hunts) {
      if (hunt.hunt_id != hunt_id) {
        continue;
      }
      hunt.total = total;
      hunt.killed = killed;
      hunt.mob_id = mob_id;
      ++revision_;
      return true;
    }
  }
  return false;
}

bool QuestLog::SetHuntTotalAt(std::uint32_t quest_id, std::size_t hunt_rank, std::uint32_t mob_id,
                              std::int16_t total, std::int16_t killed) {
  QuestRecord* quest = FindMutable(quest_id);
  if (quest == nullptr || hunt_rank >= quest->hunts.size()) {
    return false;
  }
  QuestHunt& hunt = quest->hunts[hunt_rank];
  hunt.total = total;
  hunt.killed = killed;
  hunt.mob_id = mob_id;
  ++revision_;
  return true;
}

bool QuestLog::UpdateHunt(std::uint32_t quest_id, std::uint32_t hunt_id, std::int16_t total,
                          std::int16_t killed) {
  QuestRecord* quest = FindMutable(quest_id);
  if (quest == nullptr) {
    return false;
  }
  const auto found = std::ranges::find(quest->hunts, hunt_id, &QuestHunt::hunt_id);
  if (found == quest->hunts.end()) {
    return false;
  }
  const bool progressed = killed > found->killed;
  if (found->total != total || found->killed != killed) {
    found->total = total;
    found->killed = killed;
    ++revision_;
  }
  return progressed;
}

bool QuestLog::Remove(std::uint32_t quest_id) {
  const std::size_t removed = std::erase_if(
      quests_, [quest_id](const QuestRecord& quest) { return quest.quest_id == quest_id; });
  if (removed == 0) {
    return false;
  }
  ++revision_;
  return true;
}

bool QuestLog::SetActive(std::uint32_t quest_id, bool active) {
  QuestRecord* quest = FindMutable(quest_id);
  if (quest == nullptr) {
    return false;
  }
  if (quest->active != active) {
    quest->active = active;
    ++revision_;
  }
  return true;
}

void QuestLog::Clear() {
  quests_.clear();
  pending_endings_.clear();
  ++revision_;
}

void QuestLog::NoteEnding(std::uint32_t quest_id, QuestEnding ending) {
  std::erase_if(pending_endings_,
                [quest_id](const auto& pending) { return pending.first == quest_id; });
  pending_endings_.emplace_back(quest_id, ending);
}

std::optional<QuestEnding> QuestLog::TakeEnding(std::uint32_t quest_id) {
  const auto found =
      std::ranges::find(pending_endings_, quest_id, &std::pair<std::uint32_t, QuestEnding>::first);
  if (found == pending_endings_.end()) {
    return std::nullopt;
  }
  const QuestEnding ending = found->second;
  pending_endings_.erase(found);
  return ending;
}

bool IsKnownQuestEndReason(std::uint8_t wire_reason) noexcept {
  switch (static_cast<QuestEndReason>(wire_reason)) {
    case QuestEndReason::kCompleted:
    case QuestEndReason::kErased:
    case QuestEndReason::kReplaced:
    case QuestEndReason::kRetired:
      return true;
  }
  return false;
}

}  // namespace ro::sim
