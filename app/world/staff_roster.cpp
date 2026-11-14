#include "world/staff_roster.h"

#include <algorithm>
#include <utility>

#include "ro/core/ascii.h"
#include "ro/core/log.h"
#include "ro/ui/staff_tools.h"

namespace moonclient {
namespace {

using ro::net::StaffRosterClient;
using ro::net::StaffRosterStatus;
using ro::ui::StaffRosterClientKind;
using ro::ui::StaffRosterPhase;

constexpr std::uint32_t kMsPerSecond = 1000;

[[nodiscard]] StaffRosterClientKind ClientKindOf(StaffRosterClient client) {
  switch (client) {
    case StaffRosterClient::kNone:
      return StaffRosterClientKind::kNone;
    case StaffRosterClient::kOriginal:
      return StaffRosterClientKind::kOriginal;
    case StaffRosterClient::kMoonclient:
      return StaffRosterClientKind::kMoonclient;
    case StaffRosterClient::kOther:
      break;
  }
  return StaffRosterClientKind::kOther;
}

// Les genres d'action se suivent dans le même ordre sur le fil et à l'écran.
static_assert(ro::net::kStaffActivityCount == ro::ui::kStaffRosterActivityCount);
static_assert(static_cast<int>(ro::net::StaffActivity::kCommand) ==
              static_cast<int>(ro::ui::StaffRosterActivity::kCommand));

[[nodiscard]] ro::ui::StaffRosterRow RowOf(const ro::net::StaffRosterPlayer& player,
                                           const ro::vfs::JobNames* job_names) {
  ro::ui::StaffRosterRow row;
  row.account_id = player.account_id;
  row.char_id = player.char_id;
  row.name = player.name;
  row.guild = player.guild;
  row.party = player.party;
  if (job_names != nullptr) {
    row.job_name = job_names->Find(player.job);
  }
  row.base_level = player.base_level;
  row.job_level = player.job_level;
  row.map = player.map;
  row.instance_id = player.instance_id;
  row.cell_x = player.x;
  row.cell_y = player.y;
  row.group_level = player.group_level;
  row.staff = player.group_level >= ro::ui::kStaffMinGroupLevel;
  row.vending = player.state.vending;
  row.buying = player.state.buying;
  row.offline_shop = player.state.offline_shop;
  row.chat_room = player.state.chat_room;
  row.trading = player.state.trading;
  row.dead = player.state.dead;
  row.sitting = player.state.sitting;
  row.hidden = player.state.hidden;
  row.away = player.state.away;
  row.jailed = player.state.jailed;
  row.npc_dialog = player.state.npc_dialog;
  row.storage_open = player.state.storage_open;
  row.online_s = player.online_s;
  row.character_s = player.character_s;
  row.idle_s = player.idle_s;
  row.action_age_s = player.action_age_s;
  row.client = ClientKindOf(player.client);
  row.patch_index = player.patch_index;
  row.revision = player.commit;
  row.tree_modified = player.tree_modified;
  row.machine_group = player.machine_group;
  row.account_group = player.account_group;
  row.cheat_clock = player.cheat_clock;
  row.cheat_cadence = player.cheat_cadence;
  row.cheat_client = player.cheat_client;
  row.mute_minutes = player.mute_minutes;
  return row;
}

}  // namespace

bool IsStaffRosterCommand(std::string_view line) {
  return ro::core::AsciiEqualsFolded(ro::core::AsciiTrimmed(line), kStaffRosterCommand);
}

std::vector<ro::ui::StaffRosterRow> StaffRosterRowsOf(
    std::span<const ro::net::StaffRosterPlayer> players, const ro::vfs::JobNames* job_names,
    const OwnClient& own) {
  // Chaque client compte sur sa propre chaîne de patchs.
  std::int32_t newest_moonclient_patch = own.patch_index;
  std::int32_t newest_original_patch = -1;
  for (const ro::net::StaffRosterPlayer& player : players) {
    if (player.client == StaffRosterClient::kMoonclient) {
      newest_moonclient_patch = std::max(newest_moonclient_patch, player.patch_index);
    } else if (player.client == StaffRosterClient::kOriginal) {
      newest_original_patch = std::max(newest_original_patch, player.patch_index);
    }
  }

  std::vector<ro::ui::StaffRosterRow> rows;
  rows.reserve(players.size());
  for (const ro::net::StaffRosterPlayer& player : players) {
    ro::ui::StaffRosterRow row = RowOf(player, job_names);
    if (player.client == StaffRosterClient::kMoonclient) {
      row.client_outdated = player.patch_index < newest_moonclient_patch;
      row.other_revision = !row.client_outdated && player.patch_index == own.patch_index &&
                           !own.commit.empty() && !player.commit.empty() &&
                           player.commit != own.commit;
    } else if (player.client == StaffRosterClient::kOriginal) {
      row.client_outdated = player.patch_index < newest_original_patch;
    }
    rows.push_back(std::move(row));
  }
  return rows;
}

bool StaffRoster::Ask() {
  if (!allowed_) {
    return false;
  }
  ask_heard_ = true;
  return true;
}

ro::net::FrameOutcome StaffRoster::ApplyFrame(const ro::net::Frame& frame, std::uint32_t now_ms) {
  std::optional<ro::net::StaffRosterPage> page = ro::net::ReadStaffRoster(frame);
  if (!page.has_value()) {
    return frame.opcode == ro::net::kStaffRosterReplyOpcode ? ro::net::FrameOutcome::kUndecodable
                                                            : ro::net::FrameOutcome::kUnhandled;
  }
  const std::optional<ro::net::StaffRosterAnswer> answer = assembly_.Take(std::move(*page));
  if (!answer.has_value()) {
    return ro::net::FrameOutcome::kIgnored;
  }
  Note(*answer, now_ms);
  return ro::net::FrameOutcome::kApplied;
}

void StaffRoster::Note(const ro::net::StaffRosterAnswer& answer, std::uint32_t now_ms) {
  // Une réponse entière clôt l'attente, d'où qu'elle vienne.
  assembly_ = ro::net::StaffRosterAssembly{};
  answered_ms_ = now_ms;
  view_.refreshing = false;
  view_.rows.clear();
  switch (answer.status) {
    case StaffRosterStatus::kServed:
      view_.phase = StaffRosterPhase::kReady;
      view_.rows =
          StaffRosterRowsOf(answer.players, job_names_,
                            OwnClient{.patch_index = own_patch_index_, .commit = own_commit_});
      break;
    case StaffRosterStatus::kDenied:
      view_.phase = StaffRosterPhase::kDenied;
      break;
    case StaffRosterStatus::kUnknown:
      view_.phase = StaffRosterPhase::kUnreadable;
      break;
  }
}

void StaffRoster::Advance(ro::ui::StaffRosterWindowState& state,
                          const ro::ui::StaffRosterAnswer& answer,
                          const StaffRosterContext& context, std::uint32_t now_ms) {
  allowed_ = context.allowed;
  job_names_ = context.job_names;
  own_patch_index_ = context.own.patch_index;
  own_commit_ = context.own.commit;
  if (!allowed_) {
    ask_heard_ = false;
    Forget();
    return;
  }
  if (std::exchange(ask_heard_, false)) {
    state.open = true;
    state.raise = true;
  }
  if (!state.open) {
    Forget();
    return;
  }
  if (assembly_.waiting() && asked_ms_.has_value() && now_ms - *asked_ms_ >= kStaffRosterWaitMs) {
    assembly_ = ro::net::StaffRosterAssembly{};
    pending_request_.reset();
    view_.phase = StaffRosterPhase::kSilent;
    view_.refreshing = false;
    view_.rows.clear();
  }
  const bool gap_elapsed = !asked_ms_.has_value() || now_ms - *asked_ms_ >= kStaffRosterLeastGapMs;
  if (!assembly_.waiting() && gap_elapsed) {
    // La première demande d'une fenêtre qui vient de s'ouvrir.
    const bool first = !answered_ms_.has_value() && view_.phase == StaffRosterPhase::kAsking;
    const bool due = state.auto_refresh && view_.phase == StaffRosterPhase::kReady &&
                     answered_ms_.has_value() && now_ms - *answered_ms_ >= kStaffRosterRefreshMs;
    if (first || answer.refresh || due) {
      Request(now_ms);
    }
  }
  view_.can_refresh = !assembly_.waiting() &&
                      (!asked_ms_.has_value() || now_ms - *asked_ms_ >= kStaffRosterLeastGapMs);
  view_.age_s = answered_ms_.has_value() ? (now_ms - *answered_ms_) / kMsPerSecond : 0;
  view_.refresh_period_s = kStaffRosterRefreshMs / kMsPerSecond;
}

std::optional<std::uint16_t> StaffRoster::TakeRequest() {
  return std::exchange(pending_request_, std::nullopt);
}

void StaffRoster::SendRequest(ro::net::Connection& link) {
  const std::optional<std::uint16_t> request_id = TakeRequest();
  if (!request_id.has_value()) {
    return;
  }
  if (auto sent = ro::net::RequestStaffRoster(link, *request_id); !sent) {
    ro::core::LogWarning("joueurs en ligne : demande non émise — {}",
                         ro::core::Describe(sent.error()));
  }
}

const ro::ui::StaffRosterRow* StaffRoster::Find(std::uint32_t account_id) const {
  const auto found = std::ranges::find(view_.rows, account_id, &ro::ui::StaffRosterRow::account_id);
  return found != view_.rows.end() ? &*found : nullptr;
}

void StaffRoster::Request(std::uint32_t now_ms) {
  ++last_request_id_;
  pending_request_ = last_request_id_;
  assembly_.Expect(last_request_id_);
  asked_ms_ = now_ms;
  if (view_.phase == StaffRosterPhase::kReady) {
    // La liste précédente reste montrée jusqu'à la suivante.
    view_.refreshing = true;
    return;
  }
  view_.phase = StaffRosterPhase::kAsking;
  view_.rows.clear();
}

void StaffRoster::Forget() {
  pending_request_.reset();
  assembly_ = ro::net::StaffRosterAssembly{};
  answered_ms_.reset();
  view_ = ro::ui::StaffRosterView{};
}

}  // namespace moonclient
