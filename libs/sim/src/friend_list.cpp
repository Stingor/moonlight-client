#include "ro/sim/friend_list.h"

#include <algorithm>

namespace ro::sim {

void FriendList::Replace(std::vector<Friend> friends) {
  if (friends.size() > kMaxFriends) {
    friends.resize(kMaxFriends);
  }
  for (Friend& listed : friends) {
    listed.online = false;
  }
  friends_ = std::move(friends);
}

void FriendList::Add(Friend added) {
  if (Friend* known = FindMutable(added.account_id, added.char_id); known != nullptr) {
    *known = std::move(added);
    return;
  }
  friends_.push_back(std::move(added));
}

bool FriendList::Remove(std::uint32_t account_id, std::uint32_t char_id) {
  return std::erase_if(friends_, [account_id, char_id](const Friend& listed) {
           return listed.account_id == account_id && listed.char_id == char_id;
         }) > 0;
}

bool FriendList::TellOnline(std::uint32_t account_id, std::uint32_t char_id, bool online,
                            std::string name) {
  Friend* known = FindMutable(account_id, char_id);
  if (known == nullptr) {
    return false;
  }
  known->online = online;
  if (!name.empty()) {
    known->name = std::move(name);
  }
  return true;
}

const Friend* FriendList::Find(std::uint32_t account_id, std::uint32_t char_id) const {
  const auto found = std::ranges::find_if(friends_, [account_id, char_id](const Friend& listed) {
    return listed.account_id == account_id && listed.char_id == char_id;
  });
  return found == friends_.end() ? nullptr : &*found;
}

Friend* FriendList::FindMutable(std::uint32_t account_id, std::uint32_t char_id) {
  const auto found = std::ranges::find_if(friends_, [account_id, char_id](const Friend& listed) {
    return listed.account_id == account_id && listed.char_id == char_id;
  });
  return found == friends_.end() ? nullptr : &*found;
}

const Friend* FriendList::FindByAccount(std::uint32_t account_id) const {
  const auto found = std::ranges::find(friends_, account_id, &Friend::account_id);
  return found == friends_.end() ? nullptr : &*found;
}

const Friend* FriendList::FindByName(std::string_view name) const {
  const auto found = std::ranges::find(friends_, name, &Friend::name);
  return found == friends_.end() ? nullptr : &*found;
}

std::vector<FriendNotice> FriendList::TakeNotices() {
  std::vector<FriendNotice> taken;
  taken.swap(notices_);
  return taken;
}

}  // namespace ro::sim
