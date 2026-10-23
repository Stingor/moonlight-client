// Un trophée de mini-jeu dit au serveur. Voir l'en-tête.

#include "ro/net/minigame_trophy.h"

#include <vector>

#include "ro/proto/map_packets.h"

namespace ro::net {

core::Result<void> SendMinigameTrophy(Connection& link, std::uint8_t trophy) {
  proto::CzMoonlightMinigameTrophy request;
  request.packetLength = static_cast<std::int16_t>(proto::CzMoonlightMinigameTrophy::kWireSize);
  request.trophy = trophy;
  const std::vector<std::uint8_t> bytes = proto::Encode(request);
  if (bytes.empty()) {
    return core::Fail(core::ErrorCode::kInvalidArgument, "trophée de mini-jeu non encodable");
  }
  link.Send(bytes);
  return {};
}

}  // namespace ro::net
