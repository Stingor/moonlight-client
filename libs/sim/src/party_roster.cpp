#include "ro/sim/party_roster.h"

#include <algorithm>
#include <utility>

namespace ro::sim {
namespace {

// Ce que la liste ne porte pas se garde d'une description à la suivante, tant
// que le membre reste en ligne sur la même carte : ailleurs, ces valeurs ne
// disent plus rien de lui.
void CarryOver(const PartyMember& before, PartyMember& after) {
  if (after.offline || before.map != after.map) {
    return;
  }
  after.position = before.position;
  after.health = before.health;
  after.magic = before.magic;
  after.dead = before.dead;
}

}  // namespace

void PartyRoster::Replace(std::string party_name, std::vector<PartyMember> members) {
  for (PartyMember& member : members) {
    if (const PartyMember* before = Find(member.account_id); before != nullptr) {
      CarryOver(*before, member);
    }
  }
  name_ = std::move(party_name);
  members_ = std::move(members);
}

void PartyRoster::Upsert(PartyMember member) {
  const auto found = std::ranges::find(members_, member.account_id, &PartyMember::account_id);
  if (found != members_.end()) {
    CarryOver(*found, member);
    *found = std::move(member);
    return;
  }
  members_.push_back(std::move(member));
}

bool PartyRoster::Remove(std::uint32_t account_id) {
  std::erase_if(share_,
                [account_id](const PartyShareRow& row) { return row.account_id == account_id; });
  return std::erase_if(members_, [account_id](const PartyMember& member) {
           return member.account_id == account_id;
         }) > 0;
}

void PartyRoster::Clear() {
  name_.clear();
  members_.clear();
  options_.reset();
  share_.clear();
  share_idle_s_ = 0;
  share_known_ = false;
}

const PartyMember* PartyRoster::Find(std::uint32_t account_id) const {
  const auto found = std::ranges::find(members_, account_id, &PartyMember::account_id);
  return found == members_.end() ? nullptr : &*found;
}

PartyMember* PartyRoster::FindMutable(std::uint32_t account_id) {
  const auto found = std::ranges::find(members_, account_id, &PartyMember::account_id);
  return found == members_.end() ? nullptr : &*found;
}

const PartyMember* PartyRoster::FindByName(std::string_view name) const {
  const auto found = std::ranges::find(members_, name, &PartyMember::name);
  return found == members_.end() ? nullptr : &*found;
}

bool PartyRoster::HasOnlineMember(std::uint32_t account_id) const {
  const PartyMember* member = Find(account_id);
  return member != nullptr && !member->offline;
}

bool PartyRoster::IsLeader(std::uint32_t account_id) const {
  const PartyMember* member = Find(account_id);
  return member != nullptr && member->leader;
}

bool PartyRoster::TellPosition(std::uint32_t account_id, std::optional<PartyPosition> position) {
  PartyMember* member = FindMutable(account_id);
  if (member == nullptr) {
    return false;
  }
  member->position = position;
  return true;
}

bool PartyRoster::TellHealth(std::uint32_t account_id, PartyGauge health) {
  PartyMember* member = FindMutable(account_id);
  if (member == nullptr) {
    return false;
  }
  member->health = health;
  return true;
}

bool PartyRoster::TellMagic(std::uint32_t account_id, PartyGauge magic, std::uint32_t now_ms) {
  PartyMember* member = FindMutable(account_id);
  if (member == nullptr) {
    return false;
  }
  member->magic = PartyMagic{.gauge = magic, .received_ms = now_ms};
  return true;
}

bool PartyRoster::ForgetMagic(std::uint32_t account_id) {
  PartyMember* member = FindMutable(account_id);
  if (member == nullptr) {
    return false;
  }
  member->magic.reset();
  return true;
}

bool PartyRoster::TellDead(std::uint32_t account_id, bool dead) {
  PartyMember* member = FindMutable(account_id);
  if (member == nullptr) {
    return false;
  }
  member->dead = dead;
  return true;
}

bool PartyRoster::TellJobAndLevel(std::uint32_t account_id, std::uint16_t job,
                                  std::uint16_t base_level) {
  PartyMember* member = FindMutable(account_id);
  if (member == nullptr) {
    return false;
  }
  member->job = job;
  member->base_level = base_level;
  return true;
}

bool PartyRoster::TellLeader(std::uint32_t previous_account_id, std::uint32_t new_account_id) {
  PartyMember* next = FindMutable(new_account_id);
  if (next == nullptr) {
    return false;
  }
  if (PartyMember* previous = FindMutable(previous_account_id); previous != nullptr) {
    previous->leader = false;
  }
  next->leader = true;
  return true;
}

std::optional<PartyGauge> PartyRoster::FreshMagic(std::uint32_t account_id,
                                                  std::uint32_t now_ms) const {
  const PartyMember* member = Find(account_id);
  if (member == nullptr || !member->magic.has_value()) {
    return std::nullopt;
  }
  if (now_ms - member->magic->received_ms > kPartyMagicStaleMs) {
    return std::nullopt;
  }
  return member->magic->gauge;
}

void PartyRoster::TellOptions(PartyOptions options) {
  if (options_.has_value() && *options_ != options && in_party()) {
    Notify(PartyNotice{.kind = PartyNoticeKind::kOptionsChanged, .options = options});
  }
  options_ = options;
}

void PartyRoster::TellItemOptions(bool pickup_shared, bool loot_shared) {
  if (!options_.has_value()) {
    return;
  }
  options_->pickup_shared = pickup_shared;
  options_->loot_shared = loot_shared;
}

void PartyRoster::TellShare(std::uint16_t idle_s, std::vector<PartyShareRow> rows) {
  share_idle_s_ = idle_s;
  share_ = std::move(rows);
  share_known_ = true;
}

std::uint8_t PartyRoster::ShareFlagsFor(std::uint32_t account_id,
                                        std::uint32_t own_account_id) const {
  const PartyMember* member = Find(account_id);
  if (!share_known_ || member == nullptr || member->offline) {
    return 0;
  }
  std::uint8_t flags = 0;
  const auto row = std::ranges::find(share_, account_id, &PartyShareRow::account_id);
  if (row != share_.end()) {
    flags = row->flags;
  }
  const PartyMember* own = Find(own_account_id);
  if (account_id != own_account_id && own != nullptr && !own->map.empty() && !member->map.empty() &&
      own->map != member->map) {
    flags |= kShareOtherMap;
  }
  return flags;
}

std::vector<PartyNotice> PartyRoster::TakeNotices() {
  std::vector<PartyNotice> taken;
  taken.swap(notices_);
  return taken;
}

}  // namespace ro::sim
