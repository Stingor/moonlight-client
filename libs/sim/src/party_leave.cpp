// Quitter le groupe : qui reprend la direction. Voir `PlanLeave`.

#include "ro/sim/party_roster.h"

namespace ro::sim {

LeavePlan PlanLeave(const PartyRoster& party, std::uint32_t own_account_id) {
  const PartyMember* own = party.Find(own_account_id);
  if (own == nullptr || !own->leader || party.members().size() <= 1) {
    return LeavePlan{};
  }
  bool someone_online = false;
  for (const PartyMember& member : party.members()) {
    if (member.account_id == own_account_id || member.offline) {
      continue;
    }
    someone_online = true;
    if (member.map == own->map) {
      return LeavePlan{.step = LeaveStep::kHandOverThenLeave,
                       .successor_account_id = member.account_id};
    }
  }
  return LeavePlan{.step = someone_online ? LeaveStep::kNeedsHandOver : LeaveStep::kConfirmDisband};
}

}  // namespace ro::sim
