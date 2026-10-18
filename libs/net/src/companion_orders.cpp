// Voir l'en-tête.

#include "ro/net/companion_orders.h"

#include <utility>

#include "ro/proto/companion_packets.h"
#include "ro/proto/map_packets.h"

#include "send_encoded.h"

namespace ro::net {

core::Result<void> RequestCompanionMove(Connection& link, std::uint32_t companion_id,
                                        sim::CellPosition cell) {
  proto::CzRequestMovenpc request;
  request.GID = companion_id;
  request.PosDir = sim::PackPosition(cell);
  link.Send(proto::Encode(request));
  return {};
}

core::Result<void> RequestCompanionAttack(Connection& link, std::uint32_t companion_id,
                                          std::uint32_t target_id) {
  return SendEncoded(link, proto::Encode(proto::CompanionAttackRequest{
                               .companion_id = companion_id, .target_id = target_id, .action = 0}));
}

core::Result<void> RequestCompanionReturn(Connection& link, std::uint32_t companion_id) {
  return SendEncoded(link,
                     proto::Encode(proto::CompanionReturnRequest{.companion_id = companion_id}));
}

}  // namespace ro::net
