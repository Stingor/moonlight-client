// La liste des charrettes. Voir l'en-tête : deux chemins l'ouvrent, et
// l'identité qu'elle porte choisit le paquet de la réponse.

#include "ro/net/cart_choice.h"

#include "ro/proto/cart_packets.h"

namespace ro::net {

FrameOutcome ApplyCartChoiceFrame(const Frame& frame, sim::CartChoice& choice) {
  if (frame.opcode != proto::SelectCartOffer::kOpcode) {
    return FrameOutcome::kUnhandled;
  }
  const auto offer = proto::DecodeSelectCartOffer(frame.bytes);
  if (!offer) {
    return FrameOutcome::kUndecodable;
  }
  // ⚠ UNE LISTE VIDE N'OUVRE RIEN, en silence, comme chez le client.
  if (offer->cart_types.empty()) {
    return FrameOutcome::kIgnored;
  }
  choice.OfferFromServer(offer->identity, offer->cart_types);
  return FrameOutcome::kApplied;
}

core::Result<void> RequestCartChoice(Connection& link, std::uint32_t identity,
                                     std::uint8_t cart_type) {
  auto frame = identity == 0 ? proto::Encode(proto::ChangeCartRequest{cart_type})
                             : proto::Encode(proto::SelectCartRequest{identity, cart_type});
  if (!frame) {
    return std::unexpected(frame.error());
  }
  link.Send(*frame);
  return {};
}

}  // namespace ro::net
