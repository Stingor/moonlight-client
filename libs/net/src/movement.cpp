#include "ro/net/movement.h"

#include "ro/proto/manual_packets.h"
#include "ro/proto/map_packets.h"
#include "ro/proto/play_ready_packets.h"

namespace ro::net {

core::Result<void> NotifyMapLoaded(Connection& link) {
  auto frame = proto::Encode(proto::MapLoadedNotice());
  if (!frame) {
    return std::unexpected(frame.error());
  }
  link.Send(*frame);
  return {};
}

core::Result<void> AnswerMapProperty(const Frame& frame, Connection& link) {
  if (frame.opcode != proto::PacketMaptypeproperty2::kOpcode) {
    return {};
  }
  auto ready = proto::EncodePlayReady();
  if (!ready) {
    return std::unexpected(ready.error());
  }
  link.Send(*ready);
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
