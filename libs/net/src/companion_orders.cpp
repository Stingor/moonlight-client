// Voir l'en-tête.

#include "ro/net/companion_orders.h"

#include <utility>
#include <vector>

#include "ro/proto/companion_packets.h"
#include "ro/proto/map_packets.h"

namespace ro::net {
namespace {

[[nodiscard]] core::Result<void> Send(Connection& link,
                                      core::Result<std::vector<std::uint8_t>> frame) {
  if (!frame) {
    return std::unexpected(frame.error());
  }
  link.Send(*frame);
  return {};
}

}  // namespace

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
  return Send(link, proto::Encode(proto::CompanionAttackRequest{
                        .companion_id = companion_id, .target_id = target_id, .action = 0}));
}

core::Result<void> RequestCompanionReturn(Connection& link, std::uint32_t companion_id) {
  return Send(link, proto::Encode(proto::CompanionReturnRequest{.companion_id = companion_id}));
}

}  // namespace ro::net
