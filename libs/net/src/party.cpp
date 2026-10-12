#include "ro/net/party.h"

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ro/core/text.h"
#include "ro/net/target_info.h"
#include "ro/proto/map_packets.h"
#include "ro/proto/social_packets.h"

#include "frame_apply.h"

namespace ro::net {
namespace {

using sim::PartyNoticeKind;

// ⚠ LES NOMS SONT DES IDENTIFIANTS DU JEU, en 1252, comme ceux de la guilde.
[[nodiscard]] std::string Name(std::string_view wire) { return core::Utf8FromCp1252(wire); }

// LE NOM DU GROUPE, ses mots interdits masqués quand on tient la liste. Celui
// des membres ne l'est pas.
[[nodiscard]] std::string PartyName(std::string_view wire, const sim::BannedWords* banned) {
  return sim::BannedWords::MaskWith(banned, Name(wire));
}

// Le fil code le CHEF par zéro.
constexpr std::uint32_t kWireLeader = 0;

// Le point qu'un membre efface de la carte en la quittant.
constexpr std::int16_t kNoCell = -1;

// Les motifs d'un départ.
constexpr std::int8_t kWithdrawLeave = 0;
constexpr std::int8_t kWithdrawExpel = 1;
constexpr std::int8_t kWithdrawCantLeave = 2;
constexpr std::int8_t kWithdrawCantExpel = 3;

// Les codes de la réponse à une création, puis à une invitation, et l'avis
// qu'ils donnent. Un code absent n'en donne aucun.
struct Verdict {
  std::int32_t code = 0;
  PartyNoticeKind kind = PartyNoticeKind::kCreated;
};
constexpr std::array<Verdict, 4> kCreationVerdicts = {{
    {0, PartyNoticeKind::kCreated},
    {1, PartyNoticeKind::kNameTaken},
    {2, PartyNoticeKind::kAlreadyInParty},
    {3, PartyNoticeKind::kCreationMapRefused},
}};
constexpr std::array<Verdict, 12> kInviteVerdicts = {{
    {0, PartyNoticeKind::kInviteeInOtherParty},
    {1, PartyNoticeKind::kInviteRefused},
    {2, PartyNoticeKind::kInviteAccepted},
    {3, PartyNoticeKind::kPartyFull},
    {4, PartyNoticeKind::kSameAccountJoined},
    {5, PartyNoticeKind::kInvitationsBlocked},
    {7, PartyNoticeKind::kInviteeNotFound},
    {8, PartyNoticeKind::kInviteeMapRefused},
    {9, PartyNoticeKind::kJoinMapRefused},
    {10, PartyNoticeKind::kMemorialDungeon},
    {11, PartyNoticeKind::kInviteeLevelRefused},
    {18, PartyNoticeKind::kInviteeLacksBasicSkill},
}};

template <std::size_t N>
[[nodiscard]] std::optional<PartyNoticeKind> VerdictOf(const std::array<Verdict, N>& verdicts,
                                                       std::int32_t code) {
  for (const Verdict& verdict : verdicts) {
    if (verdict.code == code) {
      return verdict.kind;
    }
  }
  return std::nullopt;
}

[[nodiscard]] FrameOutcome Notified(sim::PartyRoster& party, std::optional<PartyNoticeKind> kind,
                                    std::string name) {
  if (!kind.has_value()) {
    return FrameOutcome::kIgnored;
  }
  party.Notify(sim::PartyNotice{.kind = *kind, .name = std::move(name)});
  return FrameOutcome::kApplied;
}

[[nodiscard]] FrameOutcome Touched(bool applied) {
  return applied ? FrameOutcome::kApplied : FrameOutcome::kIgnored;
}

FrameOutcome ApplyWithdraw(const proto::ZcDeleteMemberFromGroup& packet, sim::PartyRoster& party,
                           std::uint32_t own_account_id) {
  switch (packet.result) {
    case kWithdrawLeave:
    case kWithdrawExpel: {
      const auto leaving = static_cast<std::uint32_t>(packet.AID);
      if (leaving == own_account_id) {
        party.Clear();
        party.Notify(sim::PartyNotice{.kind = PartyNoticeKind::kYouLeft});
      } else {
        party.Remove(leaving);
      }
      return FrameOutcome::kApplied;
    }
    // Un refus : personne ne part.
    case kWithdrawCantLeave:
      return Notified(party, PartyNoticeKind::kLeaveRefused, {});
    case kWithdrawCantExpel:
      return Notified(party, PartyNoticeKind::kExpelRefused, {});
    default:
      return FrameOutcome::kIgnored;
  }
}

FrameOutcome ApplyOptions(const proto::PartyOptionsNotice& notice, sim::PartyRoster& party) {
  const bool pickup = notice.item_pickup != 0;
  const bool loot = notice.item_share != 0;
  if (notice.exp_share == proto::PartyOptionsNotice::kExpChangeRefused) {
    // L'EXP reste ce qu'elle était ; seules les règles d'objets se redisent.
    party.TellItemOptions(pickup, loot);
    return Notified(party, PartyNoticeKind::kExpShareRefused, {});
  }
  party.TellOptions(sim::PartyOptions{
      .exp_shared = notice.exp_share != 0, .pickup_shared = pickup, .loot_shared = loot});
  return FrameOutcome::kApplied;
}

FrameOutcome ApplyVitals(const proto::ZcMoonlightTargetInfo& packet, sim::PartyRoster& party,
                         std::uint32_t now_ms) {
  if (party.Find(packet.gid) == nullptr) {
    return FrameOutcome::kUnhandled;
  }
  // Hors de vue, ou tu : on OUBLIE ce qu'on savait, plutôt que de montrer un PS
  // figé sans que rien ne le signale.
  if (packet.status != kTargetAnswerOk || (packet.known & kTargetKnowsMagic) == 0) {
    party.ForgetMagic(packet.gid);
    return FrameOutcome::kApplied;
  }
  party.TellMagic(packet.gid,
                  sim::PartyGauge{.current = static_cast<std::int32_t>(packet.sp),
                                  .max = static_cast<std::int32_t>(packet.maxsp)},
                  now_ms);
  if ((packet.known & kTargetKnowsHealth) != 0) {
    party.TellHealth(packet.gid, sim::PartyGauge{.current = static_cast<std::int32_t>(packet.hp),
                                                 .max = static_cast<std::int32_t>(packet.maxhp)});
  }
  return FrameOutcome::kApplied;
}

FrameOutcome ApplyShare(const Frame& frame, sim::PartyRoster& party) {
  const auto state = proto::DecodePartyShareState(frame.bytes);
  if (!state) {
    return FrameOutcome::kUndecodable;
  }
  std::vector<sim::PartyShareRow> rows;
  rows.reserve(state->entries.size());
  for (const proto::PartyShareEntry& entry : state->entries) {
    rows.push_back(sim::PartyShareRow{.account_id = entry.account_id, .flags = entry.flags});
  }
  party.TellShare(state->idle_s, std::move(rows));
  return FrameOutcome::kApplied;
}

FrameOutcome ApplyPartyFrameAt(const Frame& frame, sim::PartyRoster& party,
                               std::uint32_t own_account_id, std::optional<std::uint32_t> now_ms,
                               const sim::BannedWords* banned) {
  switch (frame.opcode) {
    case proto::ZcGroupList::kOpcode:
      return Decoded<proto::ZcGroupList>(frame, [&party, banned](const auto& packet) {
        std::vector<sim::PartyMember> members;
        members.reserve(packet.members.size());
        for (const proto::ZcGroupListSub& member : packet.members) {
          members.push_back(
              sim::PartyMember{.account_id = member.AID,
                               .char_id = member.GID,
                               .name = Name(member.playerName),
                               .map = member.mapName,
                               .leader = member.leader == kWireLeader,
                               .offline = member.offline != 0,
                               .job = static_cast<std::uint16_t>(member.class_),
                               .base_level = static_cast<std::uint16_t>(member.baseLevel)});
        }
        party.Replace(PartyName(packet.partyName, banned), std::move(members));
        return FrameOutcome::kApplied;
      });

    case proto::ZcAddMemberToGroup::kOpcode:
      return Decoded<proto::ZcAddMemberToGroup>(frame, [&party, banned](const auto& packet) {
        sim::PartyMember member{.account_id = packet.AID,
                                .char_id = packet.GID,
                                .name = Name(packet.playerName),
                                .map = packet.mapName,
                                .leader = packet.leader == kWireLeader,
                                .offline = packet.offline != 0,
                                .job = static_cast<std::uint16_t>(packet.class_),
                                .base_level = static_cast<std::uint16_t>(packet.baseLevel)};
        const bool placed = packet.offline == 0;
        const sim::PartyPosition cell{.x = packet.x, .y = packet.y};
        party.TellName(PartyName(packet.partyName, banned));
        party.Upsert(std::move(member));
        if (placed) {
          party.TellPosition(packet.AID, cell);
        }
        party.TellItemOptions(packet.sharePickup != 0, packet.shareLoot != 0);
        return FrameOutcome::kApplied;
      });

    case proto::ZcDeleteMemberFromGroup::kOpcode:
      return Decoded<proto::ZcDeleteMemberFromGroup>(
          frame, [&party, own_account_id](const auto& packet) {
            return ApplyWithdraw(packet, party, own_account_id);
          });

    case proto::PacketPartyLeaderChanged::kOpcode:
      return Decoded<proto::PacketPartyLeaderChanged>(frame, [&party](const auto& packet) {
        if (!party.TellLeader(packet.prev_leader_aid, packet.new_leader_aid)) {
          return FrameOutcome::kIgnored;
        }
        return Notified(party, PartyNoticeKind::kLeaderChanged,
                        party.Find(packet.new_leader_aid)->name);
      });

    case proto::ZcNotifyHpToGroupm::kOpcode:
      return Decoded<proto::ZcNotifyHpToGroupm>(frame, [&party](const auto& packet) {
        return Touched(party.TellHealth(
            packet.AID, sim::PartyGauge{.current = packet.hp, .max = packet.maxhp}));
      });

    case proto::ZcNotifyPositionToGroupm::kOpcode:
      return Decoded<proto::ZcNotifyPositionToGroupm>(frame, [&party](const auto& packet) {
        std::optional<sim::PartyPosition> cell;
        if (packet.xPos != kNoCell || packet.yPos != kNoCell) {
          cell = sim::PartyPosition{.x = packet.xPos, .y = packet.yPos};
        }
        return Touched(party.TellPosition(static_cast<std::uint32_t>(packet.AID), cell));
      });

    case proto::ZcNotifyMemberinfoToGroupm::kOpcode:
      return Decoded<proto::ZcNotifyMemberinfoToGroupm>(frame, [&party](const auto& packet) {
        return Touched(party.TellJobAndLevel(static_cast<std::uint32_t>(packet.AID),
                                             static_cast<std::uint16_t>(packet.job),
                                             static_cast<std::uint16_t>(packet.level)));
      });

    case proto::ZcGroupIsalive::kOpcode:
      return Decoded<proto::ZcGroupIsalive>(frame, [&party](const auto& packet) {
        return Touched(party.TellDead(packet.AID, packet.isDead != 0));
      });

    case proto::ZcAckMakeGroup::kOpcode:
      return Decoded<proto::ZcAckMakeGroup>(frame, [&party](const auto& packet) {
        return Notified(party, VerdictOf(kCreationVerdicts, packet.result), {});
      });

    case proto::ZcPartyJoinReq::kOpcode:
      return Decoded<proto::ZcPartyJoinReq>(frame, [&party, banned](const auto& packet) {
        party.TellInvitation(
            sim::PartyInvitation{.party_id = static_cast<std::uint32_t>(packet.GRID),
                                 .party_name = PartyName(packet.groupName, banned)});
        return FrameOutcome::kApplied;
      });

    case proto::ZcPartyJoinReqAck::kOpcode:
      return Decoded<proto::ZcPartyJoinReqAck>(frame, [&party](const auto& packet) {
        return Notified(party, VerdictOf(kInviteVerdicts, packet.result),
                        Name(packet.characterName));
      });

    case proto::PartyOptionsNotice::kOpcode: {
      const auto notice = proto::DecodePartyOptionsNotice(frame.bytes);
      return notice ? ApplyOptions(*notice, party) : FrameOutcome::kUndecodable;
    }

    case proto::ZcPartyConfig::kOpcode:
      return Decoded<proto::ZcPartyConfig>(frame, [&party](const auto& packet) {
        party.TellRefusesInvitations(packet.denyPartyInvites != 0);
        return FrameOutcome::kApplied;
      });

    case proto::PartyShareState::kOpcode:
      return ApplyShare(frame, party);

    case proto::ZcMoonlightTargetInfo::kOpcode:
      if (!now_ms.has_value()) {
        return FrameOutcome::kUnhandled;
      }
      return Decoded<proto::ZcMoonlightTargetInfo>(frame, [&party, now_ms](const auto& packet) {
        return ApplyVitals(packet, party, *now_ms);
      });

    default:
      return FrameOutcome::kUnhandled;
  }
}

}  // namespace

FrameOutcome ApplyPartyFrame(const Frame& frame, sim::PartyRoster& party,
                             std::uint32_t own_account_id, std::uint32_t now_ms,
                             const sim::BannedWords* banned) {
  return ApplyPartyFrameAt(frame, party, own_account_id, now_ms, banned);
}

FrameOutcome ApplyPartyFrame(const Frame& frame, sim::PartyRoster& party,
                             std::uint32_t own_account_id, std::uint32_t now_ms) {
  return ApplyPartyFrameAt(frame, party, own_account_id, now_ms, nullptr);
}

FrameOutcome ApplyPartyFrame(const Frame& frame, sim::PartyRoster& party,
                             std::uint32_t own_account_id) {
  return ApplyPartyFrameAt(frame, party, own_account_id, std::nullopt, nullptr);
}

}  // namespace ro::net
