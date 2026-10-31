// Le vol libre de l'administrateur. Voir l'en-tête.

#include "ro/net/free_flight.h"

#include "ro/proto/map_packets.h"

namespace ro::net {

core::Result<void> RequestFreeFlightCell(Connection& link, std::int16_t cell_x,
                                         std::int16_t cell_y) {
  proto::CzMoonlightFreeFlight request;
  request.packetLength = static_cast<std::int16_t>(proto::CzMoonlightFreeFlight::kWireSize);
  request.x = cell_x;
  request.y = cell_y;
  link.Send(proto::Encode(request));
  return {};
}

}  // namespace ro::net
