// La réforme. Voir l'en-tête pour les trois temps d'une réussite.

#include "ro/net/item_reform.h"

#include <cstdint>

#include "ro/proto/map_packets.h"

namespace ro::net {

FrameOutcome ApplyItemReformFrame(const Frame& frame, sim::ItemReform& reform) {
  switch (frame.opcode) {
    case proto::ZcOpenReformUi::kOpcode: {
      proto::ZcOpenReformUi packet;
      if (!proto::Decode(frame.bytes, packet)) {
        return FrameOutcome::kUndecodable;
      }
      reform.TellOpening(static_cast<std::uint32_t>(packet.ITID));
      return FrameOutcome::kApplied;
    }

    case proto::ZcItemReformAck::kOpcode: {
      proto::ZcItemReformAck packet;
      if (!proto::Decode(frame.bytes, packet)) {
        return FrameOutcome::kUndecodable;
      }
      reform.TellResult(static_cast<std::uint8_t>(packet.result),
                        static_cast<std::uint16_t>(packet.index));
      return FrameOutcome::kApplied;
    }

    case proto::ZcNpcackMapmove::kOpcode:
    case proto::ZcNpcackServermove::kOpcode:
      reform.CloseSilently();
      return FrameOutcome::kUnhandled;

    default:
      return FrameOutcome::kUnhandled;
  }
}

core::Result<void> RequestItemReform(Connection& link, sim::ItemReform& reform,
                                     const sim::ItemReformTrigger& trigger,
                                     const sim::Inventory& bag) {
  if (!sim::CheckItemReformRequest(reform, trigger, bag).passed()) {
    return core::Fail(core::ErrorCode::kInvalidArgument,
                      "demande de réforme que le serveur refuserait");
  }
  link.Send(proto::Encode(
      proto::CzItemReform{.ITID = static_cast<std::int32_t>(reform.trigger_item_id()),
                          .index = static_cast<std::int16_t>(*reform.chosen_slot())}));
  reform.MarkRequestSent();
  return {};
}

void SayItemReformFarewell(Connection& link, sim::ItemReform& reform) {
  if (reform.TakeFarewell()) {
    link.Send(proto::Encode(proto::CzCloseReformUi{}));
  }
}

}  // namespace ro::net
