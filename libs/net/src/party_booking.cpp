// La recherche de groupe. Voir l'en-tête.

#include "ro/net/party_booking.h"

#include <utility>
#include <vector>

#include "ro/core/text.h"
#include "ro/proto/party_booking_packets.h"

namespace ro::net {
namespace {

static_assert(proto::kPartyBookingJobSlots == sim::kPartyBookingJobSlots);
static_assert(proto::kPartyBookingNoJob == sim::kPartyBookingNoJob);

sim::PartyBookingAd AdFrom(const proto::PartyBookingEntry& entry) {
  sim::PartyBookingAd ad;
  ad.index = entry.index;
  ad.leader_name = core::Utf8FromWire(entry.leader_name);
  ad.posted_unix = entry.posted_unix;
  ad.level = entry.level;
  ad.map_code = entry.map_code;
  ad.jobs = entry.jobs;
  return ad;
}

core::Result<void> SendEncoded(Connection& link,
                               const core::Result<std::vector<std::uint8_t>>& frame) {
  if (!frame) {
    return std::unexpected(frame.error());
  }
  link.Send(*frame);
  return {};
}

core::Result<void> RefuseFault(sim::PartyBookingFault fault) {
  switch (fault) {
    case sim::PartyBookingFault::kNone:
      break;
    case sim::PartyBookingFault::kLevelOutOfRange:
      return core::Fail(core::ErrorCode::kInvalidArgument,
                        "annonce de groupe : niveau hors des bornes");
    case sim::PartyBookingFault::kNoJob:
      return core::Fail(core::ErrorCode::kInvalidArgument,
                        "annonce de groupe : aucun métier recherché");
    case sim::PartyBookingFault::kMapAndJob:
      return core::Fail(core::ErrorCode::kInvalidArgument,
                        "recherche de groupe : un lieu et un métier à la fois ne rendent rien");
  }
  return {};
}

core::Result<void> RefuseWithoutAd() {
  return core::Fail(core::ErrorCode::kInvalidArgument, "le joueur n'a pas d'annonce de groupe");
}

}  // namespace

FrameOutcome ApplyPartyBookingFrame(const Frame& frame, sim::PartyBooking& booking,
                                    bool list_shown) {
  switch (frame.opcode) {
    case proto::PartyBookingRegisterAck::kOpcode: {
      const auto ack = proto::DecodePartyBookingRegisterAck(frame.bytes);
      if (!ack) {
        return FrameOutcome::kUndecodable;
      }
      booking.TellRegisterResult(ack->result);
      return FrameOutcome::kApplied;
    }

    case proto::PartyBookingSearchAck::kOpcode: {
      auto ack = proto::DecodePartyBookingSearchAck(frame.bytes);
      if (!ack) {
        return FrameOutcome::kUndecodable;
      }
      std::vector<sim::PartyBookingAd> page;
      page.reserve(ack->entries.size());
      for (const proto::PartyBookingEntry& entry : ack->entries) {
        page.push_back(AdFrom(entry));
      }
      booking.TellSearchResults(ack->more_results, std::move(page));
      return FrameOutcome::kApplied;
    }

    case proto::PartyBookingDeleteAck::kOpcode: {
      const auto ack = proto::DecodePartyBookingDeleteAck(frame.bytes);
      if (!ack) {
        return FrameOutcome::kUndecodable;
      }
      booking.TellDeleteResult(ack->result);
      return FrameOutcome::kApplied;
    }

    case proto::PartyBookingInsertNotice::kOpcode: {
      const auto notice = proto::DecodePartyBookingInsertNotice(frame.bytes);
      if (!notice) {
        return FrameOutcome::kUndecodable;
      }
      booking.TellInserted(AdFrom(notice->entry), list_shown);
      return FrameOutcome::kApplied;
    }

    case proto::PartyBookingUpdateNotice::kOpcode: {
      const auto notice = proto::DecodePartyBookingUpdateNotice(frame.bytes);
      if (!notice) {
        return FrameOutcome::kUndecodable;
      }
      booking.TellUpdated(notice->index, notice->jobs);
      return FrameOutcome::kApplied;
    }

    case proto::PartyBookingDeleteNotice::kOpcode: {
      const auto notice = proto::DecodePartyBookingDeleteNotice(frame.bytes);
      if (!notice) {
        return FrameOutcome::kUndecodable;
      }
      booking.TellDeleted(notice->index);
      return FrameOutcome::kApplied;
    }

    default:
      return FrameOutcome::kUnhandled;
  }
}

core::Result<void> RequestPartyBookingSearch(Connection& link, sim::PartyBooking& booking,
                                             const sim::PartyBookingCriteria& criteria) {
  if (core::Result<void> refused = RefuseFault(sim::CheckPartyBookingSearch(criteria)); !refused) {
    return refused;
  }
  proto::PartyBookingSearchRequest request;
  request.level = criteria.level;
  request.map_code = criteria.map_code;
  request.job = criteria.job;
  request.first_index = 0;
  request.result_count = sim::kPartyBookingPageSize;
  core::Result<void> sent = SendEncoded(link, proto::Encode(request));
  if (sent) {
    booking.NoteSearchSent(criteria);
  }
  return sent;
}

core::Result<void> RequestPartyBookingNextPage(Connection& link, sim::PartyBooking& booking) {
  if (!booking.more_on_server() || !booking.criteria().has_value()) {
    return core::Fail(core::ErrorCode::kInvalidArgument,
                      "le serveur n'annonce pas d'autre page d'annonces de groupe");
  }
  const sim::PartyBookingCriteria criteria = *booking.criteria();
  proto::PartyBookingSearchRequest request;
  request.level = criteria.level;
  request.map_code = criteria.map_code;
  request.job = criteria.job;
  request.first_index = booking.next_index();
  request.result_count = sim::kPartyBookingPageSize;
  core::Result<void> sent = SendEncoded(link, proto::Encode(request));
  if (sent) {
    booking.NoteSearchSent(criteria);
  }
  return sent;
}

core::Result<void> RequestPartyBookingRegister(Connection& link, sim::PartyBooking& booking,
                                               const sim::PartyBookingAd& draft) {
  if (booking.mine().has_value() || booking.registering()) {
    return core::Fail(core::ErrorCode::kInvalidArgument,
                      "le joueur a déjà une annonce de groupe, ou la publie");
  }
  if (core::Result<void> refused =
          RefuseFault(sim::CheckPartyBookingRegister(draft.level, draft.jobs));
      !refused) {
    return refused;
  }
  proto::PartyBookingRegisterRequest request;
  request.level = draft.level;
  request.map_code = draft.map_code;
  request.jobs = draft.jobs;
  core::Result<void> sent = SendEncoded(link, proto::Encode(request));
  if (sent) {
    booking.NoteRegisterSent(draft);
  }
  return sent;
}

core::Result<void> RequestPartyBookingUpdate(Connection& link, const sim::PartyBooking& booking,
                                             const sim::PartyBookingJobs& jobs) {
  if (!booking.mine().has_value()) {
    return RefuseWithoutAd();
  }
  if (core::Result<void> refused = RefuseFault(sim::CheckPartyBookingUpdate(jobs)); !refused) {
    return refused;
  }
  proto::PartyBookingUpdateRequest request;
  request.jobs = jobs;
  return SendEncoded(link, proto::Encode(request));
}

core::Result<void> RequestPartyBookingDelete(Connection& link, const sim::PartyBooking& booking) {
  if (!booking.mine().has_value()) {
    return RefuseWithoutAd();
  }
  return SendEncoded(link, proto::Encode(proto::PartyBookingDeleteRequest{}));
}

}  // namespace ro::net
