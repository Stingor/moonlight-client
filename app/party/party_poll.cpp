#include "party/party_poll.h"

namespace moonclient {

std::uint32_t PartyVitalsPoll::Next(std::span<const ro::sim::PartyMember> members,
                                    std::uint32_t own_account_id, bool wanted,
                                    std::uint32_t now_ms) {
  if (!wanted) {
    return 0;
  }
  if (last_poll_ms_.has_value() && now_ms - *last_poll_ms_ < kPartyPollIntervalMs) {
    return 0;
  }
  if (members.size() <= 1) {
    return 0;
  }
  for (std::size_t tried = 0; tried < members.size(); ++tried) {
    if (cursor_ >= members.size()) {
      cursor_ = 0;
    }
    const ro::sim::PartyMember& member = members[cursor_++];
    if (member.account_id == 0 || member.account_id == own_account_id || member.offline) {
      continue;
    }
    last_poll_ms_ = now_ms;
    return member.account_id;
  }
  return 0;
}

std::uint32_t MemberStatusPoll::Next(std::span<const ro::sim::PartyMember> members,
                                     std::uint32_t own_account_id, bool wanted,
                                     const ro::sim::MemberStatuses& statuses,
                                     const std::function<bool(std::uint32_t)>& present,
                                     std::uint32_t now_ms) {
  if (!wanted) {
    return 0;
  }
  if (last_poll_ms_.has_value() && now_ms - *last_poll_ms_ < kMemberStatusPollIntervalMs) {
    return 0;
  }
  if (members.size() <= 1) {
    return 0;
  }
  for (std::size_t tried = 0; tried < members.size(); ++tried) {
    if (cursor_ >= members.size()) {
      cursor_ = 0;
    }
    const ro::sim::PartyMember& member = members[cursor_++];
    if (member.account_id == 0 || member.account_id == own_account_id || member.offline ||
        statuses.Refused(member.account_id, now_ms) || !present(member.account_id)) {
      continue;
    }
    last_poll_ms_ = now_ms;
    return member.account_id;
  }
  return 0;
}

void MemberStatusPoll::Reset() {
  cursor_ = 0;
  last_poll_ms_.reset();
}

}  // namespace moonclient
