// La liste d'Autospell. Voir l'en-tête.

#include "ro/net/spell_list.h"

#include "ro/proto/autospell_packets.h"
#include "ro/proto/map_packets.h"

namespace ro::net {

FrameOutcome ApplySpellListFrame(const Frame& frame, sim::SpellListOffer& offer) {
  if (frame.opcode != proto::AutoSpellList::kOpcode) {
    return FrameOutcome::kUnhandled;
  }
  const auto list = proto::DecodeAutoSpellList(frame.bytes);
  if (!list) {
    return FrameOutcome::kUndecodable;
  }
  offer.Offer(list->skill_ids);
  return offer.open() ? FrameOutcome::kApplied : FrameOutcome::kIgnored;
}

core::Result<void> RequestSpellChoice(Connection& link, std::uint16_t skill_id) {
  proto::CzSelectautospell request;
  request.skill_id = skill_id;
  link.Send(proto::Encode(request));
  return {};
}

}  // namespace ro::net
