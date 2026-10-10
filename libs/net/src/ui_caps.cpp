// L'annonce de ce que ce client sait afficher et lire. Voir l'en-tête.

#include "ro/net/ui_caps.h"

#include <vector>

#include "ro/proto/map_packets.h"

namespace ro::net {

core::Result<void> AnnounceUiCaps(Connection& link, std::uint32_t caps) {
  proto::CzMoonlightUiCaps announce;
  announce.caps = caps;
  const std::vector<std::uint8_t> bytes = proto::Encode(announce);
  if (bytes.empty()) {
    return core::Fail(core::ErrorCode::kInvalidArgument, "annonce des surfaces non encodable");
  }
  link.Send(bytes);
  return {};
}

}  // namespace ro::net
