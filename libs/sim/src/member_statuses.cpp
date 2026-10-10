#include "ro/sim/member_statuses.h"

#include <algorithm>
#include <utility>

namespace ro::sim {
namespace {

// Ce qu'il reste d'un état connu. ⚠ SIGNÉ : un état qui vient d'échoir donnerait,
// en non signé, un restant colossal.
[[nodiscard]] std::uint32_t RemainingOf(const ActiveStatus& status, std::uint32_t now_ms) {
  if (status.expires_ms == 0) {
    return 0;
  }
  const auto left = static_cast<std::int32_t>(status.expires_ms - now_ms);
  return left > 0 ? static_cast<std::uint32_t>(left) : 0;
}

[[nodiscard]] const ActiveStatus* FindEffect(const std::vector<ActiveStatus>& list,
                                             StatusEffectId effect) {
  const auto found = std::ranges::find(list, effect, &ActiveStatus::effect);
  return found == list.end() ? nullptr : &*found;
}

}  // namespace

std::uint32_t KeepLongestDuration(std::uint32_t announced, std::uint32_t previous_total,
                                  std::uint32_t previous_left) {
  if (announced == 0) {
    return previous_total;
  }
  if (announced > previous_left + kMemberStatusRefreshMarginMs) {
    return announced;
  }
  return std::max(previous_total, announced);
}

void MemberStatuses::Tell(std::uint32_t account_id, std::span<const StatusReport> reported,
                          std::uint32_t now_ms) {
  const auto known = by_account_.find(account_id);
  std::vector<ActiveStatus> fresh;
  fresh.reserve(reported.size());
  for (const StatusReport& report : reported) {
    if (report.effect == 0) {
      continue;
    }
    ActiveStatus status;
    status.effect = report.effect;
    status.expires_ms = report.remaining_ms == 0 ? 0 : now_ms + report.remaining_ms;
    std::uint32_t previous_total = 0;
    std::uint32_t previous_left = 0;
    if (known != by_account_.end()) {
      if (const ActiveStatus* old = FindEffect(known->second, report.effect); old != nullptr) {
        previous_total = old->total_ms;
        previous_left = RemainingOf(*old, now_ms);
      }
    }
    status.total_ms = KeepLongestDuration(std::max(report.remaining_ms, report.total_ms),
                                          previous_total, previous_left);
    fresh.push_back(status);
  }
  by_account_.insert_or_assign(account_id, std::move(fresh));
}

void MemberStatuses::Apply(std::uint32_t account_id, StatusEffectId effect, bool active,
                           std::uint32_t remaining_ms, std::uint32_t total_ms,
                           std::uint32_t now_ms) {
  if (effect == 0) {
    return;
  }
  auto known = by_account_.find(account_id);
  if (!active) {
    if (known != by_account_.end()) {
      std::erase_if(known->second,
                    [effect](const ActiveStatus& status) { return status.effect == effect; });
    }
    return;
  }
  if (known == by_account_.end()) {
    known = by_account_.try_emplace(account_id).first;
  }

  const bool timed = remaining_ms != 0 && remaining_ms != kUnknownDurationMs;
  ActiveStatus status;
  status.effect = effect;
  status.expires_ms = timed ? now_ms + remaining_ms : 0;
  status.total_ms = timed ? std::max(remaining_ms, total_ms) : 0;

  // Le même état qui revient RENOUVELLE le sien : sa durée repart, sans seconde
  // icône.
  const auto old = std::ranges::find(known->second, effect, &ActiveStatus::effect);
  if (old != known->second.end()) {
    status.total_ms =
        KeepLongestDuration(status.total_ms, old->total_ms, RemainingOf(*old, now_ms));
    *old = status;
    return;
  }
  known->second.push_back(status);
}

void MemberStatuses::Forget(std::uint32_t account_id) { by_account_.erase(account_id); }

void MemberStatuses::Refuse(std::uint32_t account_id, std::uint32_t now_ms) {
  by_account_.erase(account_id);
  refused_ms_.insert_or_assign(account_id, now_ms);
}

bool MemberStatuses::Refused(std::uint32_t account_id, std::uint32_t now_ms) const {
  const auto found = refused_ms_.find(account_id);
  return found != refused_ms_.end() && now_ms - found->second < kMemberStatusRefusedQuietMs;
}

void MemberStatuses::Purge(std::uint32_t now_ms, const std::function<bool(std::uint32_t)>& keep) {
  std::erase_if(by_account_, [&keep](const auto& entry) { return !keep(entry.first); });
  for (auto& entry : by_account_) {
    std::erase_if(entry.second, [now_ms](const ActiveStatus& status) {
      return status.expires_ms != 0 && static_cast<std::int32_t>(now_ms - status.expires_ms) >= 0;
    });
  }
}

void MemberStatuses::Clear() {
  by_account_.clear();
  refused_ms_.clear();
}

const std::vector<ActiveStatus>* MemberStatuses::Find(std::uint32_t account_id) const {
  const auto found = by_account_.find(account_id);
  return found == by_account_.end() ? nullptr : &found->second;
}

}  // namespace ro::sim
