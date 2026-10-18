#include "ro/net/movement.h"

#include "ro/proto/manual_packets.h"
#include "ro/proto/map_packets.h"
#include "ro/proto/play_ready_packets.h"

#include "send_encoded.h"

namespace ro::net {

core::Result<void> NotifyMapLoaded(Connection& link) {
  return SendEncoded(link, proto::Encode(proto::MapLoadedNotice()));
}

core::Result<void> AnswerMapProperty(const Frame& frame, Connection& link) {
  if (frame.opcode != proto::PacketMaptypeproperty2::kOpcode) {
    return {};
  }
  return SendEncoded(link, proto::EncodePlayReady());
}

core::Result<void> RequestMove(Connection& link, sim::CellPosition destination) {
  proto::WalkRequest request;
  // La direction n'a pas de sens dans une DESTINATION : le serveur la lit avec
  // les coordonnées parce que les trois tiennent dans les mêmes trois octets,
  // puis l'ignore — c'est le trajet qui décide vers où l'on regarde.
  destination.direction = 0;
  request.packed_destination = sim::PackPosition(destination);

  return SendEncoded(link, proto::Encode(request));
}

}  // namespace ro::net
