#include "ro/net/friends.h"

#include <algorithm>
#include <array>
#include <string>
#include <utility>
#include <vector>

#include "ro/core/text.h"
#include "ro/proto/map_packets.h"
#include "ro/proto/social_packets.h"

#include "frame_apply.h"
#include "send_encoded.h"

namespace ro::net {
namespace {

using sim::FriendNoticeKind;

[[nodiscard]] std::string Name(std::string_view wire) { return core::Utf8FromCp1252(wire); }

constexpr std::uint16_t kBefriended = 0;

// Les codes du sort d'une demande, et l'avis qu'ils donnent. Un code absent
// n'en donne aucun.
struct Verdict {
  std::uint16_t code = 0;
  FriendNoticeKind kind = FriendNoticeKind::kBefriended;
};
constexpr std::array<Verdict, 8> kVerdicts = {{
    {kBefriended, FriendNoticeKind::kBefriended},
    {1, FriendNoticeKind::kDeclined},
    {2, FriendNoticeKind::kOwnListFull},
    {3, FriendNoticeKind::kTheirListFull},
    {4, FriendNoticeKind::kAlreadyRequested},
    {6, FriendNoticeKind::kAlreadyFriends},
    {7, FriendNoticeKind::kNotFound},
    {8, FriendNoticeKind::kFriendOfAnotherCharacter},
}};

FrameOutcome ApplyAddResult(const proto::FriendAddResult& result, sim::FriendList& friends) {
  const auto verdict = std::ranges::find(kVerdicts, result.result, &Verdict::code);
  if (verdict == kVerdicts.end()) {
    return FrameOutcome::kIgnored;
  }
  std::string name = Name(result.name);
  if (result.result == kBefriended) {
    // Il vient de répondre : il est en ligne.
    friends.Add(sim::Friend{
        .account_id = result.account_id, .char_id = result.char_id, .name = name, .online = true});
  }
  friends.Notify(sim::FriendNotice{.kind = verdict->kind, .name = std::move(name)});
  return FrameOutcome::kApplied;
}

}  // namespace

FrameOutcome ApplyFriendFrame(const Frame& frame, sim::FriendList& friends) {
  switch (frame.opcode) {
    case proto::ZcFriendsList::kOpcode:
      return Decoded<proto::ZcFriendsList>(frame, [&friends](const auto& packet) {
        std::vector<sim::Friend> listed;
        listed.reserve(packet.friends.size());
        for (const proto::ZcFriendsListSub& entry : packet.friends) {
          listed.push_back(
              sim::Friend{.account_id = entry.AID, .char_id = entry.CID, .name = Name(entry.name)});
        }
        friends.Replace(std::move(listed));
        return FrameOutcome::kApplied;
      });

    case proto::ZcFriendsState::kOpcode:
      return Decoded<proto::ZcFriendsState>(frame, [&friends](const auto& packet) {
        return friends.TellOnline(packet.AID, packet.CID, packet.offline == 0, Name(packet.name))
                   ? FrameOutcome::kApplied
                   : FrameOutcome::kIgnored;
      });

    case proto::FriendRequestNotice::kOpcode: {
      const auto notice = proto::DecodeFriendRequestNotice(frame.bytes);
      if (!notice) {
        return FrameOutcome::kUndecodable;
      }
      friends.TellInvitation(sim::FriendInvitation{.account_id = notice->account_id,
                                                   .char_id = notice->char_id,
                                                   .name = Name(notice->name)});
      return FrameOutcome::kApplied;
    }

    case proto::FriendAddResult::kOpcode: {
      const auto result = proto::DecodeFriendAddResult(frame.bytes);
      return result ? ApplyAddResult(*result, friends) : FrameOutcome::kUndecodable;
    }

    case proto::FriendRemovedNotice::kOpcode: {
      const auto notice = proto::DecodeFriendRemovedNotice(frame.bytes);
      if (!notice) {
        return FrameOutcome::kUndecodable;
      }
      return friends.Remove(notice->account_id, notice->char_id) ? FrameOutcome::kApplied
                                                                 : FrameOutcome::kIgnored;
    }

    default:
      return FrameOutcome::kUnhandled;
  }
}

core::Result<void> RequestFriend(Connection& link, std::string_view name) {
  proto::FriendAddRequest request;
  request.name = core::Cp1252FromUtf8(name);
  return SendEncoded(link, proto::Encode(request));
}

core::Result<void> AnswerFriendRequest(Connection& link, std::uint32_t account_id,
                                       std::uint32_t char_id, bool accept) {
  proto::FriendAnswerRequest request;
  request.account_id = account_id;
  request.char_id = char_id;
  request.answer =
      accept ? proto::FriendAnswerRequest::kAccepted : proto::FriendAnswerRequest::kRefused;
  return SendEncoded(link, proto::Encode(request));
}

core::Result<void> RequestFriendRemoval(Connection& link, std::uint32_t account_id,
                                        std::uint32_t char_id) {
  proto::FriendRemoveRequest request;
  request.account_id = account_id;
  request.char_id = char_id;
  return SendEncoded(link, proto::Encode(request));
}

}  // namespace ro::net
