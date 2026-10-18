#include "ro/proto/user_count_packet.h"

#include <array>
#include <string_view>

#include "ro/core/byte_writer.h"

#include "layout_holds.h"

namespace ro::proto {
namespace {

// ⚠ AUCUN CHAMP : le serveur enregistre ce paquet sous son gestionnaire et sa
// longueur seuls. Le recoupement porte donc sur les deux.
constexpr std::string_view kUserCountHandler = "clif_parse_HowManyConnections";
constexpr std::array<std::uint16_t, 0> kUserCountOffsets = {};

}  // namespace

core::Result<std::vector<std::uint8_t>> EncodeUserCountRequest() {
  if (!FixedLayoutHolds<UserCountRequest>(kUserCountHandler, kUserCountOffsets)) {
    return core::Fail(core::ErrorCode::kUnsupportedVersion,
                      "la disposition écrite pour le compte des joueurs connectés ne correspond "
                      "plus à ce que le serveur déclare lire");
  }
  std::vector<std::uint8_t> frame;
  core::ByteWriter writer(frame);
  writer.U16(UserCountRequest::kOpcode);
  return frame;
}

}  // namespace ro::proto
