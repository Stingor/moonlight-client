#include "chat/staff_item_command.h"

#include <algorithm>
#include <utility>

namespace moonclient {

std::string StaffItemCommand(const ro::ui::StaffItemGrant& grant) {
  if (grant.item_id == 0 || grant.amount <= 0) {
    return {};
  }
  std::string line = "@item ";
  line += std::to_string(grant.item_id);
  line += ' ';
  line += std::to_string(std::min(grant.amount, ro::ui::kStaffItemMostAmount));
  return line;
}

std::vector<std::string> StaffItemCommands(std::span<const ro::ui::StaffItemGrant> grants,
                                           ro::ui::SessionRights rights) {
  std::vector<std::string> lines;
  if (!rights.staff_tools) {
    return lines;
  }
  for (const ro::ui::StaffItemGrant& grant : grants) {
    std::string line = StaffItemCommand(grant);
    if (!line.empty()) {
      lines.push_back(std::move(line));
    }
  }
  return lines;
}

}  // namespace moonclient
