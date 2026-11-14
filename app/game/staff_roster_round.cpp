#include "game/staff_roster_round.h"

#include <utility>

#include "ro/net/chat_target.h"

#include "world/click_action.h"
#include "world/staff_roster.h"

namespace moonclient {

std::optional<StaffRosterMenu> AdvanceStaffRoster(
    GamePieces& pieces, ro::ui::StaffToolsState& tools, ro::net::Connection& link,
    const ro::sim::World& world, const ro::net::ClientIdentity& own_client, std::uint32_t now_ms) {
  StaffRoster& roster = pieces.staff_roster;
  const bool allowed = ro::ui::StaffToolsAllowed(world.group_level(), link.is_open());
  tools.roster_view = &roster.view();
  const ro::ui::StaffRosterAnswer answer = std::exchange(tools.roster_answer, {});
  roster.Advance(tools.roster, answer,
                 StaffRosterContext{
                     .allowed = allowed,
                     .job_names = &pieces.job_names,
                     .own = {.patch_index = own_client.patch_index, .commit = own_client.commit}},
                 now_ms);
  if (link.is_open()) {
    roster.SendRequest(link);
    // Le repli d'un serveur muet : la commande, dont la réponse s'écrit dans la
    // conversation.
    if (allowed && answer.say_command) {
      ReportRefusal(ro::net::SendServerCommand(link, world.player_name(), kStaffRosterCommand));
    }
  }
  if (!allowed || !answer.menu_account_id.has_value()) {
    return std::nullopt;
  }
  // Par le compte, et non par le rang : la liste a pu changer depuis le clic.
  const ro::ui::StaffRosterRow* const row = roster.Find(*answer.menu_account_id);
  if (row == nullptr) {
    return std::nullopt;
  }
  StaffRosterMenu menu;
  menu.target.family =
      row->account_id == world.player_id() ? EntityFamily::kSelf : EntityFamily::kPlayer;
  menu.target.id = row->account_id;
  menu.target.name = row->name;
  menu.target.staff = true;
  menu.target.admin = world.group_level() >= kAdminMinGroupLevel;
  menu.view = BuildStaffRosterMenu(menu.target);
  if (menu.view.items.empty()) {
    return std::nullopt;
  }
  return menu;
}

}  // namespace moonclient
