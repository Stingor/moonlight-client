// Les joueurs en ligne du staff : la demande, la réponse et son assemblage.
// Voir l'en-tête.

#include "ro/net/staff_roster.h"

#include <utility>

#include "ro/core/text.h"
#include "ro/proto/map_packets.h"

namespace ro::net {
namespace {

static_assert(kStaffRosterReplyOpcode == proto::ZcMoonlightStaffRoster::kOpcode);

// Les bits de `state`, tels que le serveur les pose.
constexpr std::uint32_t kStateVending = 0x0001;
constexpr std::uint32_t kStateBuying = 0x0002;
constexpr std::uint32_t kStateOfflineShop = 0x0004;
constexpr std::uint32_t kStateChatRoom = 0x0008;
constexpr std::uint32_t kStateTrading = 0x0010;
constexpr std::uint32_t kStateDead = 0x0020;
constexpr std::uint32_t kStateSitting = 0x0040;
constexpr std::uint32_t kStateHidden = 0x0080;
constexpr std::uint32_t kStateAway = 0x0100;
constexpr std::uint32_t kStateJailed = 0x0200;
constexpr std::uint32_t kStateNpcDialog = 0x0400;
constexpr std::uint32_t kStateStorageOpen = 0x0800;

constexpr std::uint8_t kClientFlagTreeModified = 0x01;
// L'âge d'une action qui n'a pas eu lieu.
constexpr std::uint32_t kNeverAge = 0xFFFFFFFFU;

[[nodiscard]] StaffRosterStatus StatusOf(std::uint8_t wire) {
  switch (wire) {
    case static_cast<std::uint8_t>(StaffRosterStatus::kServed):
      return StaffRosterStatus::kServed;
    case static_cast<std::uint8_t>(StaffRosterStatus::kDenied):
      return StaffRosterStatus::kDenied;
    default:
      break;
  }
  return StaffRosterStatus::kUnknown;
}

[[nodiscard]] StaffRosterClient ClientOf(std::uint8_t wire) {
  switch (wire) {
    case static_cast<std::uint8_t>(StaffRosterClient::kNone):
      return StaffRosterClient::kNone;
    case static_cast<std::uint8_t>(StaffRosterClient::kOriginal):
      return StaffRosterClient::kOriginal;
    case static_cast<std::uint8_t>(StaffRosterClient::kMoonclient):
      return StaffRosterClient::kMoonclient;
    default:
      break;
  }
  return StaffRosterClient::kOther;
}

[[nodiscard]] StaffRosterState StateOf(std::uint32_t wire) {
  const auto has = [wire](std::uint32_t bit) { return (wire & bit) != 0; };
  return StaffRosterState{.vending = has(kStateVending),
                          .buying = has(kStateBuying),
                          .offline_shop = has(kStateOfflineShop),
                          .chat_room = has(kStateChatRoom),
                          .trading = has(kStateTrading),
                          .dead = has(kStateDead),
                          .sitting = has(kStateSitting),
                          .hidden = has(kStateHidden),
                          .away = has(kStateAway),
                          .jailed = has(kStateJailed),
                          .npc_dialog = has(kStateNpcDialog),
                          .storage_open = has(kStateStorageOpen)};
}

[[nodiscard]] std::optional<std::uint32_t> AgeOf(std::uint32_t wire) {
  return wire == kNeverAge ? std::nullopt : std::optional<std::uint32_t>(wire);
}

[[nodiscard]] StaffRosterPlayer PlayerOf(const proto::MoonlightStaffRosterEntry& entry) {
  StaffRosterPlayer player;
  player.account_id = entry.account_id;
  player.char_id = entry.char_id;
  // Le serveur écrit ces textes dans la page de codes du jeu.
  player.name = core::Utf8FromWire(entry.name);
  player.guild = core::Utf8FromWire(entry.guild);
  player.party = core::Utf8FromWire(entry.party);
  player.map = core::Utf8FromWire(entry.map);
  player.instance_id = entry.instance_id;
  player.x = entry.x;
  player.y = entry.y;
  player.job = entry.job;
  player.base_level = entry.base_level;
  player.job_level = entry.job_level;
  player.group_level = entry.group_level;
  player.state = StateOf(entry.state);
  player.online_s = entry.online_s;
  player.character_s = entry.character_s;
  player.idle_s = entry.idle_s;
  player.action_age_s = {AgeOf(entry.walk_age_s),    AgeOf(entry.fight_age_s),
                         AgeOf(entry.item_age_s),    AgeOf(entry.talk_age_s),
                         AgeOf(entry.gesture_age_s), AgeOf(entry.npc_age_s),
                         AgeOf(entry.command_age_s)};
  player.client = ClientOf(entry.client);
  player.tree_modified = (entry.client_flags & kClientFlagTreeModified) != 0;
  player.patch_index = entry.patch_index;
  player.commit = core::Utf8FromWire(entry.commit);
  player.machine_group = entry.machine_group;
  player.account_group = entry.account_group;
  player.cheat_clock = entry.cheat_clock;
  player.cheat_cadence = entry.cheat_cadence;
  player.cheat_client = entry.cheat_client;
  player.mute_minutes = entry.mute_minutes;
  return player;
}

}  // namespace

void StaffRosterAssembly::Expect(std::uint16_t request_id) {
  expected_ = request_id;
  next_page_ = 0;
  pages_ = 0;
  amassed_ = StaffRosterAnswer{};
}

std::optional<StaffRosterAnswer> StaffRosterAssembly::Take(StaffRosterPage page) {
  if (!expected_.has_value() || page.request_id != *expected_) {
    return std::nullopt;
  }
  const bool first = next_page_ == 0;
  if (page.pages == 0 || page.page != next_page_ || (!first && page.pages != pages_)) {
    expected_.reset();
    amassed_ = StaffRosterAnswer{};
    return std::nullopt;
  }
  if (first) {
    pages_ = page.pages;
    amassed_.status = page.status;
  }
  for (StaffRosterPlayer& player : page.players) {
    amassed_.players.push_back(std::move(player));
  }
  ++next_page_;
  if (next_page_ < pages_) {
    return std::nullopt;
  }
  expected_.reset();
  return std::exchange(amassed_, StaffRosterAnswer{});
}

core::Result<void> RequestStaffRoster(Connection& link, std::uint16_t request_id) {
  proto::CzMoonlightReqStaffRoster request;
  // Le serveur lit la taille écrite.
  request.packetLength = static_cast<std::int16_t>(proto::CzMoonlightReqStaffRoster::kWireSize);
  request.request_id = request_id;
  link.Send(proto::Encode(request));
  return {};
}

std::optional<StaffRosterPage> DecodeStaffRoster(std::span<const std::uint8_t> frame) {
  proto::ZcMoonlightStaffRoster packet;
  if (!proto::Decode(frame, packet)) {
    return std::nullopt;
  }
  StaffRosterPage page;
  page.request_id = packet.request_id;
  page.status = StatusOf(packet.status);
  page.page = packet.page;
  page.pages = packet.pages;
  if (page.status != StaffRosterStatus::kServed) {
    return page;
  }
  page.players.reserve(packet.entries.size());
  for (const proto::MoonlightStaffRosterEntry& entry : packet.entries) {
    page.players.push_back(PlayerOf(entry));
  }
  return page;
}

std::optional<StaffRosterPage> ReadStaffRoster(const Frame& frame) {
  if (frame.opcode != proto::ZcMoonlightStaffRoster::kOpcode) {
    return std::nullopt;
  }
  return DecodeStaffRoster(frame.bytes);
}

}  // namespace ro::net
