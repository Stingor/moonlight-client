#include "ro/net/movement.h"

#include "ro/proto/manual_packets.h"

namespace ro::net {

core::Result<void> NotifyMapLoaded(Connection& link) {
  auto frame = proto::Encode(proto::MapLoadedNotice());
  if (!frame) {
    return std::unexpected(frame.error());
  }
  link.Send(*frame);
  return {};
}

core::Result<void> RequestMove(Connection& link, sim::CellPosition destination) {
  proto::WalkRequest request;
  // La direction n'a pas de sens dans une DESTINATION : le serveur la lit avec
  // les coordonnées parce que les trois tiennent dans les mêmes trois octets,
  // puis l'ignore — c'est le trajet qui décide vers où l'on regarde.
  destination.direction = 0;
  request.packed_destination = sim::PackPosition(destination);

  auto frame = proto::Encode(request);
  if (!frame) {
    return std::unexpected(frame.error());
  }
  link.Send(*frame);
  return {};
}

}  // namespace ro::net
