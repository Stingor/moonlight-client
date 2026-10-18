#include "ro/net/adoption.h"

#include <string>
#include <utility>

#include "ro/core/text.h"
#include "ro/proto/map_packets.h"
#include "ro/proto/social_packets.h"

#include "send_encoded.h"

namespace ro::net {

FrameOutcome ApplyAdoptionFrame(const Frame& frame, sim::AdoptionDesk& desk) {
  switch (frame.opcode) {
    case proto::AdoptionInvitationNotice::kOpcode: {
      const auto notice = proto::DecodeAdoptionInvitationNotice(frame.bytes);
      if (!notice) {
        return FrameOutcome::kUndecodable;
      }
      desk.TellInvitation(
          sim::AdoptionInvitation{.parent_account_id = notice->parent_account_id,
                                  .spouse_account_id = notice->spouse_account_id,
                                  .parent_name = core::Utf8FromCp1252(notice->parent_name)});
      return FrameOutcome::kApplied;
    }
    case proto::AdoptionRefusalNotice::kOpcode: {
      const auto notice = proto::DecodeAdoptionRefusalNotice(frame.bytes);
      if (!notice) {
        return FrameOutcome::kUndecodable;
      }
      if (notice->reason >= sim::kAdoptionRefusalCount) {
        return FrameOutcome::kIgnored;
      }
      desk.TellRefusal(static_cast<sim::AdoptionRefusal>(notice->reason));
      return FrameOutcome::kApplied;
    }
    default:
      return FrameOutcome::kUnhandled;
  }
}

core::Result<void> RequestAdoption(Connection& link, std::uint32_t account_id) {
  return SendEncoded(link, proto::Encode(proto::AdoptionRequest{.account_id = account_id}));
}

core::Result<void> AnswerAdoption(Connection& link, const sim::AdoptionInvitation& invitation,
                                  bool accept) {
  // La réponse que le serveur lit : un pour oui, zéro pour non.
  constexpr std::uint32_t kAccepted = 1;
  constexpr std::uint32_t kRefused = 0;
  proto::CzJoinBaby answer;
  answer.father_AID = invitation.parent_account_id;
  answer.mother_AID = invitation.spouse_account_id;
  answer.accepted = accept ? kAccepted : kRefused;
  link.Send(proto::Encode(answer));
  return {};
}

}  // namespace ro::net
