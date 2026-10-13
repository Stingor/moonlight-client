// « Je suis prêt à jouer ». Voir l'en-tête.

#include "ro/proto/play_ready_packets.h"

#include <optional>
#include <string_view>

#include "ro/core/byte_writer.h"
#include "ro/proto/manual_packets.h"
#include "ro/proto/packet_lengths.h"

namespace ro::proto {
namespace {

constexpr std::string_view kPlayReadyHandler = "clif_parse_blocking_playcancel";

}  // namespace

core::Result<std::vector<std::uint8_t>> EncodePlayReady() {
  const std::optional<std::int16_t> length = PacketLength(kPlayReadyOpcode);
  if (!MatchesServerLayout(kPlayReadyOpcode, kPlayReadyHandler, {}) || !length.has_value() ||
      static_cast<std::size_t>(*length) != kPlayReadyWireSize) {
    return core::Fail(core::ErrorCode::kUnsupportedVersion,
                      "le serveur ne déclare plus CZ_BLOCKING_PLAY_CANCEL tel que le client "
                      "l'écrit");
  }
  std::vector<std::uint8_t> frame;
  frame.reserve(kPlayReadyWireSize);
  core::ByteWriter writer(frame);
  writer.U16(kPlayReadyOpcode);
  return frame;
}

}  // namespace ro::proto
