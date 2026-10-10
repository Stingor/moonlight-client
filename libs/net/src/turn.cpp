// Tourner sur place. Voir l'en-tête : le serveur ne répond pas à l'envoyeur.

#include "ro/net/turn.h"

#include "ro/proto/manual_packets.h"
#include "ro/sim/world.h"

namespace ro::net {

core::Result<void> TurnPlayer(Connection& link, sim::World& world, sim::Facing facing) {
  proto::TurnRequest request;
  request.direction = facing.direction;
  request.head_direction = facing.head_direction;
  auto frame = proto::Encode(request);
  if (!frame) {
    return std::unexpected(frame.error());
  }
  link.Send(*frame);
  // POSÉ ICI, ET NULLE PART AILLEURS. `clif_changed_dir` diffuse en AREA_WOS :
  // la seule trame que le serveur renverra ira aux autres, jamais à nous.
  world.Turn(world.player_id(), facing.direction, facing.head_direction);
  return {};
}

}  // namespace ro::net
